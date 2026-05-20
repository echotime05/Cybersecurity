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
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

// 返回当前认证时间戳，单位为毫秒。
std::uint64_t auth_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// 构造 TGS 发出的加密报文，payload 使用指定会话密钥 DES 加密。
Packet tgs_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
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

// 处理 TGS 连接，完成 TGS_REQ 到 TGS_REP 的认证第二阶段。
void tgs_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
        // TGS 是 Kerberos 第二跳：用 KTGS 解开 ticket_tgs，用 Kc_tgs
        // 验证认证器，然后签发 Kc_v 和 ticket_v。
        const Packet request = recv_packet_logged(socket);
        packet_require_msg_type(request, MsgType::tgs_req);

        const TgsReq tgs_req = tgs_parse_req(request.payload);
        const TicketTgsBody ticket =
            tgs_ticket_decrypt(tgs_req.ticket_tgs, config.get_u64("KTGS"));
        const AuthenticatorBody auth =
            authenticator_decrypt(tgs_req.authenticator_tgs, ticket.kc_tgs);
        if (ticket.idc != auth.idc || ticket.idtgs != EntityId::tgs ||
            tgs_req.idv != EntityId::v)
        {
            throw std::runtime_error("TGS identity check failed");
        }

        ProtocolPayloadView request_view;
        protocol_add_encrypted_field(request_view, "ticket_tgs", tgs_req.ticket_tgs,
                                     tgs_ticket_build_body(ticket));
        protocol_add_encrypted_field(request_view, "authenticator_tgs",
                                     tgs_req.authenticator_tgs,
                                     authenticator_build_body(auth));
        write_protocol_event(ProtocolDirection::recv, request, {}, request_view);

        const std::uint64_t kc_v = generate_des_key56();
        const std::uint64_t ts4 = auth_time_now_ms();
        const TicketVBody ticket_v_body{
            kc_v, ticket.idc, ticket.adc, EntityId::v, ts4, kDefaultLifetimeMs};
        const Bytes ticket_v = v_ticket_encrypt(ticket_v_body, config.get_u64("KV"));
        const TgsRepBody rep_body{kc_v, EntityId::v, ts4, ticket_v};
        const Bytes rep_plain = tgs_build_rep_body(rep_body);
        const Packet response =
            tgs_build_encrypted_packet(MsgType::tgs_rep, EntityId::tgs, ticket.idc,
                                       rep_plain, ticket.kc_tgs);

        ProtocolPayloadView view =
            protocol_build_encrypted_payload_view(rep_plain, response.payload);
        protocol_add_encrypted_field(view, "ticket_v", ticket_v,
                                     v_ticket_build_body(ticket_v_body));
        send_packet_logged(socket, response, view);
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
} // namespace cyber::roles::tgs
