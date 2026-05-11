#include "cyber/common/logger.hpp"

#include <stdexcept>

namespace cyber
{
Logger::Logger(std::filesystem::path path) : path_(std::move(path))
{
    if (path_.empty())
    {
        throw std::runtime_error("log path is empty");
    }

    const std::filesystem::path parent = path_.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    out_.open(path_, std::ios::out | std::ios::app);
    if (!out_)
    {
        throw std::runtime_error("failed to open log file: " + path_.string());
    }
}

const std::filesystem::path& Logger::path() const
{
    return path_;
}

void Logger::write(std::string_view entity, std::string_view thread_name, std::string_view event,
                   std::string_view message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << '[' << entity << "][" << thread_name << "][" << event << "] " << message << '\n';
    out_.flush();
    if (!out_)
    {
        throw std::runtime_error("failed to write log file: " + path_.string());
    }
}
} // namespace cyber
