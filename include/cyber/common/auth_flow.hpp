#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_payloads.hpp"
#include "cyber/common/role_runtime.hpp"

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

// Handoff from Kerberos to the tank game layer. The authenticated socket keeps
// the open V connection plus Kc_v and public keys needed by encrypted signed
// MSG_APP traffic.
struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc);

void auth_handle_packet(RoleKind role, SocketHandle socket, const Packet& request,
                        const Config& config);
} // namespace cyber
