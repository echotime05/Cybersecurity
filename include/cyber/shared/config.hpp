#pragma once

#include "cyber/shared/types.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cyber
{
struct ClientSecret
{
    EntityId id = EntityId::unknown;
    std::string password;
    std::uint64_t kc = 0;
};

class Config
{
public:
    static Config load(const std::filesystem::path& path);

    bool has(const std::string& key) const;
    std::string get_string(const std::string& key) const;
    std::uint64_t get_u64(const std::string& key) const;
    std::uint16_t get_u16(const std::string& key) const;
    EntityId get_entity_id(const std::string& key) const;

    std::vector<ClientSecret> clients() const;

private:
    std::map<std::string, std::string> values_;
};
} // namespace cyber
