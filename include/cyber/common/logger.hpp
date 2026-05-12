#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace cyber
{
class Logger
{
public:
    explicit Logger(std::filesystem::path path);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    const std::filesystem::path& path() const;

    void write(std::string_view entity, std::string_view thread_name, std::string_view event,
               std::string_view message);
    void flush();

private:
    void writer_loop();
    void throw_if_failed_locked() const;

    std::filesystem::path path_;
    std::ofstream out_;
    std::mutex mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable flush_cv_;
    std::deque<std::string> queue_;
    std::thread writer_thread_;
    bool stopping_ = false;
    std::uint64_t pending_flush_id_ = 0;
    std::uint64_t completed_flush_id_ = 0;
    std::string writer_error_;
};
} // namespace cyber
