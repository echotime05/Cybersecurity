#include "cyber/common/config.hpp"
#include "cyber/common/packet.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

template <typename Fn>
void require_packet_error(Fn&& fn, const char* message)
{
    try
    {
        fn();
    }
    catch (const cyber::PacketError&)
    {
        return;
    }
    throw std::runtime_error(message);
}
} // namespace

int main()
{
    try
    {
        const std::array<cyber::MsgType, 10> msg_types = {
            cyber::MsgType::as_req,      cyber::MsgType::as_rep,
            cyber::MsgType::tgs_req,     cyber::MsgType::tgs_rep,
            cyber::MsgType::v_auth_req,  cyber::MsgType::v_auth_rep,
            cyber::MsgType::cert_c2v,    cyber::MsgType::cert_v2c,
            cyber::MsgType::error,       cyber::MsgType::app};
        const std::array<std::string, 10> msg_names = {
            "MSG_AS_REQ",      "MSG_AS_REP",      "MSG_TGS_REQ",   "MSG_TGS_REP",
            "MSG_V_AUTH_REQ", "MSG_V_AUTH_REP", "MSG_CERT_C2V",  "MSG_CERT_V2C",
            "MSG_ERROR",      "MSG_APP"};
        for (std::size_t i = 0; i < msg_types.size(); ++i)
        {
            require(cyber::is_known(msg_types[i]), "known msg type not recognized");
            require(std::string(cyber::to_string(msg_types[i])) == msg_names[i],
                    "msg type string mismatch");
        }
        require(!cyber::is_known(static_cast<cyber::MsgType>(0xEE)),
                "unknown msg type unexpectedly recognized");

        const std::array<cyber::AppCode, 11> app_codes = {
            cyber::AppCode::key_down,      cyber::AppCode::key_up,
            cyber::AppCode::aim_event,     cyber::AppCode::fire_event,
            cyber::AppCode::game_join_req, cyber::AppCode::game_start,
            cyber::AppCode::game_state,    cyber::AppCode::app_ack,
            cyber::AppCode::game_move,     cyber::AppCode::game_target,
            cyber::AppCode::game_shoot};
        const std::array<std::string, 11> app_names = {
            "KEY_DOWN", "KEY_UP", "AIM_EVENT", "FIRE_EVENT", "GAME_JOIN_REQ",
            "GAME_START", "GAME_STATE", "APP_ACK", "GAME_MOVE", "GAME_TARGET",
            "GAME_SHOOT"};
        for (std::size_t i = 0; i < app_codes.size(); ++i)
        {
            require(cyber::is_known(app_codes[i]), "known app code not recognized");
            require(std::string(cyber::to_string(app_codes[i])) == app_names[i],
                    "app code string mismatch");
        }
        require(!cyber::is_known(static_cast<cyber::AppCode>(0xEE)),
                "unknown app code unexpectedly recognized");

        const std::array<cyber::ErrorCode, 7> error_codes = {
            cyber::ErrorCode::password_wrong,    cyber::ErrorCode::tgt_expired,
            cyber::ErrorCode::ticket_v_expired,  cyber::ErrorCode::tgs_id_mismatch,
            cyber::ErrorCode::v_id_mismatch,     cyber::ErrorCode::replay_detected,
            cyber::ErrorCode::unsupported_msg_type};
        const std::array<std::string, 7> error_names = {
            "ERR_PASSWORD_WRONG",      "ERR_TGT_EXPIRED",      "ERR_TICKET_V_EXPIRED",
            "ERR_TGS_ID_MISMATCH",    "ERR_V_ID_MISMATCH",    "ERR_REPLAY_DETECTED",
            "ERR_UNSUPPORTED_MSG_TYPE"};
        for (std::size_t i = 0; i < error_codes.size(); ++i)
        {
            require(cyber::is_known(error_codes[i]), "known error code not recognized");
            require(std::string(cyber::to_string(error_codes[i])) == error_names[i],
                    "error code string mismatch");
        }
        require(!cyber::is_known(static_cast<cyber::ErrorCode>(0xEE)),
                "unknown error code unexpectedly recognized");

        require(cyber::is_client(cyber::EntityId::client1), "client id not recognized");
        require(cyber::is_server(cyber::EntityId::as), "server id not recognized");
        require(!cyber::is_known(cyber::EntityId::unknown),
                "unknown entity id unexpectedly recognized");

        cyber::Packet packet = cyber::make_packet(
            cyber::MsgType::app, cyber::EntityId::client1, cyber::EntityId::v,
            cyber::make_app_payload(cyber::AppCode::key_down,
                                    cyber::Bytes{static_cast<std::uint8_t>('W')}),
            0x01020304U);
        const cyber::PacketHeader direct_header = cyber::packet_header(packet);
        require(direct_header.msg_type == cyber::MsgType::app, "direct header msg_type mismatch");
        require(direct_header.src == cyber::EntityId::client1, "direct header src mismatch");
        require(direct_header.dst == cyber::EntityId::v, "direct header dst mismatch");
        require(direct_header.payload_len == packet.payload.size(),
                "direct header payload_len mismatch");
        require(direct_header.reserved == 0x01020304U, "direct header reserved mismatch");

        const cyber::Bytes encoded = cyber::serialize_packet(packet);
        require(encoded.size() == cyber::kPacketHeaderSize + packet.payload.size(),
                "encoded size mismatch");
        require(encoded[3] == 0x00 && encoded[4] == 0x00 && encoded[5] == 0x00 &&
                    encoded[6] == packet.payload.size(),
                "payload_len encoding mismatch");
        require(encoded[7] == 0x01 && encoded[8] == 0x02 && encoded[9] == 0x03 &&
                    encoded[10] == 0x04,
                "reserved encoding mismatch");
        const cyber::PacketHeader parsed_header = cyber::parse_packet_header(encoded);
        require(parsed_header.msg_type == packet.msg_type, "parsed header msg_type mismatch");
        require(parsed_header.src == packet.src, "parsed header src mismatch");
        require(parsed_header.dst == packet.dst, "parsed header dst mismatch");
        require(parsed_header.payload_len == packet.payload.size(),
                "parsed header payload_len mismatch");
        require(parsed_header.reserved == packet.reserved, "parsed header reserved mismatch");

        const cyber::Packet decoded = cyber::parse_packet(encoded);
        require(decoded.msg_type == packet.msg_type, "msg_type mismatch");
        require(decoded.src == packet.src, "src mismatch");
        require(decoded.dst == packet.dst, "dst mismatch");
        require(decoded.reserved == packet.reserved, "reserved mismatch");
        require(decoded.payload == packet.payload, "payload mismatch");
        require(cyber::parse_app_code(decoded.payload) == cyber::AppCode::key_down,
                "app_code mismatch");
        require(cyber::parse_app_payload(decoded.payload) == cyber::Bytes{static_cast<std::uint8_t>('W')},
                "app_payload mismatch");

        const cyber::Bytes error_payload =
            cyber::make_error_payload(cyber::ErrorCode::unsupported_msg_type, "bad msg_type");
        require(cyber::parse_error_code(error_payload) ==
                    cyber::ErrorCode::unsupported_msg_type,
                "error code mismatch");
        require(cyber::parse_error_message(error_payload) == "bad msg_type",
                "error message mismatch");

        const std::string hex = cyber::bytes_to_hex(encoded);
        require(cyber::bytes_from_hex(hex) == encoded, "hex roundtrip mismatch");
        require(cyber::bytes_from_hex("0x" + hex) == encoded, "0x hex roundtrip mismatch");
        require(cyber::bytes_from_hex("f") == cyber::Bytes{0x0F}, "odd hex parse mismatch");

        require_packet_error([]() { cyber::parse_packet(cyber::Bytes{0x01, 0x02}); },
                             "short packet did not fail");
        require_packet_error([]() { cyber::parse_packet_header(cyber::Bytes{0x01, 0x02}); },
                             "short header did not fail");
        require_packet_error([]() { cyber::parse_app_code(cyber::Bytes{}); },
                             "empty app payload did not fail");
        require_packet_error([]() { cyber::parse_app_payload(cyber::Bytes{}); },
                             "empty app payload body did not fail");
        require_packet_error([]() { cyber::parse_error_code(cyber::Bytes{}); },
                             "empty error payload did not fail");
        require_packet_error([]() { cyber::parse_error_message(cyber::Bytes{}); },
                             "empty error message payload did not fail");
        require_packet_error([]() { cyber::bytes_from_hex("0xzz"); },
                             "invalid hex did not fail");

        cyber::Bytes mismatched = encoded;
        mismatched[6] = static_cast<std::uint8_t>(mismatched[6] + 1U);
        require_packet_error([&]() { cyber::parse_packet(mismatched); },
                             "payload length mismatch did not fail");

        const std::filesystem::path config_path = "config/course_config.txt";
        const cyber::Config config = cyber::Config::load(config_path);
        require(config.get_entity_id("AS_ID") == cyber::EntityId::as, "AS_ID mismatch");
        require(config.get_entity_id("TGS_ID") == cyber::EntityId::tgs, "TGS_ID mismatch");
        require(config.get_entity_id("V_ID") == cyber::EntityId::v, "V_ID mismatch");
        require(config.get_u16("AS_PORT") == 9001, "AS_PORT mismatch");
        const std::string as_bind_ip = config.get_string("AS_BIND_IP");
        require(as_bind_ip == "127.0.0.1" || as_bind_ip == "0.0.0.0",
                "AS_BIND_IP must be loopback or wildcard");
        require(config.get_string("AS_IP") == config.get_string("AS_HOST"),
                "AS_IP alias mismatch");
        require(cyber::is_client(config.get_entity_id("LOCAL_CLIENT_ID")),
                "LOCAL_CLIENT_ID is not a client id");
        require(config.clients().size() == 4U, "client table mismatch");

        std::cout << "protocol_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "protocol_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
