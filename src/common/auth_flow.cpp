#include "cyber/common/auth_flow.hpp"

#include "cyber/common/net_packet.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cyber
{
namespace
{
constexpr std::uint32_t kDefaultAdc = 0x7F000001U;
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

std::uint64_t now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::string endpoint_text(const TcpEndpoint& endpoint)
{
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

TcpEndpoint connect_endpoint(const Config& config, const char* ip_key, const char* port_key)
{
    return {config.get_string(ip_key), config.get_u16(port_key)};
}

ClientSecret client_secret_for(const Config& config, EntityId id)
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

std::string entity_log_name(EntityId id)
{
    return std::string(to_string(id));
}

void log_and_print(Logger& logger, const std::string& event, const std::string& message)
{
    logger.write("Client", "CAuthWorker", event, message);
    std::cout << event << ' ' << message << '\n';
}

Packet encrypted_packet(MsgType type, EntityId src, EntityId dst, const Bytes& plain,
                        std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

void ensure_msg(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
    }
}

void send_error(SocketHandle socket, Logger& logger, const std::string& thread_name,
                EntityId src, EntityId dst, const std::string& message)
{
    const Packet packet =
        make_packet(MsgType::error, src, dst,
                    make_error_payload(ErrorCode::unsupported_msg_type, message));
    send_packet_logged(socket, packet, logger, entity_log_name(src), thread_name);
}

void handle_as_packet(SocketHandle socket, const Packet& request, const Config& config,
                      Logger& logger, const std::string& thread_name)
{
    ensure_msg(request, MsgType::as_req);
    const AsReq as_req = parse_as_req(request.payload);
    const ClientSecret secret = client_secret_for(config, as_req.idc);
    const std::uint64_t kc_tgs = generate_des_key56();
    const std::uint64_t ts2 = now_ms();
    const TicketTgsBody ticket_body{
        kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
    const Bytes ticket_tgs = encrypt_ticket_tgs(ticket_body, config.get_u64("KTGS"));
    const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
    const Packet response =
        encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc, build_as_rep_body(rep_body),
                         secret.kc);
    logger.write("AS", thread_name, "AUTH_STATE",
                 "AS_REP_SENT client=" + entity_log_name(as_req.idc));
    send_packet_logged(socket, response, logger, "AS", thread_name);
}

void handle_tgs_packet(SocketHandle socket, const Packet& request, const Config& config,
                       Logger& logger, const std::string& thread_name)
{
    ensure_msg(request, MsgType::tgs_req);
    const TgsReq tgs_req = parse_tgs_req(request.payload);
    const TicketTgsBody ticket = decrypt_ticket_tgs(tgs_req.ticket_tgs, config.get_u64("KTGS"));
    const AuthenticatorBody auth =
        decrypt_authenticator(tgs_req.authenticator_tgs, ticket.kc_tgs);
    if (ticket.idc != auth.idc || ticket.idtgs != EntityId::tgs || tgs_req.idv != EntityId::v)
    {
        throw std::runtime_error("TGS identity check failed");
    }

    const std::uint64_t kc_v = generate_des_key56();
    const std::uint64_t ts4 = now_ms();
    const TicketVBody ticket_v_body{
        kc_v, ticket.idc, ticket.adc, EntityId::v, ts4, kDefaultLifetimeMs};
    const Bytes ticket_v = encrypt_ticket_v(ticket_v_body, config.get_u64("KV"));
    const TgsRepBody rep_body{kc_v, EntityId::v, ts4, ticket_v};
    const Packet response =
        encrypted_packet(MsgType::tgs_rep, EntityId::tgs, ticket.idc,
                         build_tgs_rep_body(rep_body), ticket.kc_tgs);
    logger.write("TGS", thread_name, "AUTH_STATE",
                 "TGS_REP_SENT client=" + entity_log_name(ticket.idc));
    send_packet_logged(socket, response, logger, "TGS", thread_name);
}

void handle_v_auth_packet(SocketHandle socket, const Packet& request, const Config& config,
                          AuthRuntime& runtime, Logger& logger, const std::string& thread_name)
{
    const Packet response = process_v_auth_request(request, config, runtime, logger, thread_name);
    send_packet_logged(socket, response, logger, "V", thread_name);
}

void handle_cert_packet(SocketHandle socket, const Packet& request, AuthRuntime& runtime,
                        Logger& logger, const std::string& thread_name)
{
    const AuthSession session = runtime.v_sessions.get(request.src);
    const CertC2VBody body =
        parse_cert_c2v_body(des_decrypt_payload(request.payload, session.kc_v));
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

    const Certificate v_cert =
        make_certificate(EntityId::v, runtime.v_key_pair.public_key, runtime.ca_key_pair.private_key);
    const CertV2CBody response_body{EntityId::v, serialize_certificate(v_cert)};
    const Packet response = encrypted_packet(MsgType::cert_v2c, EntityId::v, request.src,
                                             build_cert_v2c_body(response_body), session.kc_v);
    logger.write("V", thread_name, "AUTH_STATE",
                 "CERT_V2C_SENT client=" + entity_log_name(request.src));
    send_packet_logged(socket, response, logger, "V", thread_name);
}

void handle_app_packet(SocketHandle socket, const Packet& request, AuthRuntime& runtime,
                       Logger& logger, const std::string& thread_name)
{
    const AuthSession session = runtime.v_sessions.get(request.src);
    if (!session.cert_done)
    {
        throw std::runtime_error("client has not completed certificate exchange");
    }
    const Bytes decrypted = des_decrypt_payload(request.payload, session.kc_v);
    const SignedAppPayload signed_join = parse_signed_app_payload(decrypted);
    if (signed_join.app_code != AppCode::game_join_req || signed_join.app_payload.size() != 1U ||
        static_cast<EntityId>(signed_join.app_payload[0]) != request.src)
    {
        throw std::runtime_error("invalid GAME_JOIN_REQ");
    }
    if (!verify_signed_app_payload(signed_join, session.client_public_key))
    {
        throw std::runtime_error("GAME_JOIN_REQ signature verification failed");
    }
    logger.write("V", thread_name, "APP_NON_REPUDIATION", "GAME_JOIN_REQ_VERIFIED");

    const AppAckPayload ack{MsgType::app, AppCode::game_join_req, request.src, request.dst,
                            static_cast<std::uint32_t>(request.payload.size()),
                            hash64(request.payload)};
    const Bytes signed_ack =
        build_signed_app_payload(AppCode::app_ack, build_app_ack_payload(ack),
                                 runtime.v_key_pair.private_key);
    const Packet response = make_packet(MsgType::app, EntityId::v, request.src,
                                        des_encrypt_payload(signed_ack, session.kc_v));
    logger.write("V", thread_name, "APP_NON_REPUDIATION", "APP_ACK_SIGNED");
    send_packet_logged(socket, response, logger, "V", thread_name);
}

Packet request_response(const TcpEndpoint& endpoint, const Packet& request, Logger& logger,
                        const std::string& thread_name)
{
    logger.write("Client", thread_name, "CONNECT", "connect to " + endpoint_text(endpoint));
    SocketHandle socket = connect_tcp(endpoint);
    try
    {
        send_packet_logged(socket, request, logger, "Client", thread_name);
        Packet response = recv_packet_logged(socket, logger, "Client", thread_name);
        logger.write("Client", thread_name, "SOCKET_CLOSE", "close " + endpoint_text(endpoint));
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

AuthRuntime make_auth_runtime(const Config& config)
{
    AuthRuntime runtime;
    runtime.ca_key_pair =
        ca_key_pair_from_hex(config.get_string("PK_CA_N"), config.get_string("PK_CA_E"),
                             config.get_string("SK_CA_D"));
    runtime.v_key_pair = demo_rsa_key_pair_for(EntityId::v);
    return runtime;
}

Packet process_v_auth_request(const Packet& request, const Config& config, AuthRuntime& runtime,
                              Logger& logger, const std::string& thread_name)
{
    ensure_msg(request, MsgType::v_auth_req);
    const VAuthReq v_req = parse_v_auth_req(request.payload);
    const TicketVBody ticket = decrypt_ticket_v(v_req.ticket_v, config.get_u64("KV"));
    const AuthenticatorBody auth = decrypt_authenticator(v_req.authenticator_v, ticket.kc_v);
    if (ticket.idc != auth.idc || ticket.idv != EntityId::v || request.src != ticket.idc)
    {
        throw std::runtime_error("V identity check failed");
    }
    runtime.v_sessions.put_v_auth(ticket.idc, ticket.adc, ticket.kc_v);
    logger.write("V", thread_name, "AUTH_STATE",
                 "V_AUTH_REP_SENT client=" + entity_log_name(ticket.idc));
    return encrypted_packet(MsgType::v_auth_rep, EntityId::v, ticket.idc,
                            build_v_auth_rep_body({auth.ts + 1U}), ticket.kc_v);
}

VAuthenticatedSocket authenticate_client_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc, Logger& logger,
                                                     const std::string& thread_name)
{
    AuthClientState state;
    state.client_id = client_id;
    state.adc = kDefaultAdc;
    state.kc = kc;
    state.client_key_pair = demo_rsa_key_pair_for(client_id);

    const std::uint64_t ts1 = now_ms();
    const Packet as_req =
        make_packet(MsgType::as_req, state.client_id, EntityId::as,
                    build_as_req({state.client_id, EntityId::tgs, ts1}));
    const Packet as_rep = request_response(connect_endpoint(config, "AS_IP", "AS_PORT"), as_req,
                                           logger, thread_name);
    ensure_msg(as_rep, MsgType::as_rep);
    const AsRepBody as_body = parse_as_rep_body(des_decrypt_payload(as_rep.payload, state.kc));
    state.kc_tgs = as_body.kc_tgs;
    state.ticket_tgs = as_body.ticket_tgs;
    logger.write("Client", thread_name, "AUTH_STATE", "AS_OK");

    const AuthenticatorBody auth_tgs{state.client_id, state.adc, now_ms()};
    const TgsReq tgs_req_body{EntityId::v, state.ticket_tgs,
                              encrypt_authenticator(auth_tgs, state.kc_tgs)};
    const Packet tgs_req =
        make_packet(MsgType::tgs_req, state.client_id, EntityId::tgs,
                    build_tgs_req(tgs_req_body));
    const Packet tgs_rep = request_response(connect_endpoint(config, "TGS_IP", "TGS_PORT"),
                                            tgs_req, logger, thread_name);
    ensure_msg(tgs_rep, MsgType::tgs_rep);
    const TgsRepBody tgs_body =
        parse_tgs_rep_body(des_decrypt_payload(tgs_rep.payload, state.kc_tgs));
    state.kc_v = tgs_body.kc_v;
    state.ticket_v = tgs_body.ticket_v;
    logger.write("Client", thread_name, "AUTH_STATE", "TGS_OK");

    const TcpEndpoint v = connect_endpoint(config, "V_IP", "V_PORT");
    logger.write("Client", thread_name, "CONNECT", "connect to V " + endpoint_text(v));
    SocketHandle socket = connect_tcp(v);
    try
    {
        const std::uint64_t ts5 = now_ms();
        const AuthenticatorBody auth_v{state.client_id, state.adc, ts5};
        const VAuthReq v_req_body{state.ticket_v, encrypt_authenticator(auth_v, state.kc_v)};
        const Packet v_req = make_packet(MsgType::v_auth_req, state.client_id, EntityId::v,
                                         build_v_auth_req(v_req_body));
        send_packet_logged(socket, v_req, logger, "Client", thread_name);
        const Packet v_rep = recv_packet_logged(socket, logger, "Client", thread_name);
        ensure_msg(v_rep, MsgType::v_auth_rep);
        const VAuthRepBody v_body =
            parse_v_auth_rep_body(des_decrypt_payload(v_rep.payload, state.kc_v));
        if (v_body.ts5_plus_1 != ts5 + 1U)
        {
            throw std::runtime_error("V_AUTH TS5+1 check failed");
        }
        logger.write("Client", thread_name, "AUTH_STATE", "V_AUTH_OK");
        return {state, socket};
    }
    catch (const std::exception&)
    {
        close_socket(socket);
        throw;
    }
}

void handle_auth_packet(RoleKind role, SocketHandle socket, const Packet& request,
                        const Config& config, AuthRuntime& runtime, Logger& logger,
                        const std::string& thread_name)
{
    try
    {
        if (role == RoleKind::as_server)
        {
            handle_as_packet(socket, request, config, logger, thread_name);
        }
        else if (role == RoleKind::tgs_server)
        {
            handle_tgs_packet(socket, request, config, logger, thread_name);
        }
        else if (role == RoleKind::v_server && request.msg_type == MsgType::v_auth_req)
        {
            handle_v_auth_packet(socket, request, config, runtime, logger, thread_name);
        }
        else if (role == RoleKind::v_server && request.msg_type == MsgType::cert_c2v)
        {
            handle_cert_packet(socket, request, runtime, logger, thread_name);
        }
        else if (role == RoleKind::v_server && request.msg_type == MsgType::app)
        {
            handle_app_packet(socket, request, runtime, logger, thread_name);
        }
        else
        {
            send_error(socket, logger, thread_name,
                       role == RoleKind::as_server
                           ? EntityId::as
                           : (role == RoleKind::tgs_server ? EntityId::tgs : EntityId::v),
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

void run_client_auth_test(const Config& config, Logger& logger)
{
    SocketRuntime socket_runtime;
    AuthRuntime auth_runtime = make_auth_runtime(config);
    AuthClientState state;
    state.client_id = config.get_entity_id("LOCAL_CLIENT_ID");
    state.adc = kDefaultAdc;
    state.kc = client_secret_for(config, state.client_id).kc;
    state.client_key_pair = demo_rsa_key_pair_for(state.client_id);
    const std::string thread_name = "CAuthWorker";

    logger.write("Client", thread_name, "THREAD_START", "auth-test start");

    const std::uint64_t ts1 = now_ms();
    const Packet as_req =
        make_packet(MsgType::as_req, state.client_id, EntityId::as,
                    build_as_req({state.client_id, EntityId::tgs, ts1}));
    const Packet as_rep = request_response(connect_endpoint(config, "AS_IP", "AS_PORT"), as_req,
                                           logger, thread_name);
    ensure_msg(as_rep, MsgType::as_rep);
    const AsRepBody as_body = parse_as_rep_body(des_decrypt_payload(as_rep.payload, state.kc));
    state.kc_tgs = as_body.kc_tgs;
    state.ticket_tgs = as_body.ticket_tgs;
    log_and_print(logger, "AUTH_STATE", "AS_OK");

    const AuthenticatorBody auth_tgs{state.client_id, state.adc, now_ms()};
    const TgsReq tgs_req_body{EntityId::v, state.ticket_tgs,
                              encrypt_authenticator(auth_tgs, state.kc_tgs)};
    const Packet tgs_req =
        make_packet(MsgType::tgs_req, state.client_id, EntityId::tgs,
                    build_tgs_req(tgs_req_body));
    const Packet tgs_rep = request_response(connect_endpoint(config, "TGS_IP", "TGS_PORT"),
                                            tgs_req, logger, thread_name);
    ensure_msg(tgs_rep, MsgType::tgs_rep);
    const TgsRepBody tgs_body =
        parse_tgs_rep_body(des_decrypt_payload(tgs_rep.payload, state.kc_tgs));
    state.kc_v = tgs_body.kc_v;
    state.ticket_v = tgs_body.ticket_v;
    log_and_print(logger, "AUTH_STATE", "TGS_OK");

    const std::uint64_t ts5 = now_ms();
    const AuthenticatorBody auth_v{state.client_id, state.adc, ts5};
    const VAuthReq v_req_body{state.ticket_v, encrypt_authenticator(auth_v, state.kc_v)};
    const Packet v_req = make_packet(MsgType::v_auth_req, state.client_id, EntityId::v,
                                     build_v_auth_req(v_req_body));
    const Packet v_rep = request_response(connect_endpoint(config, "V_IP", "V_PORT"), v_req,
                                          logger, thread_name);
    ensure_msg(v_rep, MsgType::v_auth_rep);
    const VAuthRepBody v_body =
        parse_v_auth_rep_body(des_decrypt_payload(v_rep.payload, state.kc_v));
    if (v_body.ts5_plus_1 != ts5 + 1U)
    {
        throw std::runtime_error("V_AUTH TS5+1 check failed");
    }
    log_and_print(logger, "AUTH_STATE", "V_AUTH_OK");

    const Certificate client_cert =
        make_certificate(state.client_id, state.client_key_pair.public_key,
                         auth_runtime.ca_key_pair.private_key);
    const CertC2VBody cert_body{state.client_id, serialize_certificate(client_cert)};
    const Packet cert_req =
        encrypted_packet(MsgType::cert_c2v, state.client_id, EntityId::v,
                         build_cert_c2v_body(cert_body), state.kc_v);
    const Packet cert_rep = request_response(connect_endpoint(config, "V_IP", "V_PORT"), cert_req,
                                             logger, thread_name);
    ensure_msg(cert_rep, MsgType::cert_v2c);
    const CertV2CBody cert_v =
        parse_cert_v2c_body(des_decrypt_payload(cert_rep.payload, state.kc_v));
    const Certificate v_cert = parse_certificate(cert_v.cert);
    if (cert_v.v_id != EntityId::v ||
        !verify_certificate(v_cert, auth_runtime.ca_key_pair.public_key) ||
        v_cert.subject_id != EntityId::v)
    {
        throw std::runtime_error("V certificate verification failed");
    }
    state.v_public_key = v_cert.subject_pk;
    log_and_print(logger, "AUTH_STATE", "AUTH_DONE");

    const Bytes join_payload{static_cast<std::uint8_t>(state.client_id)};
    const Bytes signed_join =
        build_signed_app_payload(AppCode::game_join_req, join_payload,
                                 state.client_key_pair.private_key);
    const Packet join_req = make_packet(MsgType::app, state.client_id, EntityId::v,
                                        des_encrypt_payload(signed_join, state.kc_v));
    logger.write("Client", thread_name, "APP_NON_REPUDIATION", "GAME_JOIN_REQ_SIGNED");
    std::cout << "APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED\n";
    const Packet ack_packet = request_response(connect_endpoint(config, "V_IP", "V_PORT"),
                                               join_req, logger, thread_name);
    ensure_msg(ack_packet, MsgType::app);
    const SignedAppPayload signed_ack =
        parse_signed_app_payload(des_decrypt_payload(ack_packet.payload, state.kc_v));
    if (signed_ack.app_code != AppCode::app_ack ||
        !verify_signed_app_payload(signed_ack, state.v_public_key))
    {
        throw std::runtime_error("APP_ACK signature verification failed");
    }
    const AppAckPayload ack = parse_app_ack_payload(signed_ack.app_payload);
    if (ack.acked_msg_type != MsgType::app || ack.acked_app_code != AppCode::game_join_req ||
        ack.acked_src != state.client_id || ack.acked_dst != EntityId::v ||
        ack.acked_payload_len != join_req.payload.size() ||
        ack.acked_payload_hash != hash64(join_req.payload))
    {
        throw std::runtime_error("APP_ACK correlation check failed");
    }
    logger.write("Client", thread_name, "APP_NON_REPUDIATION", "APP_ACK_VERIFIED");
    std::cout << "APP_NON_REPUDIATION APP_ACK_VERIFIED\n";

    logger.write("Client", thread_name, "THREAD_EXIT", "auth-test exit");
    std::cout << "auth-test: ok\n";
    std::cout << "log file: " << logger.path().string() << '\n';
}
} // namespace cyber
