#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/net_packet.hpp"

namespace cyber::roles::as
{
// 处理一个 Client 到 AS 的认证连接：校验 AS_REQ，生成 Kc_tgs 和 TGT，并返回 AS_REP。
void as_process_connection(SocketHandle socket, const Config& config);
} // namespace cyber::roles::as
