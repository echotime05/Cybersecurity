#include "cyber/common/runtime_paths.hpp"

#include <string>

namespace cyber
{
std::filesystem::path default_log_root()
{
    if (std::filesystem::exists("_generated"))
    {
        return std::filesystem::path("_generated") / "logs";
    }
    return "logs";
}

std::filesystem::path log_root_from_config(const Config& config)
{
    if (config.has("LOG_ROOT"))
    {
        return config.get_string("LOG_ROOT");
    }
    return default_log_root();
}

std::filesystem::path log_path(const Config& config, std::string_view filename)
{
    return log_root_from_config(config) / std::string(filename);
}

std::filesystem::path log_path(std::string_view filename)
{
    return default_log_root() / std::string(filename);
}

std::filesystem::path protocol_events_dir(const Config& config)
{
    return log_root_from_config(config) / "protocol_events";
}

std::filesystem::path protocol_events_dir()
{
    return default_log_root() / "protocol_events";
}
} // namespace cyber
