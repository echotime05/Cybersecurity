#include "cyber/common/auth_flow.hpp"

#include "cyber/common/net_packet.hpp"
#include "cyber/common/protocol_event.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace cyber
{
namespace
{
constexpr std::uint32_t kDefaultAdc = 0x7F000001U;
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

std::uint64_t auth_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

TcpEndpoint config_build_endpoint(const Config& config, const char* ip_key, const char* port_key)
{
    return {config.get_string(ip_key), config.get_u16(port_key)};
}

ClientSecret auth_find_client_secret(const Config& config, EntityId id)
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

Packet packet_build_encrypted(MsgType type, EntityId src, EntityId dst, const Bytes& plain,
                              std::uint64_t key)
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

void auth_send_error_packet(SocketHandle socket, EntityId src, EntityId dst,
                            const std::string& message)
{
    const Packet packet =
        make_packet(MsgType::error, src, dst,
                    make_error_payload(ErrorCode::unsupported_msg_type, message));
    send_packet_logged(socket, packet);
}

void as_process_packet(SocketHandle socket, const Packet& request, const Config& config)
{
    packet_require_msg_type(request, MsgType::as_req);
    const AsReq as_req = as_parse_req(request.payload);
    const ClientSecret secret = auth_find_client_secret(config, as_req.idc);
    const std::uint64_t kc_tgs = generate_des_key56();
    const std::uint64_t ts2 = auth_time_now_ms();
    const TicketTgsBody ticket_body{
        kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
    const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
    const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
    const Bytes rep_plain = as_build_rep_body(rep_body);
    const Packet response =
        packet_build_encrypted(MsgType::as_rep, EntityId::as, as_req.idc, rep_plain, secret.kc);
    ProtocolPayloadView view = protocol_build_encrypted_payload_view(rep_plain, response.payload);
    protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                                 tgs_ticket_build_body(ticket_body));
    send_packet_logged(socket, response, view);
}

void tgs_process_packet(SocketHandle socket, const Packet& request, const Config& config)
{
    packet_require_msg_type(request, MsgType::tgs_req);
    const TgsReq tgs_req = tgs_parse_req(request.payload);
    const TicketTgsBody ticket = tgs_ticket_decrypt(tgs_req.ticket_tgs, config.get_u64("KTGS"));
    const AuthenticatorBody auth =
        authenticator_decrypt(tgs_req.authenticator_tgs, ticket.kc_tgs);
    if (ticket.idc != auth.idc || ticket.idtgs != EntityId::tgs || tgs_req.idv != EntityId::v)
    {
        throw std::runtime_error("TGS identity check failed");
    }
    ProtocolPayloadView request_view;
    protocol_add_encrypted_field(request_view, "ticket_tgs", tgs_req.ticket_tgs,
                                 tgs_ticket_build_body(ticket));
    protocol_add_encrypted_field(request_view, "authenticator_tgs", tgs_req.authenticator_tgs,
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
        packet_build_encrypted(MsgType::tgs_rep, EntityId::tgs, ticket.idc, rep_plain,
                               ticket.kc_tgs);
    ProtocolPayloadView view = protocol_build_encrypted_payload_view(rep_plain, response.payload);
    protocol_add_encrypted_field(view, "ticket_v", ticket_v, v_ticket_build_body(ticket_v_body));
    send_packet_logged(socket, response, view);
}

Packet auth_exchange_packet(const TcpEndpoint& endpoint, const Packet& request,
                            const ProtocolPayloadView& request_payload_view = {})
{
    SocketHandle socket = connect_tcp(endpoint);
    try
    {
        send_packet_logged(socket, request, request_payload_view);
        Packet response = recv_packet_logged(socket);
        close_socket(socket);
        return response;
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
} // namespace

AuthSessionTable::AuthSessionTable(AuthSessionTable&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    sessions_ = std::move(other.sessions_);
}

AuthSessionTable& AuthSessionTable::operator=(AuthSessionTable&& other) noexcept
{
    if (this != &other)
    {
        std::scoped_lock lock(mutex_, other.mutex_);
        sessions_ = std::move(other.sessions_);
    }
    return *this;
}

void AuthSessionTable::put_v_auth(EntityId client_id, std::uint32_t adc, std::uint64_t kc_v)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AuthSession& session = sessions_[client_id];
    session.client_id = client_id;
    session.adc = adc;
    session.kc_v = kc_v;
    session.v_auth_done = true;
}

AuthSession AuthSessionTable::get(EntityId client_id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = sessions_.find(client_id);
    if (it == sessions_.end())
    {
        throw std::runtime_error("missing auth session");
    }
    return it->second;
}

void AuthSessionTable::put_client_public_key(EntityId client_id, const RsaPublicKey& public_key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AuthSession& session = sessions_[client_id];
    if (!session.v_auth_done)
    {
        throw std::runtime_error("cannot store cert before V_AUTH");
    }
    session.client_public_key = public_key;
    session.cert_done = true;
}

AuthRuntime auth_make_runtime(const Config& config)
{
    AuthRuntime runtime;
    runtime.ca_key_pair =
        ca_key_pair_from_hex(config.get_string("PK_CA_N"), config.get_string("PK_CA_E"),
                             config.get_string("SK_CA_D"));
    runtime.v_key_pair = demo_rsa_key_pair_for(EntityId::v);
    return runtime;
}

Packet v_auth_process_request(const Packet& request, const Config& config, AuthRuntime& runtime)
{
    packet_require_msg_type(request, MsgType::v_auth_req);
    const VAuthReq v_req = v_auth_parse_req(request.payload);
    const TicketVBody ticket = v_ticket_decrypt(v_req.ticket_v, config.get_u64("KV"));
    const AuthenticatorBody auth = authenticator_decrypt(v_req.authenticator_v, ticket.kc_v);
    if (ticket.idc != auth.idc || ticket.idv != EntityId::v || request.src != ticket.idc)
    {
        throw std::runtime_error("V identity check failed");
    }
    runtime.v_sessions.put_v_auth(ticket.idc, ticket.adc, ticket.kc_v);
    return packet_build_encrypted(MsgType::v_auth_rep, EntityId::v, ticket.idc,
                                  v_auth_build_rep_body({auth.ts + 1U}), ticket.kc_v);
}

Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime)
{
    packet_require_msg_type(request, MsgType::cert_c2v);
    if (!is_client(request.src))
    {
        throw std::runtime_error("CERT_C2V source must be a client");
    }

    const AuthSession session = runtime.v_sessions.get(request.src);
    const CertC2VBody body =
        cert_parse_c2v_body(des_decrypt_payload(request.payload, session.kc_v));
    if (body.client_id != request.src)
    {
        throw std::runtime_error("CERT_C2V client id mismatch");
    }
    const Certificate client_cert = parse_certificate(body.cert);
    if (!verify_certificate(client_cert, runtime.ca_key_pair.public_key) ||
        client_cert.subject_id != request.src)
    {
        throw std::runtime_error("client certificate verification failed");
    }
    runtime.v_sessions.put_client_public_key(request.src, client_cert.subject_pk);

    const Certificate v_cert = make_certificate(EntityId::v, runtime.v_key_pair.public_key,
                                                runtime.ca_key_pair.private_key);
    const CertV2CBody response_body{EntityId::v, serialize_certificate(v_cert)};
    return packet_build_encrypted(MsgType::cert_v2c, EntityId::v, request.src,
                                  cert_build_v2c_body(response_body), session.kc_v);
}

VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc)
{
    AuthClientState state;
    state.client_id = client_id;
    state.adc = kDefaultAdc;
    state.kc = kc;
    state.client_key_pair = demo_rsa_key_pair_for(client_id);
    const AuthRuntime cert_runtime = auth_make_runtime(config);

    const std::uint64_t ts1 = auth_time_now_ms();
    const Packet as_req =
        make_packet(MsgType::as_req, state.client_id, EntityId::as,
                    as_build_req({state.client_id, EntityId::tgs, ts1}));
    const Packet as_rep = auth_exchange_packet(
        config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
    packet_require_msg_type(as_rep, MsgType::as_rep);
    const Bytes as_rep_plain = des_decrypt_payload(as_rep.payload, state.kc);
    const AsRepBody as_body = as_parse_rep_body(as_rep_plain);
    ProtocolPayloadView as_rep_view =
        protocol_build_encrypted_payload_view(as_rep_plain, as_rep.payload);
    protocol_add_encrypted_field(as_rep_view, "ticket_tgs", as_body.ticket_tgs);
    write_protocol_event(ProtocolDirection::recv, as_rep, {}, as_rep_view);
    state.kc_tgs = as_body.kc_tgs;
    state.ticket_tgs = as_body.ticket_tgs;

    const AuthenticatorBody auth_tgs{state.client_id, state.adc, auth_time_now_ms()};
    const Bytes authenticator_tgs = authenticator_encrypt(auth_tgs, state.kc_tgs);
    const TgsReq tgs_req_body{EntityId::v, state.ticket_tgs,
                              authenticator_tgs};
    const Packet tgs_req =
        make_packet(MsgType::tgs_req, state.client_id, EntityId::tgs,
                    tgs_build_req(tgs_req_body));
    ProtocolPayloadView tgs_req_view;
    protocol_add_encrypted_field(tgs_req_view, "ticket_tgs", state.ticket_tgs);
    protocol_add_encrypted_field(tgs_req_view, "authenticator_tgs", authenticator_tgs,
                                 authenticator_build_body(auth_tgs));
    const Packet tgs_rep = auth_exchange_packet(
        config_build_endpoint(config, "TGS_IP", "TGS_PORT"), tgs_req,
        tgs_req_view);
    packet_require_msg_type(tgs_rep, MsgType::tgs_rep);
    const Bytes tgs_rep_plain = des_decrypt_payload(tgs_rep.payload, state.kc_tgs);
    const TgsRepBody tgs_body = tgs_parse_rep_body(tgs_rep_plain);
    ProtocolPayloadView tgs_rep_view =
        protocol_build_encrypted_payload_view(tgs_rep_plain, tgs_rep.payload);
    protocol_add_encrypted_field(tgs_rep_view, "ticket_v", tgs_body.ticket_v);
    write_protocol_event(ProtocolDirection::recv, tgs_rep, {}, tgs_rep_view);
    state.kc_v = tgs_body.kc_v;
    state.ticket_v = tgs_body.ticket_v;

    const TcpEndpoint v = config_build_endpoint(config, "V_IP", "V_PORT");
    SocketHandle socket = connect_tcp(v);
    try
    {
        const std::uint64_t ts5 = auth_time_now_ms();
        const AuthenticatorBody auth_v{state.client_id, state.adc, ts5};
        const Bytes authenticator_v = authenticator_encrypt(auth_v, state.kc_v);
        const VAuthReq v_req_body{state.ticket_v, authenticator_v};
        const Packet v_req = make_packet(MsgType::v_auth_req, state.client_id, EntityId::v,
                                         v_auth_build_req(v_req_body));
        ProtocolPayloadView v_req_view;
        protocol_add_encrypted_field(v_req_view, "ticket_v", state.ticket_v);
        protocol_add_encrypted_field(v_req_view, "authenticator_v", authenticator_v,
                                     authenticator_build_body(auth_v));
        send_packet_logged(socket, v_req, v_req_view);
        const Packet v_rep = recv_packet_logged(socket);
        packet_require_msg_type(v_rep, MsgType::v_auth_rep);
        const Bytes v_rep_plain = des_decrypt_payload(v_rep.payload, state.kc_v);
        const VAuthRepBody v_body = v_auth_parse_rep_body(v_rep_plain);
        write_protocol_event(
            ProtocolDirection::recv, v_rep, {},
            protocol_build_encrypted_payload_view(v_rep_plain, v_rep.payload));
        if (v_body.ts5_plus_1 != ts5 + 1U)
        {
            throw std::runtime_error("V_AUTH TS5+1 check failed");
        }
        const Certificate client_cert =
            make_certificate(state.client_id, state.client_key_pair.public_key,
                             cert_runtime.ca_key_pair.private_key);
        const CertC2VBody cert_body{state.client_id, serialize_certificate(client_cert)};
        const Bytes cert_plain = cert_build_c2v_body(cert_body);
        const Packet cert_req =
            packet_build_encrypted(MsgType::cert_c2v, state.client_id, EntityId::v,
                                   cert_plain, state.kc_v);
        send_packet_logged(socket, cert_req,
                           protocol_build_encrypted_payload_view(cert_plain, cert_req.payload));
        const Packet cert_rep = recv_packet_logged(socket);
        packet_require_msg_type(cert_rep, MsgType::cert_v2c);
        const Bytes cert_rep_plain = des_decrypt_payload(cert_rep.payload, state.kc_v);
        const CertV2CBody cert_v = cert_parse_v2c_body(cert_rep_plain);
        write_protocol_event(
            ProtocolDirection::recv, cert_rep, {},
            protocol_build_encrypted_payload_view(cert_rep_plain, cert_rep.payload));
        const Certificate v_cert = parse_certificate(cert_v.cert);
        if (cert_v.v_id != EntityId::v ||
            !verify_certificate(v_cert, cert_runtime.ca_key_pair.public_key) ||
            v_cert.subject_id != EntityId::v)
        {
            throw std::runtime_error("V certificate verification failed");
        }
        state.v_public_key = v_cert.subject_pk;
        return {state, socket};
    }
    catch (const std::exception&)
    {
        close_socket(socket);
        throw;
    }
}

void auth_handle_packet(RoleKind role, SocketHandle socket, const Packet& request,
                        const Config& config)
{
    try
    {
        if (role == RoleKind::as_server)
        {
            as_process_packet(socket, request, config);
        }
        else if (role == RoleKind::tgs_server)
        {
            tgs_process_packet(socket, request, config);
        }
        else
        {
            auth_send_error_packet(socket,
                                   role == RoleKind::as_server ? EntityId::as : EntityId::tgs,
                                   request.src, "unsupported auth message");
        }
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}

} // namespace cyber
