#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_payloads.hpp"
#include "cyber/common/role_runtime.hpp"

#include <map>
#include <mutex>
#include <string>

namespace cyber
{
struct AuthClientState
{
    EntityId client_id = EntityId::unknown;
    std::uint32_t adc = 0x7F000001U;
    std::uint64_t kc = 0;
    std::uint64_t kc_tgs = 0;
    std::uint64_t kc_v = 0;
    Bytes ticket_tgs;
    Bytes ticket_v;
    RsaKeyPair client_key_pair;
    RsaPublicKey v_public_key;
};

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

// Handoff from Kerberos to the tank game layer. The authenticated socket keeps
// the open V connection plus Kc_v and public keys needed by encrypted signed
// MSG_APP traffic.
struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

AuthRuntime make_auth_runtime(const Config& config);

Packet process_v_auth_request(const Packet& request, const Config& config, AuthRuntime& runtime,
                              Logger& logger, const std::string& thread_name);

Packet process_cert_c2v_request(const Packet& request, AuthRuntime& runtime, Logger& logger,
                                const std::string& thread_name);

VAuthenticatedSocket authenticate_client_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc, Logger& logger,
                                                     const std::string& thread_name);

void handle_auth_packet(RoleKind role, SocketHandle socket, const Packet& request,
                        const Config& config, Logger& logger, const std::string& thread_name);
} // namespace cyber
