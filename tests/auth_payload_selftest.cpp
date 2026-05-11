#include "cyber/common/crypto.hpp"
#include "cyber/common/protocol_payloads.hpp"

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
        require(cyber::parse_as_req(cyber::build_as_req(as_req)).ts1 == 1001,
                "AS_REQ roundtrip failed");

        cyber::TicketTgsBody ticket_tgs_body{
            kc_tgs, cyber::EntityId::client1, adc, cyber::EntityId::tgs, 2002, 300000};
        const cyber::Bytes ticket_tgs = cyber::encrypt_ticket_tgs(ticket_tgs_body, ktgs);
        require(cyber::decrypt_ticket_tgs(ticket_tgs, ktgs).idc == cyber::EntityId::client1,
                "Ticket_tgs decrypt failed");

        cyber::AsRepBody as_rep_body{kc_tgs, cyber::EntityId::tgs, 2002, 300000, ticket_tgs};
        require(cyber::parse_as_rep_body(cyber::build_as_rep_body(as_rep_body)).ticket_tgs ==
                    ticket_tgs,
                "AS_REP_BODY roundtrip failed");

        cyber::AuthenticatorBody auth_tgs{cyber::EntityId::client1, adc, 3003};
        cyber::TgsReq tgs_req{cyber::EntityId::v, ticket_tgs,
                              cyber::encrypt_authenticator(auth_tgs, kc_tgs)};
        require(cyber::parse_tgs_req(cyber::build_tgs_req(tgs_req)).ticket_tgs == ticket_tgs,
                "TGS_REQ roundtrip failed");

        cyber::TicketVBody ticket_v_body{
            kc_v, cyber::EntityId::client1, adc, cyber::EntityId::v, 4004, 300000};
        const cyber::Bytes ticket_v = cyber::encrypt_ticket_v(ticket_v_body, kv);
        require(cyber::decrypt_ticket_v(ticket_v, kv).idv == cyber::EntityId::v,
                "Ticket_v decrypt failed");

        cyber::TgsRepBody tgs_rep_body{kc_v, cyber::EntityId::v, 4004, ticket_v};
        require(cyber::parse_tgs_rep_body(cyber::build_tgs_rep_body(tgs_rep_body)).ticket_v ==
                    ticket_v,
                "TGS_REP_BODY roundtrip failed");

        cyber::AuthenticatorBody auth_v{cyber::EntityId::client1, adc, 5005};
        cyber::VAuthReq v_auth_req{ticket_v, cyber::encrypt_authenticator(auth_v, kc_v)};
        require(cyber::parse_v_auth_req(cyber::build_v_auth_req(v_auth_req)).ticket_v == ticket_v,
                "V_AUTH_REQ roundtrip failed");

        require(cyber::parse_v_auth_rep_body(cyber::build_v_auth_rep_body({5006})).ts5_plus_1 ==
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
        require(cyber::parse_cert_c2v_body(cyber::build_cert_c2v_body(cert_c2v)).cert == cert_bytes,
                "CERT_C2V_BODY roundtrip failed");

        cyber::AppAckPayload ack{cyber::MsgType::app, cyber::AppCode::game_join_req,
                                 cyber::EntityId::client1, cyber::EntityId::v, 2, 0xABCDEF};
        require(cyber::parse_app_ack_payload(cyber::build_app_ack_payload(ack)).acked_payload_hash ==
                    0xABCDEF,
                "APP_ACK roundtrip failed");

        const cyber::Bytes signed_join = cyber::build_signed_app_payload(
            cyber::AppCode::game_join_req, cyber::Bytes{0x01}, client_pair.private_key);
        const cyber::SignedAppPayload parsed_join = cyber::parse_signed_app_payload(signed_join);
        require(parsed_join.app_code == cyber::AppCode::game_join_req,
                "signed app code mismatch");
        require(cyber::verify_signed_app_payload(parsed_join, client_pair.public_key),
                "signed app verification failed");

        std::cout << "auth_payload_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_payload_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
