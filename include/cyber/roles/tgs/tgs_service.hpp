#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"

namespace cyber::roles::tgs
{
void tgs_process_connection(SocketHandle socket, const Config& config);
} // namespace cyber::roles::tgs
