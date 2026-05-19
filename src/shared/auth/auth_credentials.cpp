#include "cyber/shared/auth_credentials.hpp"

#include "cyber/shared/crypto.hpp"

#include <stdexcept>

namespace cyber
{
std::uint64_t auth_derive_client_key(EntityId client_id, const std::string& password)
{
    if (!is_client(client_id))
    {
        throw std::runtime_error("auth_derive_client_key requires a client id");
    }

    const std::string material =
        "client-kc-v1:" + std::to_string(static_cast<int>(client_id)) + ":" + password;
    const Bytes bytes(material.begin(), material.end());
    std::uint64_t key = hash64(bytes) & 0x00FFFFFFFFFFFFFFULL;
    if (key == 0)
    {
        key = 0x0001010101010101ULL;
    }
    return key;
}

} // namespace cyber
