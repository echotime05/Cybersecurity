#pragma once

#include <string>
#include <string_view>

namespace cyber
{
// 解析普通文本日志后得到的一条结构化记录。
struct LogEntry
{
    std::string entity;
    std::string thread_name;
    std::string event;
    std::string message;
};

// 尝试解析一行普通日志，成功时写入 out 并返回 true。
bool parse_log_line(std::string_view line, LogEntry& out);
// 解析一行普通日志，格式错误时抛出异常。
LogEntry parse_log_line_or_throw(std::string_view line);
} // namespace cyber
