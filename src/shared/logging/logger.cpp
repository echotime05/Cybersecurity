#include "cyber/shared/logger.hpp"

#include <chrono>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cyber
{
namespace
{
// 日志后台线程最长等待时间，超时也会尝试刷盘。
constexpr std::chrono::milliseconds kLogFlushInterval(20);
// 队列达到该条数时主动唤醒后台线程批量写入。
constexpr std::size_t kLogBatchThreshold = 64;
} // namespace

// 打开日志文件并启动后台写线程。
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

    writer_thread_ = std::thread(&Logger::writer_loop, this);
}

// 停止后台写线程并写完剩余日志。
Logger::~Logger()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        ++pending_flush_id_;
    }
    queue_cv_.notify_one();

    if (writer_thread_.joinable())
    {
        writer_thread_.join();
    }
}

// 返回当前日志文件路径。
const std::filesystem::path& Logger::path() const
{
    return path_;
}

// 将一条结构化日志放入异步队列。
void Logger::write(std::string_view entity, std::string_view thread_name, std::string_view event,
                   std::string_view message)
{
    std::ostringstream line;
    line << '[' << entity << "][" << thread_name << "][" << event << "] " << message << '\n';

    std::lock_guard<std::mutex> lock(mutex_);
    throw_if_failed_locked();
    if (stopping_)
    {
        throw std::runtime_error("logger is shutting down: " + path_.string());
    }

    queue_.push_back(line.str());
    if (queue_.size() >= kLogBatchThreshold)
    {
        queue_cv_.notify_one();
    }
}

// 等待当前队列中日志全部落盘。
void Logger::flush()
{
    std::unique_lock<std::mutex> lock(mutex_);
    throw_if_failed_locked();

    const std::uint64_t flush_id = ++pending_flush_id_;
    queue_cv_.notify_one();
    flush_cv_.wait(lock, [&]() {
        return completed_flush_id_ >= flush_id || !writer_error_.empty();
    });
    throw_if_failed_locked();
}

// 后台写线程主循环，批量取队列并写入文件。
void Logger::writer_loop()
{
    for (;;)
    {
        std::deque<std::string> batch;
        std::uint64_t flush_id = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            const bool woke_by_predicate = queue_cv_.wait_for(lock, kLogFlushInterval, [&]() {
                return stopping_ || pending_flush_id_ > completed_flush_id_ ||
                       queue_.size() >= kLogBatchThreshold;
            });

            if (!woke_by_predicate && queue_.empty())
            {
                continue;
            }

            if (stopping_ && queue_.empty() && pending_flush_id_ == completed_flush_id_)
            {
                break;
            }

            batch.swap(queue_);
            flush_id = pending_flush_id_;
        }

        try
        {
            for (const std::string& line : batch)
            {
                out_ << line;
            }
            out_.flush();
            if (!out_)
            {
                throw std::runtime_error("failed to write log file: " + path_.string());
            }
        }
        catch (const std::exception& ex)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writer_error_ = ex.what();
            completed_flush_id_ = pending_flush_id_;
            flush_cv_.notify_all();
            return;
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            writer_error_ = "failed to write log file: " + path_.string();
            completed_flush_id_ = pending_flush_id_;
            flush_cv_.notify_all();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (flush_id > completed_flush_id_)
            {
                completed_flush_id_ = flush_id;
                flush_cv_.notify_all();
            }
        }
    }
}

// 检查后台线程是否记录了写文件错误。
void Logger::throw_if_failed_locked() const
{
    if (!writer_error_.empty())
    {
        throw std::runtime_error(writer_error_);
    }
}
} // namespace cyber
