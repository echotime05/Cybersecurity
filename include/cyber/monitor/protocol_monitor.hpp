#pragma once

#include "cyber/shared/net_socket.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace cyber::monitor
{
// 增量读取 protocol_events/*.txt，并把新增事件转换为 Web UI 可消费的 JSON。
class ProtocolEventTailer
{
public:
    // 指定协议事件日志目录。
    explicit ProtocolEventTailer(std::filesystem::path events_dir);

    // 读取所有新增事件，返回已经带 id 的 JSON 字符串。
    std::vector<std::string> poll_json_events();

private:
    std::filesystem::path events_dir_;
    std::map<std::filesystem::path, std::uintmax_t> offsets_;
    std::uint64_t next_id_ = 1;
};

// Monitor WebSocket 服务，把本机协议事件实时推送到 Web UI 的 Protocol 页面。
class ProtocolMonitorServer
{
public:
    // 构造监听端点和协议事件目录。
    ProtocolMonitorServer(TcpEndpoint endpoint, std::filesystem::path events_dir);
    // 停止服务并释放监听 socket。
    ~ProtocolMonitorServer();

    // 阻塞运行 Monitor 服务。
    void run();
    // 测试用启动入口，后台运行并返回实际端口。
    std::uint16_t start_for_test();
    // 在当前线程运行监听和事件推送。
    void run_until_stopped();
    // 请求停止 Monitor。
    void stop();

private:
    // 接受浏览器 WebSocket 连接。
    void accept_loop();
    // 为单个 Web UI 连接推送协议事件。
    void handle_client(SocketHandle socket);

    TcpEndpoint endpoint_;
    std::filesystem::path events_dir_;
    SocketHandle listener_ = 0;
    bool stopping_ = false;
};
} // namespace cyber::monitor
