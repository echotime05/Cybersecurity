#pragma once

#include "cyber/common/config.hpp"

#include <filesystem>
#include <string_view>

namespace cyber
{
std::filesystem::path default_log_root();
std::filesystem::path log_root_from_config(const Config& config);
std::filesystem::path log_path(const Config& config, std::string_view filename);
std::filesystem::path log_path(std::string_view filename);
std::filesystem::path protocol_events_dir(const Config& config);
std::filesystem::path protocol_events_dir();
} // namespace cyber
