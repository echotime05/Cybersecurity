#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_payloads.hpp"

namespace cyber::roles::client
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

struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config,
                                                     EntityId client_id,
                                                     std::uint64_t kc);
} // namespace cyber::roles::client
