#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/crypto.hpp"
#include "cyber/shared/net_packet.hpp"
#include "cyber/protocol/certificate_messages.hpp"
#include "cyber/protocol/kerberos_messages.hpp"

#include <cstdint>
#include <map>
#include <mutex>

namespace cyber::roles::v
{
// V 端保存的单个 Client 认证会话，后续游戏报文解密和验签依赖这里的 Kc_v/公钥。
struct AuthSession
{
    EntityId client_id = EntityId::unknown;
    std::uint32_t adc = 0;
    std::uint64_t kc_v = 0;
    RsaPublicKey client_public_key;
    bool v_auth_done = false;
    bool cert_done = false;
};

// V 端认证会话表，负责把 V_AUTH 和证书交换得到的信息按 Client ID 合并。
class AuthSessionTable
{
public:
    AuthSessionTable() = default;
    AuthSessionTable(const AuthSessionTable&) = delete;
    AuthSessionTable& operator=(const AuthSessionTable&) = delete;
    AuthSessionTable(AuthSessionTable&& other) noexcept;
    AuthSessionTable& operator=(AuthSessionTable&& other) noexcept;

    // 记录通过 Kerberos V_AUTH 得到的 Client 会话密钥和地址码。
    void put_v_auth(EntityId client_id, std::uint32_t adc, std::uint64_t kc_v);
    // 读取指定 Client 的认证会话快照。
    AuthSession get(EntityId client_id) const;
    // 记录证书交换后得到的 Client 公钥。
    void put_client_public_key(EntityId client_id, const RsaPublicKey& public_key);

private:
    mutable std::mutex mutex_;
    std::map<EntityId, AuthSession> sessions_;
};

// V 认证模块运行时状态，包含 CA/V 密钥和全部 Client 会话。
struct AuthRuntime
{
    RsaKeyPair ca_key_pair;
    RsaKeyPair v_key_pair;
    AuthSessionTable v_sessions;
};

// 根据配置构造 V 认证运行时，初始化 CA 密钥和 V 自身密钥。
AuthRuntime v_auth_make_runtime(const Config& config);
// 处理 Client 发来的 V_AUTH_REQ，成功时写入会话并返回 V_AUTH_REP。
Packet v_auth_process_request(const Packet& request, const Config& config,
                              AuthRuntime& runtime);
// 处理 Client 发来的证书消息，验证 Client 证书并返回 V 证书。
Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime);
} // namespace cyber::roles::v
