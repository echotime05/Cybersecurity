#include "cyber/roles/v/v_auth_service.hpp"

#include <stdexcept>
#include <utility>

namespace cyber::roles::v
{
namespace
{
Packet v_auth_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                     const Bytes& plain, std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

void packet_require_msg_type(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
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

void AuthSessionTable::put_client_public_key(EntityId client_id,
                                             const RsaPublicKey& public_key)
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

AuthRuntime v_auth_make_runtime(const Config& config)
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
    // V_AUTH is the third Kerberos hop: V opens ticket_v with KV, validates the
    // authenticator with Kc_v, and stores the resulting game session key.
    packet_require_msg_type(request, MsgType::v_auth_req);
    const VAuthReq v_req = v_auth_parse_req(request.payload);
    const TicketVBody ticket = v_ticket_decrypt(v_req.ticket_v, config.get_u64("KV"));
    const AuthenticatorBody auth = authenticator_decrypt(v_req.authenticator_v, ticket.kc_v);
    if (ticket.idc != auth.idc || ticket.idv != EntityId::v || request.src != ticket.idc)
    {
        throw std::runtime_error("V identity check failed");
    }
    runtime.v_sessions.put_v_auth(ticket.idc, ticket.adc, ticket.kc_v);
    return v_auth_build_encrypted_packet(MsgType::v_auth_rep, EntityId::v, ticket.idc,
                                         v_auth_build_rep_body({auth.ts + 1U}), ticket.kc_v);
}

Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime)
{
    // Certificate exchange binds the authenticated Client ID to its RSA public
    // key, so later MSG_APP signatures can be verified by V.
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
    return v_auth_build_encrypted_packet(MsgType::cert_v2c, EntityId::v, request.src,
                                         cert_build_v2c_body(response_body), session.kc_v);
}
} // namespace cyber::roles::v
