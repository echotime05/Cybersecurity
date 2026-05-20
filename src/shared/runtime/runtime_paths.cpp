#include "cyber/shared/runtime_paths.hpp"

#include <string>

namespace cyber
{
// 返回默认日志目录，构建目录存在时优先写到 _generated/logs。
std::filesystem::path default_log_root()
{
    if (std::filesystem::exists("_generated"))
    {
        return std::filesystem::path("_generated") / "logs";
    }
    return "logs";
}

// 从配置读取 LOG_ROOT，没有配置时使用默认日志目录。
std::filesystem::path log_root_from_config(const Config& config)
{
    if (config.has("LOG_ROOT"))
    {
        return config.get_string("LOG_ROOT");
    }
    return default_log_root();
}

// 基于配置日志根目录生成指定日志文件路径。
std::filesystem::path log_path(const Config& config, std::string_view filename)
{
    return log_root_from_config(config) / std::string(filename);
}

// 基于默认日志根目录生成指定日志文件路径。
std::filesystem::path log_path(std::string_view filename)
{
    return default_log_root() / std::string(filename);
}

// 基于配置日志根目录返回协议事件目录。
std::filesystem::path protocol_events_dir(const Config& config)
{
    return log_root_from_config(config) / "protocol_events";
}

// 基于默认日志根目录返回协议事件目录。
std::filesystem::path protocol_events_dir()
{
    return default_log_root() / "protocol_events";
}
} // namespace cyber
