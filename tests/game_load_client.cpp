#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/protocol/protocol_event.hpp"
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_non_repudiation.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/roles/client/client_auth_flow.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

struct ClientStats
{
    cyber::EntityId id = cyber::EntityId::unknown;
    std::uint64_t input_sent = 0;
    std::uint64_t input_ack_recv = 0;
    std::uint64_t state_recv = 0;
    std::uint64_t state_ack_sent = 0;
    std::uint64_t errors = 0;
    double state_interval_sum_ms = 0.0;
    double state_interval_max_ms = 0.0;
    double state_interval_min_ms = 0.0;
    std::uint64_t state_interval_count = 0;
    std::string error_message;
};

cyber::ProtocolPayloadView app_payload_view(const cyber::Packet& packet, std::uint64_t kc_v)
{
    cyber::ProtocolPayloadView view;
    view.plain_hex = cyber::bytes_to_hex(cyber::game::app_decode_payload(packet.payload, kc_v));
    view.encrypted_hex = cyber::bytes_to_hex(packet.payload);
    return view;
}

void record_state_interval(ClientStats& stats, Clock::time_point& last_state,
                           Clock::time_point current)
{
    if (last_state != Clock::time_point{})
    {
        const double interval_ms =
            std::chrono::duration<double, std::milli>(current - last_state).count();
        stats.state_interval_sum_ms += interval_ms;
        stats.state_interval_max_ms =
            stats.state_interval_count == 0
                ? interval_ms
                : std::max(stats.state_interval_max_ms, interval_ms);
        stats.state_interval_min_ms =
            stats.state_interval_count == 0
                ? interval_ms
                : std::min(stats.state_interval_min_ms, interval_ms);
        ++stats.state_interval_count;
    }
    last_state = current;
}

void send_signed_game_packet(cyber::SocketHandle socket,
                             const cyber::roles::client::AuthClientState& auth_state,
                             cyber::game::GameMsgType type, const cyber::Bytes& payload,
                             std::mutex& send_mutex, ClientStats& stats)
{
    const cyber::Packet packet = cyber::game::app_build_signed_game_packet(
        auth_state.client_id, cyber::EntityId::v, type, payload, auth_state.kc_v,
        auth_state.client_key_pair.private_key);

    std::lock_guard<std::mutex> lock(send_mutex);
    cyber::send_packet_logged(socket, packet);
    cyber::write_protocol_event(
        cyber::ProtocolDirection::send, packet,
        cyber::protocol_app_message(cyber::game::app_map_game_message_code(type)),
        app_payload_view(packet, auth_state.kc_v));
    ++stats.input_sent;
}

void send_state_ack(cyber::SocketHandle socket,
                    const cyber::roles::client::AuthClientState& auth_state,
                    const cyber::Packet& received_packet,
                    const cyber::SignedAppPayload& received_payload,
                    std::mutex& send_mutex, ClientStats& stats)
{
    const cyber::Packet ack = cyber::game::ack_build_signed_packet(
        received_packet, received_payload, auth_state.client_id, cyber::EntityId::v,
        auth_state.kc_v, auth_state.client_key_pair.private_key);

    std::lock_guard<std::mutex> lock(send_mutex);
    cyber::send_packet_logged(socket, ack);
    cyber::write_protocol_event(cyber::ProtocolDirection::send, ack,
                                cyber::protocol_app_message(cyber::AppCode::app_ack),
                                app_payload_view(ack, auth_state.kc_v));
    ++stats.state_ack_sent;
}

void receiver_loop(cyber::SocketHandle socket,
                   const cyber::roles::client::AuthClientState& auth_state,
                   std::atomic<bool>& stopping,
                   std::mutex& send_mutex, ClientStats& stats)
{
    Clock::time_point last_state;
    while (!stopping)
    {
        try
        {
            const cyber::Packet packet = cyber::recv_packet_logged(socket);
            if (packet.msg_type != cyber::MsgType::app)
            {
                continue;
            }

            const cyber::SignedAppPayload signed_payload =
                cyber::game::app_decode_signed_packet(packet, auth_state.kc_v);
            if (signed_payload.app_code == cyber::AppCode::app_ack)
            {
                (void)cyber::game::ack_parse_verified_payload(signed_payload,
                                                               auth_state.v_public_key);
                cyber::write_protocol_event(cyber::ProtocolDirection::recv, packet,
                                            cyber::protocol_app_message(cyber::AppCode::app_ack),
                                            app_payload_view(packet, auth_state.kc_v));
                ++stats.input_ack_recv;
                continue;
            }

            const cyber::game::GameMessage message =
                cyber::game::app_parse_verified_game_message(signed_payload,
                                                          auth_state.v_public_key);
            cyber::write_protocol_event(
                cyber::ProtocolDirection::recv, packet,
                cyber::protocol_app_message(signed_payload.app_code),
                app_payload_view(packet, auth_state.kc_v));
            if (message.type == cyber::game::GameMsgType::state)
            {
                ++stats.state_recv;
                record_state_interval(stats, last_state, Clock::now());
                send_state_ack(socket, auth_state, packet, signed_payload, send_mutex, stats);
            }
        }
        catch (const std::exception& ex)
        {
            if (!stopping)
            {
                ++stats.errors;
                stats.error_message = ex.what();
            }
            return;
        }
    }
}

void client_worker(const cyber::Config& config, const cyber::ClientSecret& secret,
                   int duration_seconds, int input_hz, ClientStats& stats)
{
    stats.id = secret.id;
    cyber::SocketHandle socket = 0;
    std::atomic<bool> stopping{false};
    std::mutex send_mutex;
    std::thread receiver;
    cyber::roles::client::AuthClientState auth_state;
    try
    {
        const std::uint64_t kc = cyber::auth_derive_client_key(secret.id, secret.password);
        cyber::roles::client::VAuthenticatedSocket auth =
            cyber::roles::client::client_auth_connect_to_v_socket(config, secret.id, kc);
        socket = auth.socket;
        auth_state = auth.state;

        receiver = std::thread([&]() {
            receiver_loop(socket, auth_state, stopping, send_mutex, stats);
        });

        send_signed_game_packet(socket, auth_state, cyber::game::GameMsgType::join,
                                cyber::game::game_build_join({secret.id}),
                                send_mutex, stats);

        const auto end_time = Clock::now() + std::chrono::seconds(duration_seconds);
        const auto frame_interval =
            std::chrono::microseconds(std::max(1, 1000000 / input_hz));
        auto next_frame = Clock::now();
        int frame = 0;
        while (Clock::now() < end_time)
        {
            next_frame += frame_interval;
            const int phase = (frame + static_cast<int>(secret.id)) % 4;
            const std::int8_t x = phase == 0 ? 1 : (phase == 2 ? -1 : 0);
            const std::int8_t y = phase == 1 ? 1 : (phase == 3 ? -1 : 0);
            const float angle = static_cast<float>((frame * 19 + static_cast<int>(secret.id) * 37) %
                                                   360);

            send_signed_game_packet(socket, auth_state, cyber::game::GameMsgType::move,
                                    cyber::game::game_build_move({x, y}), send_mutex, stats);
            send_signed_game_packet(socket, auth_state, cyber::game::GameMsgType::target,
                                    cyber::game::game_build_target({angle}), send_mutex, stats);
            if (frame % 5 == 0)
            {
                send_signed_game_packet(socket, auth_state, cyber::game::GameMsgType::shoot,
                                        cyber::game::game_build_shoot({}), send_mutex, stats);
            }
            ++frame;
            std::this_thread::sleep_until(next_frame);
        }

        stopping = true;
        cyber::close_socket(socket);
        socket = 0;
        if (receiver.joinable())
        {
            receiver.join();
        }
    }
    catch (const std::exception& ex)
    {
        stopping = true;
        if (socket != 0)
        {
            cyber::close_socket(socket);
            socket = 0;
        }
        if (receiver.joinable())
        {
            receiver.join();
        }
        ++stats.errors;
        stats.error_message = ex.what();
    }
    if (socket != 0)
    {
        cyber::close_socket(socket);
    }
}

double average_interval(const ClientStats& stats)
{
    if (stats.state_interval_count == 0)
    {
        return 0.0;
    }
    return stats.state_interval_sum_ms / static_cast<double>(stats.state_interval_count);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr << "usage: game_load_client CONFIG DURATION_SECONDS INPUT_HZ\n";
        return 2;
    }

    try
    {
        const std::filesystem::path config_path = argv[1];
        const int duration_seconds = std::stoi(argv[2]);
        const int input_hz = std::stoi(argv[3]);
        if (duration_seconds <= 0 || input_hz <= 0)
        {
            throw std::runtime_error("duration and input_hz must be positive");
        }

        cyber::SocketRuntime runtime;
        const cyber::Config config = cyber::Config::load(config_path);
        const std::vector<cyber::ClientSecret> clients = config.clients();
        if (clients.size() != 4U)
        {
            throw std::runtime_error("load test requires four clients in config");
        }

        std::vector<ClientStats> stats(4);
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < clients.size(); ++i)
        {
            threads.emplace_back(client_worker, std::cref(config), std::cref(clients[i]),
                                 duration_seconds, input_hz, std::ref(stats[i]));
        }
        for (std::thread& thread : threads)
        {
            thread.join();
        }

        std::uint64_t total_input_sent = 0;
        std::uint64_t total_input_ack_recv = 0;
        std::uint64_t total_state_recv = 0;
        std::uint64_t total_state_ack_sent = 0;
        std::uint64_t total_errors = 0;
        double interval_sum = 0.0;
        std::uint64_t interval_count = 0;
        double interval_max = 0.0;

        for (const ClientStats& item : stats)
        {
            total_input_sent += item.input_sent;
            total_input_ack_recv += item.input_ack_recv;
            total_state_recv += item.state_recv;
            total_state_ack_sent += item.state_ack_sent;
            total_errors += item.errors;
            interval_sum += item.state_interval_sum_ms;
            interval_count += item.state_interval_count;
            interval_max = std::max(interval_max, item.state_interval_max_ms);

            std::cout << "PERF_CLIENT client=" << cyber::to_string(item.id)
                      << " input_sent=" << item.input_sent
                      << " input_ack_recv=" << item.input_ack_recv
                      << " state_recv=" << item.state_recv
                      << " state_ack_sent=" << item.state_ack_sent
                      << " avg_state_interval_ms=" << std::fixed << std::setprecision(2)
                      << average_interval(item)
                      << " max_state_interval_ms=" << item.state_interval_max_ms
                      << " errors=" << item.errors;
            if (!item.error_message.empty())
            {
                std::cout << " error=\"" << item.error_message << "\"";
            }
            std::cout << '\n';
        }

        const double avg_interval =
            interval_count == 0 ? 0.0 : interval_sum / static_cast<double>(interval_count);
        std::cout << "PERF_SUMMARY clients=4 duration_s=" << duration_seconds
                  << " input_hz=" << input_hz
                  << " input_sent=" << total_input_sent
                  << " input_ack_recv=" << total_input_ack_recv
                  << " state_recv=" << total_state_recv
                  << " state_ack_sent=" << total_state_ack_sent
                  << " avg_state_interval_ms=" << std::fixed << std::setprecision(2)
                  << avg_interval
                  << " max_state_interval_ms=" << interval_max
                  << " errors=" << total_errors << '\n';
        return total_errors == 0 ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "game_load_client failed: " << ex.what() << '\n';
        return 1;
    }
}
