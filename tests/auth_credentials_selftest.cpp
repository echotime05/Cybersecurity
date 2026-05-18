#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/types.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        require(cyber::derive_client_key(cyber::EntityId::client1, "123456") ==
                    0x0059EF3DB7CB8C8DULL,
                "Client1 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client2, "admin123") ==
                    0x006A73A4EBE9C564ULL,
                "Client2 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client3, "hehe12345") ==
                    0x0057EF9D5F45AB7BULL,
                "Client3 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client4, "&wxh@147") ==
                    0x00EC3EB766D59086ULL,
                "Client4 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client1, "wrong") !=
                    0x0059EF3DB7CB8C8DULL,
                "wrong password should produce a different key");
        std::cout << "auth_credentials_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_credentials_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
