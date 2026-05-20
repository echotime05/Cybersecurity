#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/crypto.hpp"
#include "cyber/shared/net_socket.hpp"
#include "cyber/protocol/certificate_messages.hpp"
#include "cyber/protocol/kerberos_messages.hpp"

namespace cyber::roles::client
{
// Client 认证流程运行状态，保存 AS/TGS/V 三阶段产出的票据、会话密钥和证书材料。
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

// 认证成功后返回的 V socket 和对应状态，游戏客户端继续复用这条连接。
struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

// 串行完成 AS、TGS、V_AUTH 和证书交换，返回已认证的 V 长连接。
VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config,
                                                     EntityId client_id,
                                                     std::uint64_t kc);
} // namespace cyber::roles::client
