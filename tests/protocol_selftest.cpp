#include "cyber/common/config.hpp"
#include "cyber/common/packet.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        cyber::Packet packet;
        packet.msg_type = cyber::MsgType::app;
        packet.src = cyber::EntityId::client1;
        packet.dst = cyber::EntityId::v;
        packet.reserved = 0x01020304U;
        packet.payload = cyber::make_app_payload(cyber::AppCode::key_down,
                                                 cyber::Bytes{static_cast<std::uint8_t>('W')});

        const cyber::Bytes encoded = cyber::serialize_packet(packet);
        require(encoded.size() == cyber::kPacketHeaderSize + packet.payload.size(),
                "encoded size mismatch");
        require(encoded[3] == 0x00 && encoded[4] == 0x00 && encoded[5] == 0x00 &&
                    encoded[6] == packet.payload.size(),
                "payload_len encoding mismatch");
        require(encoded[7] == 0x01 && encoded[8] == 0x02 && encoded[9] == 0x03 &&
                    encoded[10] == 0x04,
                "reserved encoding mismatch");
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

        const std::string hex = cyber::bytes_to_hex(encoded);
        require(cyber::bytes_from_hex(hex) == encoded, "hex roundtrip mismatch");

        const std::filesystem::path config_path = "config/course_config.txt";
        const cyber::Config config = cyber::Config::load(config_path);
        require(config.get_entity_id("AS_ID") == cyber::EntityId::as, "AS_ID mismatch");
        require(config.get_entity_id("TGS_ID") == cyber::EntityId::tgs, "TGS_ID mismatch");
        require(config.get_entity_id("V_ID") == cyber::EntityId::v, "V_ID mismatch");
        require(config.get_u16("AS_PORT") == 9001, "AS_PORT mismatch");
        require(config.get_string("AS_BIND_IP") == "0.0.0.0", "AS_BIND_IP mismatch");
        require(config.get_string("AS_IP") == config.get_string("AS_HOST"),
                "AS_IP alias mismatch");
        require(config.get_entity_id("LOCAL_CLIENT_ID") == cyber::EntityId::client1,
                "LOCAL_CLIENT_ID mismatch");
        require(config.clients().size() == 4U, "client table mismatch");

        require(cyber::to_string(cyber::AppCode::game_join_req) == "GAME_JOIN_REQ",
                "GAME_JOIN_REQ string mismatch");
        require(cyber::to_string(cyber::AppCode::game_start) == "GAME_START",
                "GAME_START string mismatch");
        require(cyber::to_string(cyber::AppCode::game_state) == "GAME_STATE",
                "GAME_STATE string mismatch");
        require(cyber::to_string(cyber::AppCode::app_ack) == "APP_ACK",
                "APP_ACK string mismatch");

        std::cout << "protocol_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "protocol_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
