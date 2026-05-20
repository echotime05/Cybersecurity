#include "cyber/roles/v/tank_game_server.hpp"

#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_non_repudiation.hpp"
#include "cyber/protocol/protocol_event.hpp"
#include "cyber/shared/crypto.hpp"
#include "cyber/shared/net_packet.hpp"
#include "cyber/shared/runtime_paths.hpp"

#include <chrono>
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
// 返回 V 当前使用的毫秒时间戳。
std::uint64_t game_time_now_ms()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

// 为 MSG_APP 构造协议可视化需要的明文/密文十六进制。
ProtocolPayloadView app_build_payload_view(const Packet& packet, std::uint64_t kc_v)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(app_decode_payload(packet.payload, kc_v));
    view.encrypted_hex = bytes_to_hex(packet.payload);
    return view;
}

// 构造通用 DES 加密 payload 的明文/密文可视化视图。
ProtocolPayloadView protocol_build_encrypted_payload_view(const Bytes& plain,
                                                          const Bytes& encrypted)
{
    ProtocolPayloadView view;
    view.plain_hex = bytes_to_hex(plain);
    view.encrypted_hex = bytes_to_hex(encrypted);
    return view;
}

// 解密报文 payload，并构造协议可视化明文/密文对照。
ProtocolPayloadView protocol_build_decrypted_payload_view(const Packet& packet, std::uint64_t key)
{
    return protocol_build_encrypted_payload_view(des_decrypt_payload(packet.payload, key),
                                                 packet.payload);
}

// 向协议可视化 payload 中追加一个字段级密文/明文对照。
void protocol_add_encrypted_field(ProtocolPayloadView& view, std::string name,
                                  const Bytes& encrypted, const Bytes& plain = {})
{
    ProtocolPayloadView::Field field;
    field.name = std::move(name);
    field.plain_hex = bytes_to_hex(plain);
    field.encrypted_hex = bytes_to_hex(encrypted);
    view.fields.push_back(std::move(field));
}

// 解析 V_AUTH_REQ 中的 ticket_v 和 authenticator_v，供 UI 展示。
ProtocolPayloadView v_auth_build_req_payload_view(const Packet& packet, const Config& config)
{
    const VAuthReq request = v_auth_parse_req(packet.payload);
    const TicketVBody ticket = v_ticket_decrypt(request.ticket_v, config.get_u64("KV"));
    const AuthenticatorBody auth = authenticator_decrypt(request.authenticator_v, ticket.kc_v);
    ProtocolPayloadView view;
    protocol_add_encrypted_field(view, "ticket_v", request.ticket_v, v_ticket_build_body(ticket));
    protocol_add_encrypted_field(view, "authenticator_v", request.authenticator_v,
                                 authenticator_build_body(auth));
    return view;
}
} // namespace

// 构造 V 服务；当前最终链路固定要求 V_AUTH、证书交换、签名加密游戏报文。
TankGameServer::TankGameServer(TcpEndpoint endpoint, Config config)
    : endpoint_(std::move(endpoint)),
      config_(std::move(config)),
      auth_runtime_(cyber::roles::v::v_auth_make_runtime(config_))
{
    set_protocol_event_log_root(log_root_from_config(config_));
}

// 停止 V 服务并等待客户端线程结束。
TankGameServer::~TankGameServer()
{
    stop();
    join_client_threads();
}

// 启动监听 socket 并进入 V 服务主循环。
void TankGameServer::run()
{
    listener_ = listen_tcp(endpoint_);
    std::cout << "V tank game listening on " << endpoint_.ip << ':' << endpoint_.port
              << " mode=auth-encrypted\n";
    run_until_stopped();
}

// 测试用启动入口：监听端口可为 0，启动后返回实际端口。
std::uint16_t TankGameServer::start_for_test()
{
    listener_ = listen_tcp(endpoint_);
    sockaddr_in addr{};
    int len = sizeof(addr);
    if (getsockname(static_cast<SOCKET>(listener_), reinterpret_cast<sockaddr*>(&addr), &len) != 0)
    {
        throw std::runtime_error("getsockname failed for tank game server");
    }
    endpoint_.port = ntohs(addr.sin_port);
    return endpoint_.port;
}

// 当前线程运行 V：并行启动 game tick 线程，再进入 accept 循环。
void TankGameServer::run_until_stopped()
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

// 请求停止 V，并关闭监听 socket 和所有 Client socket。
void TankGameServer::stop()
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

// 循环接受 Client TCP 连接，每个连接交给独立线程处理。
void TankGameServer::accept_loop()
{
    while (!stopping_)
    {
        try
        {
            std::string peer;
            SocketHandle accepted = accept_tcp(listener_, &peer);
            client_threads_.emplace_back(&TankGameServer::client_loop, this, accepted, peer);
        }
        catch (const std::exception&)
        {
            if (stopping_)
            {
                return;
            }
            throw;
        }
    }
}

// 处理单个 Client 连接：先认证，再循环接收签名加密游戏报文。
void TankGameServer::client_loop(SocketHandle socket, std::string peer)
{
    try
    {
        EntityId authenticated_client = EntityId::unknown;
        std::uint64_t kc_v = 0;
        RsaPublicKey client_public_key;
        if (!authenticate_socket(socket, peer, authenticated_client, kc_v, client_public_key))
        {
            close_socket(socket);
            return;
        }

        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(socket);
            if (packet.src != authenticated_client)
            {
                throw std::runtime_error("authenticated client id mismatch");
            }
            handle_packet(socket, packet, kc_v, client_public_key);
        }
    }
    catch (const std::exception&)
    {
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
}

// 验签 Client payload，写 ACK 证据，并把合法输入应用到 BattleRoom。
void TankGameServer::handle_packet(SocketHandle socket, const Packet& packet, std::uint64_t kc_v,
                                   const RsaPublicKey& client_public_key)
{
    if (packet.msg_type != MsgType::app)
    {
        return;
    }

    const SignedAppPayload signed_payload = app_decode_signed_packet(packet, kc_v);
    if (signed_payload.app_code == AppCode::app_ack)
    {
        (void)ack_parse_verified_payload(signed_payload, client_public_key);
        write_protocol_event(ProtocolDirection::recv, packet, protocol_app_message(AppCode::app_ack),
                             app_build_payload_view(packet, kc_v));
        return;
    }

    const GameMessage message = app_parse_verified_game_message(signed_payload, client_public_key);
    write_protocol_event(ProtocolDirection::recv, packet,
                         protocol_app_message(signed_payload.app_code),
                         app_build_payload_view(packet, kc_v));
    const Packet ack = ack_build_signed_packet(packet, signed_payload, EntityId::v, packet.src,
                                               kc_v, auth_runtime_.v_key_pair.private_key);
    send_packet_logged(socket, ack);
    write_protocol_event(ProtocolDirection::send, ack, protocol_app_message(AppCode::app_ack),
                         app_build_payload_view(ack, kc_v));

    const std::uint64_t now_ms = game_time_now_ms();
    switch (message.type)
    {
    case GameMsgType::join:
    {
        const JoinMessage join = game_parse_join(message.payload);
        if (!is_client(join.client_id))
        {
            return;
        }
        if (join.client_id != packet.src)
        {
            throw std::runtime_error("join client id must match packet source");
        }
        room_.join(join.client_id, now_ms);
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto existing = connections_.find(join.client_id);
        if (existing != connections_.end() && existing->second.socket != socket &&
            existing->second.socket != 0)
        {
            close_socket(existing->second.socket);
        }
        connections_[join.client_id] = {socket, join.client_id, kc_v, client_public_key};
        break;
    }
    case GameMsgType::move:
    {
        const MoveMessage move = game_parse_move(message.payload);
        room_.handle_move(packet.src, move.x, move.y);
        break;
    }
    case GameMsgType::target:
    {
        const TargetMessage target = game_parse_target(message.payload);
        room_.handle_target(packet.src, target.angle);
        break;
    }
    case GameMsgType::shoot:
    {
        (void)game_parse_shoot(message.payload);
        room_.handle_shoot(packet.src);
        break;
    }
    default:
        break;
    }
}

// 在同一条 V socket 上完成 V_AUTH 和证书交换。
bool TankGameServer::authenticate_socket(SocketHandle socket, const std::string&,
                                         EntityId& client_id, std::uint64_t& kc_v,
                                         RsaPublicKey& client_public_key)
{
    const Packet auth_packet = recv_packet_logged(socket);
    if (auth_packet.msg_type != MsgType::v_auth_req || !is_client(auth_packet.src))
    {
        return false;
    }

    const Packet response =
        cyber::roles::v::v_auth_process_request(auth_packet, config_, auth_runtime_);
    write_protocol_event(ProtocolDirection::recv, auth_packet, {},
                         v_auth_build_req_payload_view(auth_packet, config_));
    const cyber::roles::v::AuthSession auth_session =
        auth_runtime_.v_sessions.get(auth_packet.src);
    send_packet_logged(socket, response,
                       protocol_build_decrypted_payload_view(response, auth_session.kc_v));

    const Packet cert_packet = recv_packet_logged(socket);
    if (cert_packet.msg_type != MsgType::cert_c2v || cert_packet.src != auth_packet.src)
    {
        return false;
    }

    const cyber::roles::v::AuthSession cert_session =
        auth_runtime_.v_sessions.get(cert_packet.src);
    write_protocol_event(ProtocolDirection::recv, cert_packet, {},
                         protocol_build_decrypted_payload_view(cert_packet, cert_session.kc_v));
    const Packet cert_response =
        cyber::roles::v::cert_process_c2v_request(cert_packet, auth_runtime_);
    send_packet_logged(socket, cert_response,
                       protocol_build_decrypted_payload_view(cert_response, cert_session.kc_v));

    const cyber::roles::v::AuthSession session = auth_runtime_.v_sessions.get(auth_packet.src);
    client_id = auth_packet.src;
    kc_v = session.kc_v;
    client_public_key = session.client_public_key;
    return true;
}

// 固定 tick 推进权威世界，并广播最新快照。
void TankGameServer::game_loop()
{
    using clock = std::chrono::steady_clock;
    auto next_tick = clock::now();
    while (!stopping_)
    {
        next_tick += std::chrono::milliseconds(kServerTickIntervalMs);
        std::this_thread::sleep_until(next_tick);
        const std::uint64_t now_ms = game_time_now_ms();
        room_.tick(now_ms);
        broadcast(room_.snapshot(now_ms));
    }
}

// 将世界快照签名加密后发送给所有已加入 Client。
void TankGameServer::broadcast(const BattleStateSnapshot& snapshot)
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

    const Bytes state_payload = game_build_state(snapshot);
    std::vector<EntityId> failed;
    for (const ClientConnection& target : targets)
    {
        try
        {
            const Packet packet = app_build_signed_game_packet(
                EntityId::v, target.client_id, GameMsgType::state, state_payload, target.kc_v,
                auth_runtime_.v_key_pair.private_key);
            send_packet_logged(target.socket, packet);
            write_protocol_event(ProtocolDirection::send, packet,
                                 protocol_app_message(AppCode::game_state),
                                 app_build_payload_view(packet, target.kc_v));
        }
        catch (const std::exception&)
        {
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

// 等待所有 Client 处理线程结束，并清空线程列表。
void TankGameServer::join_client_threads()
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
