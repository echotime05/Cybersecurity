#include "cyber/shared/role_runtime.hpp"

#include "cyber/protocol/protocol_event.hpp"
#include "cyber/roles/as/as_service.hpp"
#include "cyber/roles/client/tank_game_client.hpp"
#include "cyber/roles/tgs/tgs_service.hpp"
#include "cyber/roles/v/tank_game_server.hpp"
#include "cyber/shared/config.hpp"
#include "cyber/shared/net_packet.hpp"
#include "cyber/shared/net_socket.hpp"
#include "cyber/shared/runtime_paths.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace cyber
{
namespace
{
// 描述一个最终运行角色需要读取的配置项。
struct RoleSpec
{
    const char* name;
    const char* id_key;
    const char* port_key;
    const char* bind_ip_key;
};

// 保存 README 启动命令中实际会使用的参数。
struct RoleOptions
{
    bool serve = false;
    bool game_auth_encrypted = false;
    std::uint16_t ui_port = 0;
    int max_connections = 0;
    std::filesystem::path config_path;
};

// 根据角色枚举返回配置键名。
RoleSpec runtime_build_role_spec(RoleKind role)
{
    switch (role)
    {
    case RoleKind::as_server:
        return {"AS", "AS_ID", "AS_PORT", "AS_BIND_IP"};
    case RoleKind::tgs_server:
        return {"TGS", "TGS_ID", "TGS_PORT", "TGS_BIND_IP"};
    case RoleKind::v_server:
        return {"V", "V_ID", "V_PORT", "V_BIND_IP"};
    case RoleKind::client:
        return {"Client", "LOCAL_CLIENT_ID", nullptr, nullptr};
    default:
        throw std::runtime_error("unknown role");
    }
}

// 判断角色是否是短连接认证服务器。
bool runtime_is_auth_server(RoleKind role)
{
    return role == RoleKind::as_server || role == RoleKind::tgs_server;
}

// 生成命令行参数缺少取值时的错误信息。
std::string runtime_missing_value_message(const std::string& arg)
{
    return arg + " requires a value";
}

// 解析最终链路保留的参数：--config、--serve、--game-auth-encrypted、--ui-port。
RoleOptions runtime_parse_options(int argc, char** argv)
{
    RoleOptions options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--config")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error(runtime_missing_value_message(arg));
            }
            options.config_path = argv[++i];
        }
        else if (arg == "--serve")
        {
            options.serve = true;
        }
        else if (arg == "--game-auth-encrypted")
        {
            options.game_auth_encrypted = true;
        }
        else if (arg == "--ui-port")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error(runtime_missing_value_message(arg));
            }
            options.ui_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--max-connections")
        {
            if (i + 1 >= argc)
            {
                throw std::runtime_error(runtime_missing_value_message(arg));
            }
            options.max_connections = std::stoi(argv[++i]);
            if (options.max_connections <= 0)
            {
                throw std::runtime_error("--max-connections must be positive");
            }
        }
        else
        {
            throw std::runtime_error("unsupported final-runtime argument: " + arg);
        }
    }

    if (options.config_path.empty())
    {
        throw std::runtime_error("missing --config PATH");
    }
    return options;
}

// 校验角色是否按 README 的最终方式启动。
void runtime_validate_role_mode(RoleKind role, const RoleOptions& options)
{
    if (runtime_is_auth_server(role))
    {
        if (!options.serve || options.game_auth_encrypted || options.ui_port != 0)
        {
            throw std::runtime_error("AS/TGS must start with: --config PATH --serve");
        }
        return;
    }

    if (!options.game_auth_encrypted || options.serve || options.max_connections != 0)
    {
        throw std::runtime_error("V/Client must start with --game-auth-encrypted");
    }
    if (role == RoleKind::client && options.ui_port == 0)
    {
        throw std::runtime_error("client requires --ui-port PORT");
    }
    if (role == RoleKind::v_server && options.ui_port != 0)
    {
        throw std::runtime_error("v_server does not use --ui-port");
    }
}

// 将 TCP 端点格式化为 ip:port。
std::string net_format_endpoint(const TcpEndpoint& endpoint)
{
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

// 根据角色配置生成服务端监听端点。
TcpEndpoint runtime_bind_endpoint(const Config& config, const RoleSpec& spec)
{
    if (spec.bind_ip_key == nullptr || spec.port_key == nullptr)
    {
        throw std::runtime_error("role does not have a server endpoint");
    }
    return {config.get_string(spec.bind_ip_key), config.get_u16(spec.port_key)};
}

// 处理 AS/TGS 的单个短连接请求。
void runtime_handle_server_connection(SocketHandle socket, RoleKind role, RoleSpec spec,
                                      Config config)
{
    try
    {
        // 分派AS请求或TGS请求
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

// 运行 AS 或 TGS 的短连接认证服务循环。（实现多线程）
void runtime_run_auth_server(RoleKind role, const Config& config, const RoleSpec& spec,
                             int max_connections)
{

    // 角色校验，确保当前进程的角色是AS或TGS服务器
    if (!runtime_is_auth_server(role))
    {
        throw std::runtime_error("only AS/TGS can run --serve");
    }

    /**
     * 网络与套接字初始化
     * 功能：准备接收网络连接
     * 根据传入的配置（config和spec）解析出绑定IP和端口
     */
    SocketRuntime runtime;
    const TcpEndpoint endpoint = runtime_bind_endpoint(config, spec);

    // listen_tcp创建一个TCP监听套接字
    SocketHandle listener = listen_tcp(endpoint);

    // 在控制台打印服务器开始监听的地址和日志输出路径。
    std::cout << spec.name << " listening on " << net_format_endpoint(endpoint) << std::endl;
    std::cout << "protocol events: " << log_root_from_config(config).string()
              << "\\protocol_events" << std::endl;

    // 主时间循环与并发处理
    /**
     * 功能：不断接收客户端连接并派发任务，每收到一个连接，就创建一个新的线程来处理这个连接，调用runtime_handle_server_connection函数处理该线程，主线程继续监听新的连接请求。
     */
    try
    {
        int accepted_count = 0;
        while (true)
        {

            std::string peer;
            SocketHandle accepted = accept_tcp(listener, &peer);
            ++accepted_count;

            std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);

            // 连接数限制和线程管理
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

// 启动 V 或 Client 的加密游戏链路。
void runtime_run_encrypted_game_role(RoleKind role, const Config& config, const RoleSpec& spec,
                                     std::uint16_t ui_port)
{
    if (role == RoleKind::v_server)
    {
        SocketRuntime runtime;
        cyber::game::TankGameServer server(runtime_bind_endpoint(config, spec), config);
        server.run();
        return;
    }

    if (role == RoleKind::client)
    {
        cyber::game::TankGameClient client(config, ui_port);
        client.run();
        return;
    }

    throw std::runtime_error("only V/Client can run --game-auth-encrypted");
}

} // namespace

// 统一角色 main 入口：只保留 README 最终启动方式。
int run_role_main(RoleKind role, int argc, char** argv)
{
    const RoleSpec spec = runtime_build_role_spec(role);

    try
    {
        const RoleOptions options = runtime_parse_options(argc, argv);
        runtime_validate_role_mode(role, options);

        const Config config = Config::load(options.config_path);
        set_protocol_event_log_root(log_root_from_config(config));
        std::cout << spec.name << " role loaded config: " << options.config_path.string() << '\n';
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

        if (runtime_is_auth_server(role))
        {
            runtime_run_auth_server(role, config, spec, options.max_connections);
        }
        else
        {
            runtime_run_encrypted_game_role(role, config, spec, options.ui_port);
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
