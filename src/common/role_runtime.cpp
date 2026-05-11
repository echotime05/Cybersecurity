#include "cyber/common/role_runtime.hpp"

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
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

RoleSpec spec_for(RoleKind role)
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

void print_usage(const RoleSpec& spec)
{
    std::cout << "Usage: " << spec.binary
              << " [--config PATH] [--self-test] [--print-config] [--serve] [--once]"
                 " [--max-connections N] [--connect-test]\n";
}

std::filesystem::path find_default_config()
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

void run_common_self_test(const Config& config, const RoleSpec& spec)
{
    const EntityId configured_id = config.get_entity_id(spec.id_key);
    if (spec.id != EntityId::unknown && configured_id != spec.id)
    {
        throw std::runtime_error(std::string("unexpected id for role ") + spec.name);
    }

    Packet packet;
    packet.msg_type = MsgType::app;
    packet.src = EntityId::client1;
    packet.dst = EntityId::v;
    packet.payload = make_app_payload(AppCode::app_ack, Bytes{0x00, 0x00, 0x00, 0x01});

    const Bytes encoded = serialize_packet(packet);
    const Packet decoded = parse_packet(encoded);
    if (decoded.msg_type != packet.msg_type || decoded.src != packet.src ||
        decoded.dst != packet.dst || decoded.payload != packet.payload)
    {
        throw std::runtime_error("packet roundtrip failed");
    }

    if (parse_app_code(decoded.payload) != AppCode::app_ack)
    {
        throw std::runtime_error("app payload parse failed");
    }

    const auto clients = config.clients();
    if (clients.size() != 4U)
    {
        throw std::runtime_error("client secret table is incomplete");
    }

    const EntityId local_client = config.get_entity_id("LOCAL_CLIENT_ID");
    bool local_client_found = false;
    for (const ClientSecret& client : clients)
    {
        if (client.id == local_client)
        {
            local_client_found = true;
            break;
        }
    }
    if (!local_client_found)
    {
        throw std::runtime_error("LOCAL_CLIENT_ID is not one of C1_ID..C4_ID");
    }
}

void print_endpoint(const Config& config, const char* name, const char* ip_key,
                    const char* bind_ip_key, const char* port_key)
{
    std::cout << name << " connect: " << config.get_string(ip_key) << ':'
              << config.get_u16(port_key) << '\n';
    std::cout << name << " listen:  " << config.get_string(bind_ip_key) << ':'
              << config.get_u16(port_key) << '\n';
}

void print_deployment_config(const Config& config, const RoleSpec& spec)
{
    const EntityId local_client = config.get_entity_id("LOCAL_CLIENT_ID");
    std::cout << "role: " << spec.name << '\n';
    std::cout << "local client id: " << to_string(local_client) << " (0x" << std::hex
              << static_cast<int>(local_client) << std::dec << ")\n";
    print_endpoint(config, "AS", "AS_IP", "AS_BIND_IP", "AS_PORT");
    print_endpoint(config, "TGS", "TGS_IP", "TGS_BIND_IP", "TGS_PORT");
    print_endpoint(config, "V", "V_IP", "V_BIND_IP", "V_PORT");
    if (spec.port_key != nullptr)
    {
        std::cout << "this server listen: " << config.get_string(spec.bind_ip_key) << ':'
                  << config.get_u16(spec.port_key) << '\n';
    }
}

std::string client_log_filename(EntityId client_id)
{
    switch (client_id)
    {
    case EntityId::client1:
        return "client_01.log";
    case EntityId::client2:
        return "client_02.log";
    case EntityId::client3:
        return "client_03.log";
    case EntityId::client4:
        return "client_04.log";
    default:
        throw std::runtime_error("LOCAL_CLIENT_ID is not a client id");
    }
}

std::filesystem::path role_log_path(RoleKind role, const Config& config)
{
    const std::filesystem::path log_dir = "logs";
    switch (role)
    {
    case RoleKind::as_server:
        return log_dir / "as.log";
    case RoleKind::tgs_server:
        return log_dir / "tgs.log";
    case RoleKind::v_server:
        return log_dir / "v.log";
    case RoleKind::client:
        return log_dir / client_log_filename(config.get_entity_id("LOCAL_CLIENT_ID"));
    default:
        throw std::runtime_error("unknown role");
    }
}

std::string main_thread_name(RoleKind role)
{
    switch (role)
    {
    case RoleKind::as_server:
        return "ASMainThread";
    case RoleKind::tgs_server:
        return "TGSMainThread";
    case RoleKind::v_server:
        return "VMainThread";
    case RoleKind::client:
        return "UI/GameThread";
    default:
        throw std::runtime_error("unknown role");
    }
}

void run_logged_self_test(RoleKind role, const Config& config, const RoleSpec& spec)
{
    Logger logger(role_log_path(role, config));
    const std::string thread_name = main_thread_name(role);
    logger.write(spec.name, thread_name, "THREAD_START", "self-test start");
    logger.write(spec.name, thread_name, "LOG", "log file=" + logger.path().generic_string());
    try
    {
        run_common_self_test(config, spec);
        logger.write(spec.name, thread_name, "THREAD_EXIT", "self-test exit");
        std::cout << "log file: " << logger.path().string() << '\n';
        std::cout << "self-test: ok\n";
    }
    catch (const std::exception& ex)
    {
        logger.write(spec.name, thread_name, "ERROR", ex.what());
        logger.write(spec.name, thread_name, "THREAD_EXIT", "self-test failed");
        throw;
    }
}

Bytes text_payload(const std::string& text)
{
    return Bytes(text.begin(), text.end());
}

std::string endpoint_text(const TcpEndpoint& endpoint)
{
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

TcpEndpoint bind_endpoint(const Config& config, const RoleSpec& spec)
{
    if (spec.bind_ip_key == nullptr || spec.port_key == nullptr)
    {
        throw std::runtime_error("role does not have a server endpoint");
    }
    return {config.get_string(spec.bind_ip_key), config.get_u16(spec.port_key)};
}

TcpEndpoint connect_endpoint(const Config& config, const char* ip_key, const char* port_key)
{
    return {config.get_string(ip_key), config.get_u16(port_key)};
}

std::string worker_thread_name(RoleKind role)
{
    switch (role)
    {
    case RoleKind::as_server:
        return "ASWorker-Probe";
    case RoleKind::tgs_server:
        return "TGSWorker-Probe";
    case RoleKind::v_server:
        return "VWorker-Probe";
    default:
        throw std::runtime_error("client does not have a server worker");
    }
}

Packet make_probe_response(RoleKind role, const RoleSpec& spec, const Packet& request)
{
    if (role == RoleKind::as_server && request.msg_type == MsgType::as_req)
    {
        return make_packet(MsgType::as_rep, spec.id, request.src, text_payload("probe_as_rep"));
    }
    if (role == RoleKind::tgs_server && request.msg_type == MsgType::tgs_req)
    {
        return make_packet(MsgType::tgs_rep, spec.id, request.src, text_payload("probe_tgs_rep"));
    }
    if (role == RoleKind::v_server && request.msg_type == MsgType::v_auth_req)
    {
        return make_packet(MsgType::v_auth_rep, spec.id, request.src,
                           text_payload("probe_v_auth_rep"));
    }
    if (role == RoleKind::v_server && request.msg_type == MsgType::cert_c2v)
    {
        return make_packet(MsgType::cert_v2c, spec.id, request.src, text_payload("probe_cert_v2c"));
    }
    if (role == RoleKind::v_server && request.msg_type == MsgType::app)
    {
        return make_packet(MsgType::app, spec.id, request.src,
                           make_app_payload(AppCode::app_ack, Bytes{0x01}));
    }

    return make_packet(MsgType::error, spec.id, request.src,
                       make_error_payload(ErrorCode::unsupported_msg_type, "probe unsupported msg_type"));
}

void handle_probe_connection(SocketHandle socket, std::shared_ptr<Logger> logger, RoleKind role,
                             RoleSpec spec)
{
    const std::string thread_name = worker_thread_name(role);
    logger->write(spec.name, thread_name, "THREAD_START", thread_name + " start");
    try
    {
        const Packet request = recv_packet_logged(socket, *logger, spec.name, thread_name);
        const Packet response = make_probe_response(role, spec, request);
        if (response.msg_type == MsgType::error)
        {
            logger->write(spec.name, thread_name, "ERROR", "ERR_UNSUPPORTED_MSG_TYPE");
        }
        send_packet_logged(socket, response, *logger, spec.name, thread_name);
        logger->write(spec.name, thread_name, "SOCKET_CLOSE", "close probe connection");
        close_socket(socket);
        logger->write(spec.name, thread_name, "THREAD_EXIT", thread_name + " exit");
    }
    catch (const std::exception& ex)
    {
        logger->write(spec.name, thread_name, "ERROR", ex.what());
        close_socket(socket);
        logger->write(spec.name, thread_name, "THREAD_EXIT", thread_name + " failed");
    }
}

void run_server(RoleKind role, const Config& config, const RoleSpec& spec, int max_connections)
{
    if (role == RoleKind::client)
    {
        throw std::runtime_error("client cannot run --serve");
    }

    SocketRuntime runtime;
    auto logger = std::make_shared<Logger>(role_log_path(role, config));
    const std::string main_thread = main_thread_name(role);
    const TcpEndpoint endpoint = bind_endpoint(config, spec);
    logger->write(spec.name, main_thread, "THREAD_START",
                  main_thread + " start listen=" + endpoint_text(endpoint));

    SocketHandle listener = listen_tcp(endpoint);
    std::cout << spec.name << " listening on " << endpoint_text(endpoint) << std::endl;
    std::cout << "log file: " << logger->path().string() << std::endl;

    try
    {
        int accepted_count = 0;
        while (true)
        {
            std::string peer;
            SocketHandle accepted = accept_tcp(listener, &peer);
            ++accepted_count;
            logger->write(spec.name, main_thread, "ACCEPT", "accept connection from " + peer);

            std::thread worker(handle_probe_connection, accepted, logger, role, spec);
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
        logger->write(spec.name, main_thread, "THREAD_EXIT", main_thread + " exit");
    }
    catch (...)
    {
        close_socket(listener);
        logger->write(spec.name, main_thread, "THREAD_EXIT", main_thread + " failed");
        throw;
    }
}

void run_client_probe_exchange(Logger& logger, EntityId local_client, const std::string& name,
                               const TcpEndpoint& endpoint,
                               MsgType request_type, MsgType expected_response,
                               const std::string& auth_state)
{
    const std::string thread_name = "CAuthWorker";
    logger.write("Client", thread_name, "CONNECT", "connect to " + name + " " + endpoint_text(endpoint));
    SocketHandle socket = connect_tcp(endpoint);
    try
    {
        const EntityId dst =
            (name == "AS") ? EntityId::as : ((name == "TGS") ? EntityId::tgs : EntityId::v);
        const Packet request =
            make_packet(request_type, local_client, dst, text_payload("probe_" + name));
        send_packet_logged(socket, request, logger, "Client", thread_name);
        const Packet response = recv_packet_logged(socket, logger, "Client", thread_name);
        if (response.msg_type != expected_response)
        {
            throw std::runtime_error("unexpected response from " + name);
        }
        logger.write("Client", thread_name, "AUTH_STATE", auth_state);
        logger.write("Client", thread_name, "SOCKET_CLOSE", "close " + name + " probe connection");
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}

void run_client_connect_test(const Config& config)
{
    SocketRuntime runtime;
    Logger logger(role_log_path(RoleKind::client, config));
    const EntityId local_client = config.get_entity_id("LOCAL_CLIENT_ID");
    const std::string thread_name = "CAuthWorker";
    logger.write("Client", thread_name, "THREAD_START", "connect-test start");

    run_client_probe_exchange(logger, local_client, "AS", connect_endpoint(config, "AS_IP", "AS_PORT"),
                              MsgType::as_req,
                              MsgType::as_rep, "AS_OK");
    run_client_probe_exchange(logger, local_client, "TGS",
                              connect_endpoint(config, "TGS_IP", "TGS_PORT"), MsgType::tgs_req,
                              MsgType::tgs_rep, "TGS_OK");
    run_client_probe_exchange(logger, local_client, "V",
                              connect_endpoint(config, "V_IP", "V_PORT"), MsgType::v_auth_req,
                              MsgType::v_auth_rep, "V_AUTH_OK");
    run_client_probe_exchange(logger, local_client, "V",
                              connect_endpoint(config, "V_IP", "V_PORT"), MsgType::cert_c2v,
                              MsgType::cert_v2c, "AUTH_DONE");

    logger.write("Client", thread_name, "THREAD_EXIT", "connect-test exit");
    std::cout << "connect-test: ok\n";
    std::cout << "log file: " << logger.path().string() << '\n';
}
} // namespace

int run_role_main(RoleKind role, int argc, char** argv)
{
    const RoleSpec spec = spec_for(role);
    bool self_test = false;
    bool print_config = false;
    bool serve = false;
    bool once = false;
    bool connect_test = false;
    int max_connections = 0;
    std::filesystem::path config_path;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--self-test")
        {
            self_test = true;
        }
        else if (arg == "--print-config")
        {
            print_config = true;
        }
        else if (arg == "--serve")
        {
            serve = true;
        }
        else if (arg == "--once")
        {
            once = true;
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
        else if (arg == "--connect-test")
        {
            connect_test = true;
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
            print_usage(spec);
            return 0;
        }
        else
        {
            std::cerr << "unknown argument: " << arg << '\n';
            print_usage(spec);
            return 2;
        }
    }

    if (config_path.empty())
    {
        config_path = find_default_config();
    }

    try
    {
        const Config config = Config::load(config_path);
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
            print_deployment_config(config, spec);
            if (!self_test)
            {
                return 0;
            }
        }

        if (self_test)
        {
            run_logged_self_test(role, config, spec);
        }
        else if (serve)
        {
            run_server(role, config, spec, once ? 1 : max_connections);
        }
        else if (connect_test)
        {
            if (role != RoleKind::client)
            {
                throw std::runtime_error("--connect-test is only supported by client");
            }
            run_client_connect_test(config);
        }
        else
        {
            std::cout << "network loop will be implemented in the next stage\n";
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
