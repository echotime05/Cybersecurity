#pragma once

#include "cyber/shared/config.hpp"
#include "cyber/shared/net_socket.hpp"
#include "cyber/game/battle_room.hpp"
#include "cyber/roles/v/v_auth_service.hpp"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace cyber::game
{
class TankGameServer
{
public:
    explicit TankGameServer(TcpEndpoint endpoint);
    TankGameServer(TcpEndpoint endpoint, Config config, bool require_auth);
    ~TankGameServer();

    void run();
    std::uint16_t start_for_test();
    void run_until_stopped();
    void stop();

private:
    struct ClientConnection
    {
        SocketHandle socket = 0;
        EntityId client_id = EntityId::unknown;
        std::uint64_t kc_v = 0;
        RsaPublicKey client_public_key;
    };

    void accept_loop();
    void client_loop(SocketHandle socket, std::string peer);
    void game_loop();
    void broadcast(const BattleStateSnapshot& snapshot);
    void handle_packet(SocketHandle socket, const Packet& packet, std::uint64_t kc_v,
                       const RsaPublicKey& client_public_key);
    bool authenticate_socket(SocketHandle socket, const std::string& peer, EntityId& client_id,
                             std::uint64_t& kc_v, RsaPublicKey& client_public_key);
    void join_client_threads();

    TcpEndpoint endpoint_;
    Config config_;
    bool require_auth_ = false;
    cyber::roles::v::AuthRuntime auth_runtime_;
    SocketHandle listener_ = 0;
    std::atomic<bool> stopping_{false};
    std::mutex connections_mutex_;
    std::map<EntityId, ClientConnection> connections_;
    std::vector<std::thread> client_threads_;
    BattleRoom room_;
};
} // namespace cyber::game
