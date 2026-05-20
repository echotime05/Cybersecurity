#pragma once

#include "cyber/game/battle_room.hpp"
#include "cyber/roles/v/v_auth_service.hpp"
#include "cyber/shared/config.hpp"
#include "cyber/shared/net_socket.hpp"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace cyber::game
{
// V 服务器主类：先完成 V_AUTH 和证书交换，再接收签名加密游戏报文并广播权威世界快照。
class TankGameServer
{
public:
    // 构造正式 V 服务；当前最终链路固定要求 Client 先认证再进入游戏。
    TankGameServer(TcpEndpoint endpoint, Config config);
    // 停止服务器并回收客户端线程。
    ~TankGameServer();

    // 阻塞运行 V 服务，直到 stop 被调用或进程退出。
    void run();
    // 测试用启动入口，后台运行并返回实际监听端口。
    std::uint16_t start_for_test();
    // 在当前线程运行监听、客户端处理和游戏循环。
    void run_until_stopped();
    // 请求停止 V 服务并关闭监听 socket。
    void stop();

private:
    // 一个已认证 Client 的连接状态，包含后续通信所需的 Kc_v 和公钥。
    struct ClientConnection
    {
        SocketHandle socket = 0;
        EntityId client_id = EntityId::unknown;
        std::uint64_t kc_v = 0;
        RsaPublicKey client_public_key;
    };

    // 接受新 TCP 连接并为每个连接创建客户端线程。
    void accept_loop();
    // 处理一个 Client 连接上的认证、收包和断开清理。
    void client_loop(SocketHandle socket);
    // V 的固定 tick 游戏循环，周期性推进房间并广播快照。
    void game_loop();
    // 将世界快照签名加密后广播给所有在线 Client。
    void broadcast(const BattleStateSnapshot& snapshot);
    // 处理一个已认证 Client 发来的游戏报文，并回复 ACK。
    void handle_packet(SocketHandle socket, const Packet& packet, std::uint64_t kc_v,
                       const RsaPublicKey& client_public_key);
    // 在同一个 socket 上完成 V_AUTH 和证书交换，产出游戏链路所需材料。
    bool authenticate_socket(SocketHandle socket, EntityId& client_id, std::uint64_t& kc_v,
                             RsaPublicKey& client_public_key);
    // 等待所有客户端线程结束。
    void join_client_threads();

    TcpEndpoint endpoint_;
    Config config_;
    cyber::roles::v::AuthRuntime auth_runtime_;
    SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex connections_mutex_;
    std::map<EntityId, ClientConnection> connections_;
    std::vector<std::thread> client_threads_;
    BattleRoom room_;
};
} // namespace cyber::game
