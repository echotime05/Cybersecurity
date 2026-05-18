#include "cyber/common/config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cyber
{
namespace
{
std::string trim(std::string value)
{
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
                    return !is_space(static_cast<unsigned char>(ch));
                }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
                    return !is_space(static_cast<unsigned char>(ch));
                }).base(),
                value.end());
    return value;
}

std::uint64_t parse_u64(const std::string& value)
{
    std::string text = trim(value);
    int base = 10;
    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0)
    {
        base = 16;
        text.erase(0, 2);
    }

    std::uint64_t result = 0;
    std::istringstream iss(text);
    if (base == 16)
    {
        iss >> std::hex >> result;
    }
    else
    {
        iss >> result;
    }

    if (!iss || !iss.eof())
    {
        throw std::runtime_error("invalid integer value: " + value);
    }
    return result;
}
} // namespace

Config Config::load(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("failed to open config: " + path.string());
    }

    Config config;
    std::string line;
    int line_no = 0;
    while (std::getline(in, line))
    {
        ++line_no;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos)
        {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty())
        {
            continue;
        }

        std::size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            pos = line.find(':');
        }
        if (pos == std::string::npos)
        {
            throw std::runtime_error("invalid config line " + std::to_string(line_no));
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        if (key.empty())
        {
            throw std::runtime_error("empty config key at line " + std::to_string(line_no));
        }
        config.values_[key] = value;
    }
    return config;
}

bool Config::has(const std::string& key) const
{
    return values_.find(key) != values_.end();
}

std::string Config::get_string(const std::string& key) const
{
    const auto it = values_.find(key);
    if (it == values_.end())
    {
        throw std::runtime_error("missing config key: " + key);
    }
    return it->second;
}

std::uint64_t Config::get_u64(const std::string& key) const
{
    return parse_u64(get_string(key));
}

std::uint16_t Config::get_u16(const std::string& key) const
{
    const std::uint64_t value = get_u64(key);
    if (value > 0xFFFFU)
    {
        throw std::runtime_error("config value is too large for uint16: " + key);
    }
    return static_cast<std::uint16_t>(value);
}

EntityId Config::get_entity_id(const std::string& key) const
{
    const std::uint64_t value = get_u64(key);
    if (value > 0xFFU)
    {
        throw std::runtime_error("entity id is too large: " + key);
    }
    return static_cast<EntityId>(value);
}

std::vector<ClientSecret> Config::clients() const
{
    std::vector<ClientSecret> out;
    for (int i = 1; i <= 4; ++i)
    {
        const std::string prefix = "C" + std::to_string(i);
        ClientSecret secret;
        secret.id = get_entity_id(prefix + "_ID");
        secret.password = get_string(prefix + "_PASSWORD");
        secret.kc = get_u64(prefix + "_KC");
        out.push_back(secret);
    }
    return out;
}
} // namespace cyber
