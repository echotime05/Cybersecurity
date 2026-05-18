#pragma once

#include "cyber/common/crypto.hpp"
#include "cyber/common/packet.hpp"

#include <cstdint>

namespace cyber
{
struct AsReq
{
    EntityId idc = EntityId::unknown;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts1 = 0;
};

struct TicketTgsBody
{
    std::uint64_t kc_tgs = 0;
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts2 = 0;
    std::uint64_t lifetime2 = 0;
};

struct AsRepBody
{
    std::uint64_t kc_tgs = 0;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts2 = 0;
    std::uint64_t lifetime2 = 0;
    Bytes ticket_tgs;
};

struct AuthenticatorBody
{
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    std::uint64_t ts = 0;
};

struct TgsReq
{
    EntityId idv = EntityId::unknown;
    Bytes ticket_tgs;
    Bytes authenticator_tgs;
};

struct TicketVBody
{
    std::uint64_t kc_v = 0;
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    EntityId idv = EntityId::unknown;
    std::uint64_t ts4 = 0;
    std::uint64_t lifetime4 = 0;
};

struct TgsRepBody
{
    std::uint64_t kc_v = 0;
    EntityId idv = EntityId::unknown;
    std::uint64_t ts4 = 0;
    Bytes ticket_v;
};

struct VAuthReq
{
    Bytes ticket_v;
    Bytes authenticator_v;
};

struct VAuthRepBody
{
    std::uint64_t ts5_plus_1 = 0;
};

struct CertC2VBody
{
    EntityId client_id = EntityId::unknown;
    Bytes cert;
};

struct CertV2CBody
{
    EntityId v_id = EntityId::unknown;
    Bytes cert;
};

struct AppAckPayload
{
    MsgType acked_msg_type = MsgType::app;
    AppCode acked_app_code = AppCode::app_ack;
    EntityId acked_src = EntityId::unknown;
    EntityId acked_dst = EntityId::unknown;
    std::uint32_t acked_payload_len = 0;
    std::uint64_t acked_payload_hash = 0;
};

struct SignedAppPayload
{
    AppCode app_code = AppCode::app_ack;
    Bytes app_payload;
    Bytes signature;
};

Bytes as_build_req(const AsReq& value);
AsReq as_parse_req(const Bytes& payload);
Bytes tgs_ticket_build_body(const TicketTgsBody& value);
TicketTgsBody tgs_ticket_parse_body(const Bytes& payload);
Bytes tgs_ticket_encrypt(const TicketTgsBody& value, std::uint64_t ktgs);
TicketTgsBody tgs_ticket_decrypt(const Bytes& cipher, std::uint64_t ktgs);
Bytes as_build_rep_body(const AsRepBody& value);
AsRepBody as_parse_rep_body(const Bytes& payload);
Bytes authenticator_build_body(const AuthenticatorBody& value);
AuthenticatorBody authenticator_parse_body(const Bytes& payload);
Bytes authenticator_encrypt(const AuthenticatorBody& value, std::uint64_t key56);
AuthenticatorBody authenticator_decrypt(const Bytes& cipher, std::uint64_t key56);
Bytes tgs_build_req(const TgsReq& value);
TgsReq tgs_parse_req(const Bytes& payload);
Bytes v_ticket_build_body(const TicketVBody& value);
TicketVBody v_ticket_parse_body(const Bytes& payload);
Bytes v_ticket_encrypt(const TicketVBody& value, std::uint64_t kv);
TicketVBody v_ticket_decrypt(const Bytes& cipher, std::uint64_t kv);
Bytes tgs_build_rep_body(const TgsRepBody& value);
TgsRepBody tgs_parse_rep_body(const Bytes& payload);
Bytes v_auth_build_req(const VAuthReq& value);
VAuthReq v_auth_parse_req(const Bytes& payload);
Bytes v_auth_build_rep_body(const VAuthRepBody& value);
VAuthRepBody v_auth_parse_rep_body(const Bytes& payload);
Bytes cert_build_c2v_body(const CertC2VBody& value);
CertC2VBody cert_parse_c2v_body(const Bytes& payload);
Bytes cert_build_v2c_body(const CertV2CBody& value);
CertV2CBody cert_parse_v2c_body(const Bytes& payload);
Bytes ack_build_payload(const AppAckPayload& value);
AppAckPayload ack_parse_payload(const Bytes& payload);
Bytes app_build_signed_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key);
SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload);
bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key);
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload);
} // namespace cyber
