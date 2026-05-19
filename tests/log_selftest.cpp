#include "cyber/shared/log_parser.hpp"
#include "cyber/shared/logger.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

std::vector<std::string> read_lines(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("failed to read log file: " + path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }
    return lines;
}
} // namespace

int main()
{
    try
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("cyber_log_selftest_" + std::to_string(static_cast<long long>(now)));
        const std::filesystem::path log_path = dir / "logs" / "selftest.log";

        {
            cyber::Logger logger(log_path);
            logger.write("Client", "UI/GameThread", "THREAD_START", "logger test start");
            logger.write("Client", "UI/GameThread", "LOG", "log file=logs/selftest.log");

            std::thread thread_a([&]() {
                for (int i = 0; i < 20; ++i)
                {
                    logger.write("Client", "WorkerA", "PACKET_SEND", "line=" + std::to_string(i));
                }
            });
            std::thread thread_b([&]() {
                for (int i = 0; i < 20; ++i)
                {
                    logger.write("AS", "WorkerB", "PACKET_RECV", "line=" + std::to_string(i));
                }
            });
            thread_a.join();
            thread_b.join();

            logger.flush();
            const std::vector<std::string> flushed_lines = read_lines(log_path);
            require(flushed_lines.size() == 42U, "flush did not make queued log lines visible");

            logger.write("Client", "UI/GameThread", "THREAD_EXIT", "logger test exit");
        }

        const std::vector<std::string> lines = read_lines(log_path);
        require(lines.size() == 43U, "unexpected log line count");
        require(lines.front() == "[Client][UI/GameThread][THREAD_START] logger test start",
                "first log line mismatch");
        require(lines.back() == "[Client][UI/GameThread][THREAD_EXIT] logger test exit",
                "last log line mismatch");

        for (const std::string& line : lines)
        {
            require(!line.empty(), "empty log line");
            require(line[0] == '[', "log line does not start with entity bracket");
            require(line.find("][") != std::string::npos, "log line is missing bracket groups");
            require(line.find("] ") != std::string::npos, "log line is missing message separator");

            cyber::LogEntry entry;
            require(cyber::parse_log_line(line, entry), "valid log line did not parse");
            require(!entry.entity.empty(), "parsed entity is empty");
            require(!entry.thread_name.empty(), "parsed thread name is empty");
            require(!entry.event.empty(), "parsed event is empty");
        }

        const cyber::LogEntry first = cyber::parse_log_line_or_throw(lines.front());
        require(first.entity == "Client", "parsed first entity mismatch");
        require(first.thread_name == "UI/GameThread", "parsed first thread mismatch");
        require(first.event == "THREAD_START", "parsed first event mismatch");
        require(first.message == "logger test start", "parsed first message mismatch");

        cyber::LogEntry ignored;
        require(!cyber::parse_log_line("", ignored), "empty line unexpectedly parsed");
        require(!cyber::parse_log_line("not a structured line", ignored),
                "plain line unexpectedly parsed");
        require(!cyber::parse_log_line("[Client][Thread][EVENT]", ignored),
                "line without message separator unexpectedly parsed");
        require(!cyber::parse_log_line("[Client][][EVENT] message", ignored),
                "line with empty thread unexpectedly parsed");
        require(!cyber::parse_log_line("[][Thread][EVENT] message", ignored),
                "line with empty entity unexpectedly parsed");

        std::filesystem::remove_all(dir);
        std::cout << "log_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "log_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
