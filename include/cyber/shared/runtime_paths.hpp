#pragma once

#include "cyber/shared/config.hpp"

#include <filesystem>
#include <string_view>

namespace cyber
{
// 返回默认日志根目录，通常是仓库下的 logs。
std::filesystem::path default_log_root();
// 优先从配置读取 log_root，否则使用默认日志根目录。
std::filesystem::path log_root_from_config(const Config& config);
// 基于配置日志根目录拼接一个普通日志文件路径。
std::filesystem::path log_path(const Config& config, std::string_view filename);
// 基于默认日志根目录拼接一个普通日志文件路径。
std::filesystem::path log_path(std::string_view filename);
// 基于配置日志根目录返回 protocol_events 子目录。
std::filesystem::path protocol_events_dir(const Config& config);
// 基于默认日志根目录返回 protocol_events 子目录。
std::filesystem::path protocol_events_dir();
} // namespace cyber
