#include "cyber/common/role_runtime.hpp"

#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/packet.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
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
              << " [--config PATH] [--self-test] [--print-config]\n";
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
} // namespace

int run_role_main(RoleKind role, int argc, char** argv)
{
    const RoleSpec spec = spec_for(role);
    bool self_test = false;
    bool print_config = false;
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
