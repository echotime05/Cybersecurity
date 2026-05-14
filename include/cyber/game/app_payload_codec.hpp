#pragma once

#include "cyber/common/packet.hpp"

#include <cstdint>

namespace cyber::game
{
Bytes encode_app_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted);
Bytes decode_app_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted);
} // namespace cyber::game
