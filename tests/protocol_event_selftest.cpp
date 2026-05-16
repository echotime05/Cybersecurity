#include "cyber/common/protocol_event.hpp"

#include "cyber/common/packet.hpp"

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
} // namespace

int main()
{
    try
    {
        const cyber::Packet app =
            cyber::make_packet(cyber::MsgType::app, cyber::EntityId::client1,
                               cyber::EntityId::v, cyber::Bytes{0xAA, 0xBB});
        const std::string app_message = cyber::format_protocol_event_message(
            cyber::ProtocolDirection::send, app, "MSG_APP.GAME_MOVE", "12:03:15.044");
        require(app_message ==
                    "ts=12:03:15.044 direction=SEND endpoint=Client1->V "
                    "message=MSG_APP.GAME_MOVE category=app msg_type=MSG_APP "
                    "msg_type_raw=0x66 src=Client1 src_raw=0x01 dst=V dst_raw=0x13 "
                    "payload_len=2 payload_len_raw=0x00000002 reserved=0 "
                    "reserved_raw=0x00000000 payload_hex=aabb",
                "protocol event message mismatch");

        const cyber::ProtocolEvent parsed = cyber::parse_protocol_event_line(
            "[Client1][ProtocolMonitor][PACKET_SEND] " + app_message);
        require(parsed.role == "Client1", "parsed role mismatch");
        require(parsed.direction == "SEND", "parsed direction mismatch");
        require(parsed.endpoint == "Client1->V", "parsed endpoint mismatch");
        require(parsed.message == "MSG_APP.GAME_MOVE", "parsed message mismatch");
        require(parsed.category == "app", "parsed category mismatch");
        require(parsed.header.msg_type.label == "MSG_APP", "parsed msg type label mismatch");
        require(parsed.header.msg_type.raw == "0x66", "parsed msg type raw mismatch");
        require(parsed.header.src.label == "Client1", "parsed src label mismatch");
        require(parsed.header.dst.label == "V", "parsed dst label mismatch");
        require(parsed.header.payload_len.label == "2", "parsed payload len mismatch");
        require(parsed.header.payload_len.raw == "0x00000002",
                "parsed payload len raw mismatch");
        require(parsed.payload_hex == "aabb", "parsed payload hex mismatch");

        const std::string json = cyber::protocol_event_json(parsed, 7);
        require(json.find("\"type\":\"protocolEvent\"") != std::string::npos,
                "json type missing");
        require(json.find("\"id\":7") != std::string::npos, "json id missing");
        require(json.find("\"message\":\"MSG_APP.GAME_MOVE\"") != std::string::npos,
                "json message missing");
        require(json.find("\"category\":\"app\"") != std::string::npos,
                "json category missing");
        require(json.find("\"payloadHex\":\"aabb\"") != std::string::npos,
                "json payload missing");

        cyber::ProtocolPayloadView payload_view;
        payload_view.plain_hex = "010203";
        payload_view.encrypted_hex = "aabbccdd";
        const std::string encrypted_message = cyber::format_protocol_event_message(
            cyber::ProtocolDirection::send, app, "MSG_APP.GAME_MOVE", "12:03:15.045",
            payload_view);
        require(encrypted_message.find("payload_plain_hex=010203") != std::string::npos,
                "plain payload view missing");
        require(encrypted_message.find("payload_encrypted_hex=aabbccdd") != std::string::npos,
                "encrypted payload view missing");

        const cyber::ProtocolEvent parsed_encrypted = cyber::parse_protocol_event_line(
            "[Client1][ProtocolMonitor][PACKET_SEND] " + encrypted_message);
        require(parsed_encrypted.payload_plain_hex == "010203",
                "parsed plain payload view mismatch");
        require(parsed_encrypted.payload_encrypted_hex == "aabbccdd",
                "parsed encrypted payload view mismatch");

        const std::string encrypted_json = cyber::protocol_event_json(parsed_encrypted, 8);
        require(encrypted_json.find("\"payloadPlainHex\":\"010203\"") != std::string::npos,
                "json plain payload view missing");
        require(encrypted_json.find("\"payloadEncryptedHex\":\"aabbccdd\"") != std::string::npos,
                "json encrypted payload view missing");

        cyber::ProtocolPayloadView kerberos_view;
        kerberos_view.fields.push_back(
            {"ticket_tgs", "010203040506", "a1b2c3d4"});
        const std::string kerberos_message = cyber::format_protocol_event_message(
            cyber::ProtocolDirection::send, app, "MSG_TGS_REQ", "12:03:17.000",
            kerberos_view);
        require(kerberos_message.find("field_count=1") != std::string::npos,
                "field count missing");
        require(kerberos_message.find("field0_name=ticket_tgs") != std::string::npos,
                "field name missing");
        require(kerberos_message.find("field0_plain_hex=010203040506") != std::string::npos,
                "field plain hex missing");
        require(kerberos_message.find("field0_encrypted_hex=a1b2c3d4") != std::string::npos,
                "field encrypted hex missing");

        const cyber::ProtocolEvent parsed_kerberos = cyber::parse_protocol_event_line(
            "[Client1][ProtocolMonitor][PACKET_SEND] " + kerberos_message);
        require(parsed_kerberos.payload_fields.size() == 1U,
                "parsed payload field count mismatch");
        require(parsed_kerberos.payload_fields[0].name == "ticket_tgs",
                "parsed payload field name mismatch");
        require(parsed_kerberos.payload_fields[0].plain_hex == "010203040506",
                "parsed payload field plain mismatch");
        require(parsed_kerberos.payload_fields[0].encrypted_hex == "a1b2c3d4",
                "parsed payload field encrypted mismatch");

        const std::string kerberos_json = cyber::protocol_event_json(parsed_kerberos, 9);
        require(kerberos_json.find("\"payloadFields\":[{\"name\":\"ticket_tgs\"") !=
                    std::string::npos,
                "json payload fields missing");
        require(kerberos_json.find("\"plainHex\":\"010203040506\"") != std::string::npos,
                "json field plain missing");
        require(kerberos_json.find("\"encryptedHex\":\"a1b2c3d4\"") != std::string::npos,
                "json field encrypted missing");

        const cyber::Packet error =
            cyber::make_packet(cyber::MsgType::error, cyber::EntityId::as,
                               cyber::EntityId::client1,
                               cyber::make_error_payload(cyber::ErrorCode::password_wrong,
                                                         "bad password"));
        const std::string error_message = cyber::format_protocol_event_message(
            cyber::ProtocolDirection::recv, error, "", "12:03:16.001");
        require(error_message.find("message=MSG_ERROR.ERR_PASSWORD_WRONG") != std::string::npos,
                "error suffix missing");
        require(error_message.find("category=error") != std::string::npos,
                "error category missing");

        std::cout << "protocol_event_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "protocol_event_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
