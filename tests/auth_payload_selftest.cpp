#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/protocol_payloads.hpp"

#include <filesystem>
#include <fstream>
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
        const std::uint64_t kc_tgs = 0x0011223344556677ULL;
        const std::uint64_t kc_v = 0x0001020304050607ULL;
        const std::uint64_t ktgs = 0x001c24deeecc136eULL;
        const std::uint64_t kv = 0x003398481d2a89f6ULL;
        const std::uint32_t adc = 0x7F000001U;

        cyber::AsReq as_req{cyber::EntityId::client1, cyber::EntityId::tgs, 1001};
        require(cyber::as_parse_req(cyber::as_build_req(as_req)).ts1 == 1001,
                "AS_REQ roundtrip failed");

        cyber::TicketTgsBody ticket_tgs_body{
            kc_tgs, cyber::EntityId::client1, adc, cyber::EntityId::tgs, 2002, 300000};
        const cyber::Bytes ticket_tgs = cyber::tgs_ticket_encrypt(ticket_tgs_body, ktgs);
        require(cyber::tgs_ticket_decrypt(ticket_tgs, ktgs).idc == cyber::EntityId::client1,
                "Ticket_tgs decrypt failed");

        cyber::AsRepBody as_rep_body{kc_tgs, cyber::EntityId::tgs, 2002, 300000, ticket_tgs};
        require(cyber::as_parse_rep_body(cyber::as_build_rep_body(as_rep_body)).ticket_tgs ==
                    ticket_tgs,
                "AS_REP_BODY roundtrip failed");

        cyber::AuthenticatorBody auth_tgs{cyber::EntityId::client1, adc, 3003};
        cyber::TgsReq tgs_req{cyber::EntityId::v, ticket_tgs,
                              cyber::authenticator_encrypt(auth_tgs, kc_tgs)};
        require(cyber::tgs_parse_req(cyber::tgs_build_req(tgs_req)).ticket_tgs == ticket_tgs,
                "TGS_REQ roundtrip failed");

        cyber::TicketVBody ticket_v_body{
            kc_v, cyber::EntityId::client1, adc, cyber::EntityId::v, 4004, 300000};
        const cyber::Bytes ticket_v = cyber::v_ticket_encrypt(ticket_v_body, kv);
        require(cyber::v_ticket_decrypt(ticket_v, kv).idv == cyber::EntityId::v,
                "Ticket_v decrypt failed");

        cyber::TgsRepBody tgs_rep_body{kc_v, cyber::EntityId::v, 4004, ticket_v};
        require(cyber::tgs_parse_rep_body(cyber::tgs_build_rep_body(tgs_rep_body)).ticket_v ==
                    ticket_v,
                "TGS_REP_BODY roundtrip failed");

        cyber::AuthenticatorBody auth_v{cyber::EntityId::client1, adc, 5005};
        cyber::VAuthReq v_auth_req{ticket_v, cyber::authenticator_encrypt(auth_v, kc_v)};
        require(cyber::v_auth_parse_req(cyber::v_auth_build_req(v_auth_req)).ticket_v == ticket_v,
                "V_AUTH_REQ roundtrip failed");

        require(cyber::v_auth_parse_rep_body(cyber::v_auth_build_rep_body({5006})).ts5_plus_1 ==
                    5006,
                "V_AUTH_REP_BODY roundtrip failed");

        const cyber::RsaKeyPair ca = cyber::ca_key_pair_from_hex(
            "0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3",
            "0x10001",
            "0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69");
        const cyber::RsaKeyPair client_pair = cyber::demo_rsa_key_pair_for(cyber::EntityId::client1);
        const cyber::Certificate client_cert =
            cyber::make_certificate(cyber::EntityId::client1, client_pair.public_key, ca.private_key);
        const cyber::Bytes cert_bytes = cyber::serialize_certificate(client_cert);

        cyber::CertC2VBody cert_c2v{cyber::EntityId::client1, cert_bytes};
        require(cyber::cert_parse_c2v_body(cyber::cert_build_c2v_body(cert_c2v)).cert == cert_bytes,
                "CERT_C2V_BODY roundtrip failed");

        cyber::AppAckPayload ack{cyber::MsgType::app, cyber::AppCode::game_join_req,
                                 cyber::EntityId::client1, cyber::EntityId::v, 2, 0xABCDEF};
        require(cyber::ack_parse_payload(cyber::ack_build_payload(ack)).acked_payload_hash ==
                    0xABCDEF,
                "APP_ACK roundtrip failed");

        const cyber::Bytes signed_join = cyber::app_build_signed_payload(
            cyber::AppCode::game_join_req, cyber::Bytes{0x01}, client_pair.private_key);
        const cyber::SignedAppPayload parsed_join = cyber::app_parse_signed_payload(signed_join);
        require(parsed_join.app_code == cyber::AppCode::game_join_req,
                "signed app code mismatch");
        require(cyber::app_verify_signed_payload(parsed_join, client_pair.public_key),
                "signed app verification failed");

        const std::filesystem::path config_path =
            std::filesystem::temp_directory_path() / "auth_payload_config.txt";
        {
            std::ofstream out(config_path);
            out << "C1_ID=0x01\nC2_ID=0x02\nC3_ID=0x03\nC4_ID=0x04\n"
                << "AS_ID=0x11\nTGS_ID=0x12\nV_ID=0x13\nLOCAL_CLIENT_ID=0x01\n"
                << "AS_BIND_IP=127.0.0.1\nAS_IP=127.0.0.1\nAS_HOST=127.0.0.1\nAS_PORT=1\n"
                << "TGS_BIND_IP=127.0.0.1\nTGS_IP=127.0.0.1\nTGS_HOST=127.0.0.1\nTGS_PORT=2\n"
                << "V_BIND_IP=127.0.0.1\nV_IP=127.0.0.1\nV_HOST=127.0.0.1\nV_PORT=3\n"
                << "C1_PASSWORD=123456\nC1_KC=0x59ef3db7cb8c8d\n"
                << "C2_PASSWORD=admin123\nC2_KC=0x6a73a4ebe9c564\n"
                << "C3_PASSWORD=hehe12345\nC3_KC=0x57ef9d5f45ab7b\n"
                << "C4_PASSWORD=&wxh@147\nC4_KC=0xec3eb766d59086\n"
                << "KTGS=0x1c24deeecc136e\nKV=0x3398481d2a89f6\n"
                << "PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3\n"
                << "PK_CA_E=0x10001\n"
                << "SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69\n";
        }
        const cyber::Config config = cyber::Config::load(config_path);
        cyber::AuthRuntime runtime = cyber::auth_make_runtime(config);
        cyber::Packet v_request =
            cyber::make_packet(cyber::MsgType::v_auth_req, cyber::EntityId::client1,
                               cyber::EntityId::v, cyber::v_auth_build_req(v_auth_req));
        cyber::Logger logger(std::filesystem::temp_directory_path() / "auth_payload_v_auth.log");
        cyber::Packet v_response =
            cyber::v_auth_process_request(v_request, config, runtime, logger, "AuthPayloadTest");
        require(v_response.msg_type == cyber::MsgType::v_auth_rep, "V_AUTH response type mismatch");
        require(v_response.src == cyber::EntityId::v, "V_AUTH response source mismatch");
        require(v_response.dst == cyber::EntityId::client1, "V_AUTH response destination mismatch");
        const cyber::VAuthRepBody parsed_v_response =
            cyber::v_auth_parse_rep_body(cyber::des_decrypt_payload(v_response.payload, kc_v));
        require(parsed_v_response.ts5_plus_1 == auth_v.ts + 1U,
                "V_AUTH response timestamp mismatch");
        require(runtime.v_sessions.get(cyber::EntityId::client1).v_auth_done,
                "V_AUTH session was not stored");

        std::cout << "auth_payload_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_payload_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
