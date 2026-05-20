#include "cyber/roles/as/as_service.hpp"

#include "cyber/shared/auth_credentials.hpp"
#include "cyber/shared/crypto.hpp"
#include "cyber/protocol/protocol_event.hpp"
#include "cyber/protocol/kerberos_messages.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace cyber::roles::as
{
namespace
{
constexpr std::uint32_t kDefaultAdc = 0x7F000001U;
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

// 返回当前认证时间戳，单位为毫秒。
std::uint64_t auth_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// 根据 Client ID 在配置中查找对应口令派生密钥。
ClientSecret as_find_client_secret(const Config& config, EntityId id)
{
    for (const ClientSecret& secret : config.clients())
    {
        if (secret.id == id)
        {
            return secret;
        }
    }
    throw std::runtime_error("missing client secret for id");
}

// 构造 AS 发出的加密报文，payload 使用指定密钥 DES 加密。
Packet as_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                 const Bytes& plain, std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

// 构造协议可视化用的 payload 明文/密文对照。
ProtocolPayloadView protocol_build_encrypted_payload_view(const Bytes& plain,
                                                          const Bytes& encrypted)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(plain);
    view.encrypted_hex = bytes_to_hex(encrypted);
    return view;
}

// 向协议可视化 payload 中追加一个字段级密文/明文对照。
void protocol_add_encrypted_field(ProtocolPayloadView& view, std::string name,
                                  const Bytes& encrypted, const Bytes& plain = {})
{
    ProtocolPayloadView::Field field;
    field.name = std::move(name);
    field.plain_hex = bytes_to_hex(plain);
    field.encrypted_hex = bytes_to_hex(encrypted);
    view.fields.push_back(std::move(field));
}

// 要求收到的报文类型符合预期，不符合则终止当前连接处理。
void packet_require_msg_type(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
    }
}
} // namespace

// 处理 AS 连接，完成 AS_REQ 到 AS_REP 的认证第一阶段。
void as_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
        // AS 是 Kerberos 第一跳：确认 Client 身份，生成 Kc_tgs，并返回
        // Client 可解的 AS_REP 和 TGS 可解的 ticket_tgs。
        const Packet request = recv_packet_logged(socket);
        packet_require_msg_type(request, MsgType::as_req);

        const AsReq as_req = as_parse_req(request.payload);
        const ClientSecret secret = as_find_client_secret(config, as_req.idc);
        const std::uint64_t kc_tgs = generate_des_key56();
        const std::uint64_t ts2 = auth_time_now_ms();
        const TicketTgsBody ticket_body{
            kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
        const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
        const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
        const Bytes rep_plain = as_build_rep_body(rep_body);
        const Packet response =
            as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                                      rep_plain, secret.kc);

        ProtocolPayloadView view =
            protocol_build_encrypted_payload_view(rep_plain, response.payload);
        protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                                     tgs_ticket_build_body(ticket_body));
        send_packet_logged(socket, response, view);
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
} // namespace cyber::roles::as
