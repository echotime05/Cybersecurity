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
// 异步文本日志写入器，运行时日志和 ACK 日志可避免阻塞主网络线程。
class Logger
{
public:
    // 打开目标日志文件，并启动后台写线程。
    explicit Logger(std::filesystem::path path);
    // 停止后台写线程，析构前写完队列中剩余日志。
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 返回当前日志文件路径，便于调试和 README 说明。
    const std::filesystem::path& path() const;

    // 写入一条结构化文本日志：角色、线程、事件和消息。
    void write(std::string_view entity, std::string_view thread_name, std::string_view event,
               std::string_view message);
    // 等待后台线程把当前队列内容落盘。
    void flush();

private:
    // 后台循环，从队列取日志并写入文件。
    void writer_loop();
    // 在持锁状态下检查后台写线程是否出现文件错误。
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
