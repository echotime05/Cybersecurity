#pragma once

#include "cyber/shared/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
// 根据 Client ID 和用户输入密码派生 Kerberos 第一阶段使用的客户端长期密钥 Kc。
std::uint64_t auth_derive_client_key(EntityId client_id, const std::string& password);
} // namespace cyber
