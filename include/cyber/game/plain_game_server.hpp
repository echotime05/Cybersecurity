#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/battle_room.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace cyber::game
{
class PlainGameServer
{
public:
    explicit PlainGameServer(TcpEndpoint endpoint);
    ~PlainGameServer();

    void run();
    std::uint16_t start_for_test();
    void run_until_stopped();
    void stop();

private:
    struct ClientConnection
    {
        SocketHandle socket = 0;
        EntityId client_id = EntityId::unknown;
    };

    void accept_loop();
    void client_loop(SocketHandle socket, std::string peer);
    void game_loop();
    void broadcast(const BattleStateSnapshot& snapshot);
    void handle_packet(SocketHandle socket, const Packet& packet);
    void join_client_threads();

    TcpEndpoint endpoint_;
    SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex connections_mutex_;
    std::map<EntityId, ClientConnection> connections_;
    std::vector<std::thread> client_threads_;
    BattleRoom room_;
    Logger logger_;
};
} // namespace cyber::game
