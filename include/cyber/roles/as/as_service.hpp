#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/net_packet.hpp"

namespace cyber::roles::as
{
void as_process_connection(SocketHandle socket, const Config& config);
} // namespace cyber::roles::as
