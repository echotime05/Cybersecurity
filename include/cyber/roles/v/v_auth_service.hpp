#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/protocol_payloads.hpp"

#include <cstdint>
#include <map>
#include <mutex>

namespace cyber::roles::v
{
struct AuthSession
{
    EntityId client_id = EntityId::unknown;
    std::uint32_t adc = 0;
    std::uint64_t kc_v = 0;
    RsaPublicKey client_public_key;
    bool v_auth_done = false;
    bool cert_done = false;
};

class AuthSessionTable
{
public:
    AuthSessionTable() = default;
    AuthSessionTable(const AuthSessionTable&) = delete;
    AuthSessionTable& operator=(const AuthSessionTable&) = delete;
    AuthSessionTable(AuthSessionTable&& other) noexcept;
    AuthSessionTable& operator=(AuthSessionTable&& other) noexcept;

    void put_v_auth(EntityId client_id, std::uint32_t adc, std::uint64_t kc_v);
    AuthSession get(EntityId client_id) const;
    void put_client_public_key(EntityId client_id, const RsaPublicKey& public_key);

private:
    mutable std::mutex mutex_;
    std::map<EntityId, AuthSession> sessions_;
};

struct AuthRuntime
{
    RsaKeyPair ca_key_pair;
    RsaKeyPair v_key_pair;
    AuthSessionTable v_sessions;
};

AuthRuntime v_auth_make_runtime(const Config& config);
Packet v_auth_process_request(const Packet& request, const Config& config,
                              AuthRuntime& runtime);
Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime);
} // namespace cyber::roles::v
