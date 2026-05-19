#pragma once

#include "cyber/shared/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
std::uint64_t auth_derive_client_key(EntityId client_id, const std::string& password);
} // namespace cyber
