#pragma once

#include "cyber/protocol/packet.hpp"

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
} // namespace cyber
