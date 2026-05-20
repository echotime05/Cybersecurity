#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/net_packet.hpp"

namespace cyber::roles::tgs
{
// 处理一个 Client 到 TGS 的认证连接：解 TGT/认证器，生成 Kc_v 和 V ticket，并返回 TGS_REP。
void tgs_process_connection(SocketHandle socket, const Config& config);
} // namespace cyber::roles::tgs
