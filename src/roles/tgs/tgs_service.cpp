#include "cyber/roles/tgs/tgs_service.hpp"

#include "cyber/common/crypto.hpp"
#include "cyber/common/protocol_event.hpp"
#include "cyber/common/protocol_payloads.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace cyber::roles::tgs
{
namespace
{
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

std::uint64_t auth_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

Packet tgs_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                  const Bytes& plain, std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

ProtocolPayloadView protocol_build_encrypted_payload_view(const Bytes& plain,
                                                          const Bytes& encrypted)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(plain);
    view.encrypted_hex = bytes_to_hex(encrypted);
    return view;
}

void protocol_add_encrypted_field(ProtocolPayloadView& view, std::string name,
                                  const Bytes& encrypted, const Bytes& plain = {})
{
    ProtocolPayloadView::Field field;
    field.name = std::move(name);
    field.plain_hex = bytes_to_hex(plain);
    field.encrypted_hex = bytes_to_hex(encrypted);
    view.fields.push_back(std::move(field));
}

void packet_require_msg_type(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
    }
}
} // namespace

void tgs_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
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
