#include "cyber/common/role_runtime.hpp"

#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_event.hpp"
#include "cyber/common/runtime_paths.hpp"
#include "cyber/roles/client/tank_game_client.hpp"
#include "cyber/roles/v/tank_game_server.hpp"
#include "cyber/roles/as/as_service.hpp"
#include "cyber/roles/tgs/tgs_service.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace cyber
{
namespace
{
struct RoleSpec
{
    const char* name;
    const char* binary;
    EntityId id;
    const char* id_key;
    const char* port_key;
    const char* connect_ip_key;
    const char* bind_ip_key;
    const char* stage_goal;
};

RoleSpec runtime_build_role_spec(RoleKind role)
{
    switch (role)
    {
    case RoleKind::as_server:
        return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
                "handle AS_REQ and return AS_REP"};
    case RoleKind::tgs_server:
        return {"TGS", "tgs_server", EntityId::tgs, "TGS_ID", "TGS_PORT", "TGS_IP",
                "TGS_BIND_IP",
                "handle TGS_REQ and return TGS_REP"};
    case RoleKind::v_server:
        return {"V", "v_server", EntityId::v, "V_ID", "V_PORT", "V_IP", "V_BIND_IP",
                "handle V_AUTH, certificate exchange, and MSG_APP"};
    case RoleKind::client:
        return {"Client", "client", EntityId::unknown, "LOCAL_CLIENT_ID", nullptr, nullptr,
                nullptr,
                "run AS, TGS, V_AUTH, certificate exchange, then game events"};
    default:
        throw std::runtime_error("unknown role");
    }
}

void runtime_print_usage(const RoleSpec& spec)
{
    std::cout << "Usage: " << spec.binary
              << " [--config PATH] [--print-config] [--serve]"
                 " [--max-connections N]"
                 " [--game-auth-encrypted] [--ui-port PORT]\n";
}

std::filesystem::path config_find_default_path()
{
    const std::vector<std::filesystem::path> candidates = {
        "config/course_config.txt",
        "../config/course_config.txt",
        "../../config/course_config.txt"};

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            return candidate;
        }
    }
    return candidates.front();
}

void config_print_endpoint(const Config& config, const char* name, const char* ip_key,
                           const char* bind_ip_key, const char* port_key)
{
    std::cout << name << " connect: " << config.get_string(ip_key) << ':'
              << config.get_u16(port_key) << '\n';
    std::cout << name << " listen:  " << config.get_string(bind_ip_key) << ':'
              << config.get_u16(port_key) << '\n';
}

void config_print_deployment(const Config& config, const RoleSpec& spec)
{
    const EntityId local_client = config.get_entity_id("LOCAL_CLIENT_ID");
    std::cout << "role: " << spec.name << '\n';
    std::cout << "local client id: " << to_string(local_client) << " (0x" << std::hex
              << static_cast<int>(local_client) << std::dec << ")\n";
    config_print_endpoint(config, "AS", "AS_IP", "AS_BIND_IP", "AS_PORT");
    config_print_endpoint(config, "TGS", "TGS_IP", "TGS_BIND_IP", "TGS_PORT");
    config_print_endpoint(config, "V", "V_IP", "V_BIND_IP", "V_PORT");
    if (spec.port_key != nullptr)
    {
        std::cout << "this server listen: " << config.get_string(spec.bind_ip_key) << ':'
                  << config.get_u16(spec.port_key) << '\n';
    }
}

std::string net_format_endpoint(const TcpEndpoint& endpoint)
{
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

TcpEndpoint runtime_bind_endpoint(const Config& config, const RoleSpec& spec)
{
    if (spec.bind_ip_key == nullptr || spec.port_key == nullptr)
    {
        throw std::runtime_error("role does not have a server endpoint");
    }
    return {config.get_string(spec.bind_ip_key), config.get_u16(spec.port_key)};
}

void runtime_handle_server_connection(SocketHandle socket, RoleKind role, RoleSpec spec,
                                      Config config)
{
    try
    {
        if (role == RoleKind::as_server)
        {
            cyber::roles::as::as_process_connection(socket, config);
        }
        else if (role == RoleKind::tgs_server)
        {
            cyber::roles::tgs::tgs_process_connection(socket, config);
        }
        else
        {
            (void)config;
            close_socket(socket);
            throw std::runtime_error("unsupported auth server role");
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << spec.name << " worker failed: " << ex.what() << '\n';
        close_socket(socket);
    }
}

void runtime_run_auth_server(RoleKind role, const Config& config, const RoleSpec& spec,
                             int max_connections)
{
    if (role == RoleKind::client)
    {
        throw std::runtime_error("client cannot run --serve");
    }
    if (role == RoleKind::v_server)
    {
        throw std::runtime_error("v_server uses --game-auth-encrypted in the final runtime");
    }

    SocketRuntime runtime;
    const TcpEndpoint endpoint = runtime_bind_endpoint(config, spec);

    SocketHandle listener = listen_tcp(endpoint);
    std::cout << spec.name << " listening on " << net_format_endpoint(endpoint) << std::endl;
    std::cout << "protocol events: " << log_root_from_config(config).string()
              << "\\protocol_events" << std::endl;

    try
    {
        int accepted_count = 0;
        while (true)
        {
            std::string peer;
            SocketHandle accepted = accept_tcp(listener, &peer);
            ++accepted_count;

            std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
            if (max_connections > 0)
            {
                worker.join();
                if (accepted_count >= max_connections)
                {
                    break;
                }
            }
            else
            {
                worker.detach();
            }
        }
        close_socket(listener);
    }
    catch (...)
    {
        close_socket(listener);
        throw;
    }
}

} // namespace

int run_role_main(RoleKind role, int argc, char** argv)
{
    const RoleSpec spec = runtime_build_role_spec(role);
    bool print_config = false;
    bool serve = false;
    bool game_auth_encrypted = false;
    std::uint16_t ui_port = 0;
    int max_connections = 0;
    std::filesystem::path config_path;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--print-config")
        {
            print_config = true;
        }
        else if (arg == "--serve")
        {
            serve = true;
        }
        else if (arg == "--max-connections")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--max-connections requires a number\n";
                return 2;
            }
            max_connections = std::stoi(argv[++i]);
            if (max_connections <= 0)
            {
                std::cerr << "--max-connections must be positive\n";
                return 2;
            }
        }
        else if (arg == "--game-auth-encrypted")
        {
            game_auth_encrypted = true;
        }
        else if (arg == "--ui-port")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--ui-port requires a port\n";
                return 2;
            }
            ui_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--config")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "--config requires a path\n";
                return 2;
            }
            config_path = argv[++i];
        }
        else if (arg == "--help" || arg == "-h")
        {
            runtime_print_usage(spec);
            return 0;
        }
        else
        {
            std::cerr << "unknown argument: " << arg << '\n';
            runtime_print_usage(spec);
            return 2;
        }
    }

    if (config_path.empty())
    {
        config_path = config_find_default_path();
    }

    try
    {
        const Config config = Config::load(config_path);
        set_protocol_event_log_root(log_root_from_config(config));
        std::cout << spec.name << " role loaded config: " << config_path.string() << '\n';
        std::cout << "entity id: 0x" << std::hex
                  << static_cast<int>(config.get_entity_id(spec.id_key)) << std::dec << '\n';

        if (spec.port_key != nullptr)
        {
            std::cout << "listen port: " << config.get_u16(spec.port_key) << '\n';
        }
        else
        {
            std::cout << "local client id: " << to_string(config.get_entity_id("LOCAL_CLIENT_ID"))
                      << '\n';
        }

        std::cout << "stage goal: " << spec.stage_goal << '\n';

        if (print_config)
        {
            config_print_deployment(config, spec);
            return 0;
        }

        if (game_auth_encrypted)
        {
            if (role == RoleKind::v_server)
            {
                SocketRuntime runtime;
                cyber::game::TankGameServer server(runtime_bind_endpoint(config, spec), config,
                                                    true, true);
                server.run();
                return 0;
            }
            if (role == RoleKind::client)
            {
                if (ui_port == 0)
                {
                    throw std::runtime_error("client --game-auth-encrypted requires --ui-port");
                }
                cyber::game::TankGameClient client(config, ui_port, true);
                client.run();
                return 0;
            }
            throw std::runtime_error(
                "--game-auth-encrypted is only supported by v_server and client");
        }

        if (serve)
        {
            runtime_run_auth_server(role, config, spec, max_connections);
        }
        else
        {
            runtime_print_usage(spec);
            throw std::runtime_error("no runtime mode selected");
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << spec.name << " failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
} // namespace cyber
