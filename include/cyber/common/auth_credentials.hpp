#pragma once

#include "cyber/common/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
std::uint64_t derive_client_key(EntityId client_id, const std::string& password);
std::string default_client_name(EntityId client_id);
} // namespace cyber
