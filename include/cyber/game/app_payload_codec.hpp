#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>

namespace cyber::game
{
Bytes app_encode_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted);
Bytes app_decode_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted);
} // namespace cyber::game
