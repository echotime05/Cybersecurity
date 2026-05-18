#include "cyber/common/log_parser.hpp"

#include <stdexcept>

namespace cyber
{
bool parse_log_line(std::string_view line, LogEntry& out)
{
    LogEntry parsed;

    if (line.empty() || line.front() != '[')
    {
        return false;
    }

    const std::size_t entity_end = line.find(']', 1);
    if (entity_end == std::string_view::npos || entity_end == 1)
    {
        return false;
    }

    const std::size_t thread_start = entity_end + 1U;
    if (thread_start >= line.size() || line[thread_start] != '[')
    {
        return false;
    }

    const std::size_t thread_end = line.find(']', thread_start + 1U);
    if (thread_end == std::string_view::npos || thread_end == thread_start + 1U)
    {
        return false;
    }

    const std::size_t event_start = thread_end + 1U;
    if (event_start >= line.size() || line[event_start] != '[')
    {
        return false;
    }

    const std::size_t event_end = line.find(']', event_start + 1U);
    if (event_end == std::string_view::npos || event_end == event_start + 1U)
    {
        return false;
    }

    const std::size_t separator = event_end + 1U;
    if (separator >= line.size() || line[separator] != ' ')
    {
        return false;
    }

    parsed.entity = std::string(line.substr(1U, entity_end - 1U));
    parsed.thread_name = std::string(line.substr(thread_start + 1U, thread_end - thread_start - 1U));
    parsed.event = std::string(line.substr(event_start + 1U, event_end - event_start - 1U));
    parsed.message = std::string(line.substr(separator + 1U));

    out = std::move(parsed);
    return true;
}

LogEntry parse_log_line_or_throw(std::string_view line)
{
    LogEntry entry;
    if (!parse_log_line(line, entry))
    {
        throw std::runtime_error("invalid log line");
    }
    return entry;
}
} // namespace cyber
