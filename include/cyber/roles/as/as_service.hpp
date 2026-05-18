#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"

namespace cyber::roles::as
{
void as_process_connection(SocketHandle socket, const Config& config);
} // namespace cyber::roles::as
