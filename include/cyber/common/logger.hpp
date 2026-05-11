#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace cyber
{
class Logger
{
public:
    explicit Logger(std::filesystem::path path);

    const std::filesystem::path& path() const;

    void write(std::string_view entity, std::string_view thread_name, std::string_view event,
               std::string_view message);

private:
    std::filesystem::path path_;
    std::ofstream out_;
    std::mutex mutex_;
};
} // namespace cyber
