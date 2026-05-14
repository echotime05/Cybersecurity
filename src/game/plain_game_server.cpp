#include "cyber/game/plain_game_server.hpp"

#include "cyber/common/net_packet.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

namespace cyber::game
{
namespace
{
std::uint64_t now_system_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}
} // namespace

PlainGameServer::PlainGameServer(TcpEndpoint endpoint)
    : endpoint_(std::move(endpoint)), logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}

PlainGameServer::PlainGameServer(TcpEndpoint endpoint, Config config, bool require_auth)
    : endpoint_(std::move(endpoint)),
      config_(std::move(config)),
      require_auth_(require_auth),
      auth_runtime_(make_auth_runtime(config_)),
      logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}

PlainGameServer::~PlainGameServer()
{
    stop();
    join_client_threads();
}

void PlainGameServer::run()
{
    listener_ = listen_tcp(endpoint_);
    std::cout << "V plaintext game listening on " << endpoint_.ip << ':' << endpoint_.port
              << '\n';
    run_until_stopped();
}

std::uint16_t PlainGameServer::start_for_test()
{
    listener_ = listen_tcp(endpoint_);
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getsockname(static_cast<SOCKET>(listener_), reinterpret_cast<sockaddr*>(&addr), &len) != 0)
    {
        throw std::runtime_error("getsockname failed for plaintext game server");
    }
    endpoint_.port = ntohs(addr.sin_port);
    return endpoint_.port;
}

void PlainGameServer::run_until_stopped()
{
    std::thread game_thread([&]() { game_loop(); });
    try
    {
        accept_loop();
    }
    catch (...)
    {
        stopping_ = true;
        if (listener_ != 0)
        {
            close_socket(listener_);
            listener_ = 0;
        }
        game_thread.join();
        join_client_threads();
        throw;
    }
    stopping_ = true;
    game_thread.join();
    join_client_threads();
}

void PlainGameServer::stop()
{
    stopping_ = true;
    if (listener_ != 0)
    {
        close_socket(listener_);
        listener_ = 0;
    }
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto& [id, connection] : connections_)
    {
        (void)id;
        if (connection.socket != 0)
        {
            close_socket(connection.socket);
            connection.socket = 0;
        }
    }
}

void PlainGameServer::accept_loop()
{
    while (!stopping_)
    {
        try
        {
            std::string peer;
            SocketHandle accepted = accept_tcp(listener_, &peer);
            logger_.write("V", "PlainAccept", "ACCEPT", "accept " + peer);
            client_threads_.emplace_back(&PlainGameServer::client_loop, this, accepted, peer);
        }
        catch (const std::exception& ex)
        {
            if (stopping_)
            {
                return;
            }
            logger_.write("V", "PlainAccept", "ERROR", ex.what());
            throw;
        }
    }
}

void PlainGameServer::client_loop(SocketHandle socket, std::string peer)
{
    logger_.write("V", "PlainClient", "THREAD_START", "client " + peer);
    try
    {
        EntityId authenticated_client = EntityId::unknown;
        if (require_auth_ && !authenticate_socket(socket, peer, authenticated_client))
        {
            close_socket(socket);
            logger_.write("V", "PlainClient", "THREAD_EXIT", "client " + peer);
            return;
        }
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(socket, logger_, "V", "PlainClient");
            if (require_auth_ && packet.src != authenticated_client)
            {
                throw std::runtime_error("authenticated client id mismatch");
            }
            handle_packet(socket, packet);
        }
    }
    catch (const std::exception& ex)
    {
        if (!stopping_)
        {
            logger_.write("V", "PlainClient", "ERROR", ex.what());
        }
    }

    EntityId leaving = EntityId::unknown;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto it = connections_.begin(); it != connections_.end(); ++it)
        {
            if (it->second.socket == socket)
            {
                leaving = it->first;
                connections_.erase(it);
                break;
            }
        }
    }
    if (is_client(leaving))
    {
        room_.leave(leaving);
    }
    close_socket(socket);
    logger_.write("V", "PlainClient", "THREAD_EXIT", "client " + peer);
}

void PlainGameServer::handle_packet(SocketHandle socket, const Packet& packet)
{
    if (packet.msg_type != MsgType::app)
    {
        logger_.write("V", "PlainClient", "ERROR", "non-app packet ignored");
        return;
    }

    const GameMessage message = parse_game_message(packet.payload);
    const std::uint64_t now_ms = now_system_ms();
    switch (message.type)
    {
    case GameMsgType::join:
    {
        const JoinMessage join = parse_join(message.payload);
        if (!is_client(join.client_id))
        {
            return;
        }
        if (join.client_id != packet.src)
        {
            throw std::runtime_error("join client id must match packet source");
        }
        room_.join(join.client_id, join.name, now_ms);
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto existing = connections_.find(join.client_id);
        if (existing != connections_.end() && existing->second.socket != socket &&
            existing->second.socket != 0)
        {
            close_socket(existing->second.socket);
        }
        connections_[join.client_id] = {socket, join.client_id};
        break;
    }
    case GameMsgType::move:
    {
        const MoveMessage move = parse_move(message.payload);
        room_.handle_move(packet.src, move.x, move.y);
        break;
    }
    case GameMsgType::target:
    {
        const TargetMessage target = parse_target(message.payload);
        room_.handle_target(packet.src, target.angle);
        break;
    }
    case GameMsgType::shoot:
    {
        const ShootMessage shoot = parse_shoot(message.payload);
        room_.handle_shoot(packet.src, shoot.shooting);
        break;
    }
    case GameMsgType::name:
    {
        const NameMessage name = parse_name(message.payload);
        room_.handle_name(packet.src, name.name);
        break;
    }
    default:
        logger_.write("V", "PlainClient", "ERROR", "unknown game message ignored");
        break;
    }
}

bool PlainGameServer::authenticate_socket(SocketHandle socket, const std::string& peer,
                                          EntityId& client_id)
{
    const Packet auth_packet = recv_packet_logged(socket, logger_, "V", "PlainGameAuth");
    if (auth_packet.msg_type != MsgType::v_auth_req || !is_client(auth_packet.src))
    {
        logger_.write("V", "PlainGameAuth", "ERROR",
                      "client " + peer + " sent app traffic before V_AUTH");
        return false;
    }
    const Packet response =
        process_v_auth_request(auth_packet, config_, auth_runtime_, logger_, "PlainGameAuth");
    send_packet_logged(socket, response, logger_, "V", "PlainGameAuth");
    client_id = auth_packet.src;
    return true;
}

void PlainGameServer::game_loop()
{
    using clock = std::chrono::steady_clock;
    auto next_tick = clock::now();
    while (!stopping_)
    {
        next_tick += std::chrono::milliseconds(50);
        std::this_thread::sleep_until(next_tick);
        const std::uint64_t now_ms = now_system_ms();
        room_.tick(now_ms);
        broadcast(room_.snapshot(now_ms));
    }
}

void PlainGameServer::broadcast(const BattleStateSnapshot& snapshot)
{
    std::vector<ClientConnection> targets;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [id, connection] : connections_)
        {
            (void)id;
            if (connection.socket != 0)
            {
                targets.push_back(connection);
            }
        }
    }
    if (targets.empty())
    {
        return;
    }

    const Bytes payload = build_game_message({GameMsgType::state, build_state(snapshot)});
    std::vector<EntityId> failed;
    for (const ClientConnection& target : targets)
    {
        try
        {
            send_packet_logged(target.socket,
                               make_packet(MsgType::app, EntityId::v, target.client_id, payload),
                               logger_, "V", "PlainGameLoop");
        }
        catch (const std::exception& ex)
        {
            logger_.write("V", "PlainGameLoop", "ERROR", ex.what());
            failed.push_back(target.client_id);
            close_socket(target.socket);
        }
    }

    if (!failed.empty())
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (EntityId id : failed)
        {
            connections_.erase(id);
            room_.leave(id);
        }
    }
}

void PlainGameServer::join_client_threads()
{
    for (std::thread& thread : client_threads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    client_threads_.clear();
}
} // namespace cyber::game
