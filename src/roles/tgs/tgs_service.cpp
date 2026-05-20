#include "cyber/roles/tgs/tgs_service.hpp"

#include "cyber/shared/crypto.hpp"
#include "cyber/protocol/protocol_event.hpp"
#include "cyber/protocol/kerberos_messages.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace cyber::roles::tgs
{
namespace
{

// 默认票据生命周期：5分钟
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

// 获取当前系统的毫米级的时间戳，用于kerberos防重放攻击的时间戳验证
std::uint64_t auth_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// 辅助函数，构建一个加密的网络数据包，并用DES算法提供的密钥对明文（plain）进行加密，生成密文（cipher），最后将密文作为数据包的有效载荷（payload）发送出去。
Packet tgs_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                  const Bytes& plain, std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

/**
 * 将明文和密文转为16进制字符串，并构建一个ProtocolPayloadView对象来表示这些数据，以便在协议事件中记录和分析。这个函数的主要作用是将原始的二进制数据转换为更易读的格式，并将其组织成一个结构化的视图，以便后续处理和展示。
 */
ProtocolPayloadView protocol_build_encrypted_payload_view(const Bytes& plain,
                                                          const Bytes& encrypted)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(plain);
    view.encrypted_hex = bytes_to_hex(encrypted);
    return view;
}

/**
 * 辅助函数:向写一监控视图中添加嵌套的加密字段，如票据内部结构。
 */
void protocol_add_encrypted_field(ProtocolPayloadView& view, std::string name,
                                  const Bytes& encrypted, const Bytes& plain = {})
{
    ProtocolPayloadView::Field field;
    field.name = std::move(name);
    field.plain_hex = bytes_to_hex(plain);
    field.encrypted_hex = bytes_to_hex(encrypted);
    view.fields.push_back(std::move(field));
}

/**
 * 校验接收到的数据包是否是预期的消息类型，如果不是，则抛出一个运行时错误异常，提示“unexpected message type”。这个函数的主要作用是确保接收到的数据包符合预期的协议规范，以防止处理错误类型的数据包导致程序异常或安全问题。
 */
void packet_require_msg_type(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
    }
}
} // namespace

/**
 * 处理客户端连接的函数，负责接收客户端发送的TGS请求，解析请求内容，验证客户端身份，并生成相应的TGS响应。具体步骤包括：接收数据包、解析请求、验证票据和认证器、构建响应数据包、记录协议事件以及发送响应数据包。整个过程中还包含错误处理机制，以确保在发生异常时能够正确关闭连接并抛出异常信息。
 */
void tgs_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
        // 1. 接收数据包并校验类型必须是TGS_REQ
        const Packet request = recv_packet_logged(socket);
        packet_require_msg_type(request, MsgType::tgs_req);

        // 解析TGS_REQ请求，提取票据和认证器，并进行身份验证
        const TgsReq tgs_req = tgs_parse_req(request.payload);

        // 2. 使用TGS长期密钥（KTGS）解密TGT票据，获取其中的会话密钥（kc_tgs）和客户端身份信息（idc）。然后使用会话密钥解密认证器，获取客户端的身份信息（idc）和时间戳（ts）。最后验证票据中的客户端身份、TGS身份以及请求中的服务身份是否符合预期。如果验证失败，则抛出一个运行时错误异常，提示“TGS identity check failed”。
        const TicketTgsBody ticket =
            tgs_ticket_decrypt(tgs_req.ticket_tgs, config.get_u64("KTGS"));

        // 3. 使用票据中提取出来的会话密钥（Kc_tgs)解密客户端的Authernticator
        // Authenticator证明了客户端确实拥有Kc_tgs
        const AuthenticatorBody auth =
            authenticator_decrypt(tgs_req.authenticator_tgs, ticket.kc_tgs);


        /*
         * 4. 身份一致性和防篡改校验
            票据中的客户端ID（idc）必须等于 Authenticator 中的客户端 ID
            票据的目标必须是TGS（idtgs）
            客户端请求的目标服务ID（idv）必须是合法的服务V
         */
        if (ticket.idc != auth.idc || ticket.idtgs != EntityId::tgs ||
            tgs_req.idv != EntityId::v)
        {
            throw std::runtime_error("TGS identity check failed");
        }


        // 记录日志到协议监控界面
        ProtocolPayloadView request_view;
        protocol_add_encrypted_field(request_view, "ticket_tgs", tgs_req.ticket_tgs,
                                     tgs_ticket_build_body(ticket));
        protocol_add_encrypted_field(request_view, "authenticator_tgs",
                                     tgs_req.authenticator_tgs,
                                     authenticator_build_body(auth));
        write_protocol_event(ProtocolDirection::recv, request, {}, request_view);

        // 5. 准备签发新的服务票据（Service Ticket）并构建TGS响应数据包
        // 生成客户端（C）与最终服务（V）之间的临时会话密钥Kc_v
        const std::uint64_t kc_v = generate_des_key56();
        const std::uint64_t ts4 = auth_time_now_ms();//生成时间戳

        // 使用服务V的长期密钥（KV）加密票据，客户端无法解密，只能转法给V
        const TicketVBody ticket_v_body{
            kc_v, ticket.idc, ticket.adc, EntityId::v, ts4, kDefaultLifetimeMs};

        // 6. 组装发给客户端的回复体（TGS_REP）
        // 包含：C-V 会话密钥 (Kc_v)、目标服务ID、时间戳以及加密好的服务票据 (ticket_v)
        const Bytes ticket_v = v_ticket_encrypt(ticket_v_body, config.get_u64("KV"));
        const TgsRepBody rep_body{kc_v, EntityId::v, ts4, ticket_v};
        const Bytes rep_plain = tgs_build_rep_body(rep_body);

        // 使用第一步获得的 C-TGS 会话密钥 (Kc_tgs) 加密整个回复包，确保只有合法的客户端能解开
        const Packet response =
            tgs_build_encrypted_packet(MsgType::tgs_rep, EntityId::tgs, ticket.idc,
                                       rep_plain, ticket.kc_tgs);

        // --- 记录发送日志到协议监控界面 ---
        ProtocolPayloadView view =
            protocol_build_encrypted_payload_view(rep_plain, response.payload);
        protocol_add_encrypted_field(view, "ticket_v", ticket_v,
                                     v_ticket_build_body(ticket_v_body));
        send_packet_logged(socket, response, view);

        // 7. 处理完毕，关闭连接
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
} // namespace cyber::roles::tgs
