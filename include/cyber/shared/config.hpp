#pragma once

#include "cyber/shared/types.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cyber
{
// 配置文件中一个 Client 的认证材料：身份、口令和由口令派生的 Kc。
struct ClientSecret
{
    EntityId id = EntityId::unknown;
    std::string password;
    std::uint64_t kc = 0;
};

// 简单 key=value 配置读取器，负责给 AS/TGS/V/Client 提供端口、密钥和客户端信息。
class Config
{
public:
    // 从指定配置文件加载全部 key=value 配置。
    static Config load(const std::filesystem::path& path);

    // 判断配置项是否存在。
    bool has(const std::string& key) const;
    // 读取字符串配置项，不存在时抛出异常。
    std::string get_string(const std::string& key) const;
    // 读取无符号 64 位整数配置项，不存在或格式错误时抛出异常。
    std::uint64_t get_u64(const std::string& key) const;
    // 读取端口等无符号 16 位整数配置项。
    std::uint16_t get_u16(const std::string& key) const;
    // 读取角色身份配置项，并转换为 EntityId。
    EntityId get_entity_id(const std::string& key) const;

    // 读取配置中声明的全部 Client 账号和 Kc。
    std::vector<ClientSecret> clients() const;

private:
    std::map<std::string, std::string> values_;
};
} // namespace cyber
