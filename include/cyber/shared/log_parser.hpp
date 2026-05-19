#pragma once

#include <string>
#include <string_view>

namespace cyber
{
struct LogEntry
{
    std::string entity;
    std::string thread_name;
    std::string event;
    std::string message;
};

bool parse_log_line(std::string_view line, LogEntry& out);
LogEntry parse_log_line_or_throw(std::string_view line);
} // namespace cyber
