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

Bytes build_as_req(const AsReq& value);
AsReq parse_as_req(const Bytes& payload);
Bytes build_ticket_tgs_body(const TicketTgsBody& value);
TicketTgsBody parse_ticket_tgs_body(const Bytes& payload);
Bytes encrypt_ticket_tgs(const TicketTgsBody& value, std::uint64_t ktgs);
TicketTgsBody decrypt_ticket_tgs(const Bytes& cipher, std::uint64_t ktgs);
Bytes build_as_rep_body(const AsRepBody& value);
AsRepBody parse_as_rep_body(const Bytes& payload);
Bytes build_authenticator_body(const AuthenticatorBody& value);
AuthenticatorBody parse_authenticator_body(const Bytes& payload);
Bytes encrypt_authenticator(const AuthenticatorBody& value, std::uint64_t key56);
AuthenticatorBody decrypt_authenticator(const Bytes& cipher, std::uint64_t key56);
Bytes build_tgs_req(const TgsReq& value);
TgsReq parse_tgs_req(const Bytes& payload);
Bytes build_ticket_v_body(const TicketVBody& value);
TicketVBody parse_ticket_v_body(const Bytes& payload);
Bytes encrypt_ticket_v(const TicketVBody& value, std::uint64_t kv);
TicketVBody decrypt_ticket_v(const Bytes& cipher, std::uint64_t kv);
Bytes build_tgs_rep_body(const TgsRepBody& value);
TgsRepBody parse_tgs_rep_body(const Bytes& payload);
Bytes build_v_auth_req(const VAuthReq& value);
VAuthReq parse_v_auth_req(const Bytes& payload);
Bytes build_v_auth_rep_body(const VAuthRepBody& value);
VAuthRepBody parse_v_auth_rep_body(const Bytes& payload);
Bytes build_cert_c2v_body(const CertC2VBody& value);
CertC2VBody parse_cert_c2v_body(const Bytes& payload);
Bytes build_cert_v2c_body(const CertV2CBody& value);
CertV2CBody parse_cert_v2c_body(const Bytes& payload);
Bytes build_app_ack_payload(const AppAckPayload& value);
AppAckPayload parse_app_ack_payload(const Bytes& payload);
Bytes build_signed_app_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key);
SignedAppPayload parse_signed_app_payload(const Bytes& decrypted_payload);
bool verify_signed_app_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key);
Bytes signed_app_logical_bytes(AppCode code, const Bytes& app_payload);
} // namespace cyber
