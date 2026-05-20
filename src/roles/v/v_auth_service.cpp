#include "cyber/roles/v/v_auth_service.hpp"

#include <stdexcept>
#include <utility>

namespace cyber::roles::v
{
namespace
{
// 构造 V 认证阶段发出的加密报文。
Packet v_auth_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                     const Bytes& plain, std::uint64_t key)
{
    return make_packet(type, src, dst, des_encrypt_payload(plain, key));
}

// 要求收到的报文类型符合预期，不符合则终止当前认证流程。
void packet_require_msg_type(const Packet& packet, MsgType expected)
{
    if (packet.msg_type != expected)
    {
        throw std::runtime_error("unexpected message type");
    }
}
} // namespace

// 移动构造会话表，保证内部 mutex 不被复制。
AuthSessionTable::AuthSessionTable(AuthSessionTable&& other) noexcept
{
    std::lock_guard<std::mutex> lock(other.mutex_);
    sessions_ = std::move(other.sessions_);
}

// 移动赋值会话表，带锁转移全部认证会话。
AuthSessionTable& AuthSessionTable::operator=(AuthSessionTable&& other) noexcept
{
    if (this != &other)
    {
        std::scoped_lock lock(mutex_, other.mutex_);
        sessions_ = std::move(other.sessions_);
    }
    return *this;
}

// 保存 V_AUTH 阶段得到的 Client 会话密钥和地址码。
void AuthSessionTable::put_v_auth(EntityId client_id, std::uint32_t adc, std::uint64_t kc_v)
{
    std::lock_guard<std::mutex> lock(mutex_);
    AuthSession& session = sessions_[client_id];
    session.client_id = client_id;
    session.adc = adc;
    session.kc_v = kc_v;
    session.v_auth_done = true;
}

// 按 Client ID 读取认证会话，不存在时抛出异常。
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

// 保存证书交换阶段得到的 Client 公钥。
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

// 根据配置初始化 V 认证运行时，包括 CA 密钥和 V 自身密钥。
AuthRuntime v_auth_make_runtime(const Config& config)
{
    AuthRuntime runtime;
    runtime.ca_key_pair =
        ca_key_pair_from_hex(config.get_string("PK_CA_N"), config.get_string("PK_CA_E"),
                             config.get_string("SK_CA_D"));
    runtime.v_key_pair = demo_rsa_key_pair_for(EntityId::v);
    return runtime;
}

// 处理 V_AUTH_REQ：解 ticket_v/认证器，校验身份后保存 Kc_v。
Packet v_auth_process_request(const Packet& request, const Config& config, AuthRuntime& runtime)
{
    // V_AUTH 是 Kerberos 第三跳：V 用 KV 打开 ticket_v，用 Kc_v
    // 验证认证器，并保存后续游戏会话密钥。
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

// 处理 CERT_C2V：校验 Client 证书并返回 V 证书，完成游戏签名公钥交换。
Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime)
{
    // 证书交换把已认证 Client ID 与 RSA 公钥绑定，后续 V 依靠它验证 MSG_APP 签名。
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
