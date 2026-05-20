#pragma once

#include "cyber/protocol/packet.hpp"

#include <cstdint>

namespace cyber
{
// Client 发给 AS 的认证请求，声明自己和目标 TGS。
struct AsReq
{
    EntityId idc = EntityId::unknown;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts1 = 0;
};

// AS 写入 TGT 的内容，由 TGS 长期密钥加密。
struct TicketTgsBody
{
    std::uint64_t kc_tgs = 0;
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts2 = 0;
    std::uint64_t lifetime2 = 0;
};

// AS 返回给 Client 的主体内容，由客户端 Kc 加密。
struct AsRepBody
{
    std::uint64_t kc_tgs = 0;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts2 = 0;
    std::uint64_t lifetime2 = 0;
    Bytes ticket_tgs;
};

// Client 访问 TGS 或 V 时携带的一次性认证器，用会话密钥加密。
struct AuthenticatorBody
{
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    std::uint64_t ts = 0;
};

// Client 发给 TGS 的请求，携带 TGT 和面向 TGS 的认证器。
struct TgsReq
{
    EntityId idv = EntityId::unknown;
    Bytes ticket_tgs;
    Bytes authenticator_tgs;
};

// TGS 写入 V ticket 的内容，由 V 长期密钥加密。
struct TicketVBody
{
    std::uint64_t kc_v = 0;
    EntityId idc = EntityId::unknown;
    std::uint32_t adc = 0;
    EntityId idv = EntityId::unknown;
    std::uint64_t ts4 = 0;
    std::uint64_t lifetime4 = 0;
};

// TGS 返回给 Client 的主体内容，由 Kc_tgs 加密。
struct TgsRepBody
{
    std::uint64_t kc_v = 0;
    EntityId idv = EntityId::unknown;
    std::uint64_t ts4 = 0;
    Bytes ticket_v;
};

// Client 发给 V 的认证请求，携带 V ticket 和面向 V 的认证器。
struct VAuthReq
{
    Bytes ticket_v;
    Bytes authenticator_v;
};

// V 认证通过后返回 ts5+1，证明 V 解开了认证器。
struct VAuthRepBody
{
    std::uint64_t ts5_plus_1 = 0;
};

// 序列化 AS 请求 payload。
Bytes as_build_req(const AsReq& value);
// 解析 AS 请求 payload。
AsReq as_parse_req(const Bytes& payload);
// 序列化 TGT 明文主体。
Bytes tgs_ticket_build_body(const TicketTgsBody& value);
// 解析 TGT 明文主体。
TicketTgsBody tgs_ticket_parse_body(const Bytes& payload);
// 用 Ktgs 加密 TGT 主体。
Bytes tgs_ticket_encrypt(const TicketTgsBody& value, std::uint64_t ktgs);
// 用 Ktgs 解密并解析 TGT 主体。
TicketTgsBody tgs_ticket_decrypt(const Bytes& cipher, std::uint64_t ktgs);
// 序列化 AS 应答明文主体。
Bytes as_build_rep_body(const AsRepBody& value);
// 解析 AS 应答明文主体。
AsRepBody as_parse_rep_body(const Bytes& payload);
// 序列化认证器明文主体。
Bytes authenticator_build_body(const AuthenticatorBody& value);
// 解析认证器明文主体。
AuthenticatorBody authenticator_parse_body(const Bytes& payload);
// 用会话密钥加密认证器。
Bytes authenticator_encrypt(const AuthenticatorBody& value, std::uint64_t key56);
// 用会话密钥解密并解析认证器。
AuthenticatorBody authenticator_decrypt(const Bytes& cipher, std::uint64_t key56);
// 序列化 TGS 请求 payload。
Bytes tgs_build_req(const TgsReq& value);
// 解析 TGS 请求 payload。
TgsReq tgs_parse_req(const Bytes& payload);
// 序列化 V ticket 明文主体。
Bytes v_ticket_build_body(const TicketVBody& value);
// 解析 V ticket 明文主体。
TicketVBody v_ticket_parse_body(const Bytes& payload);
// 用 Kv 加密 V ticket 主体。
Bytes v_ticket_encrypt(const TicketVBody& value, std::uint64_t kv);
// 用 Kv 解密并解析 V ticket 主体。
TicketVBody v_ticket_decrypt(const Bytes& cipher, std::uint64_t kv);
// 序列化 TGS 应答明文主体。
Bytes tgs_build_rep_body(const TgsRepBody& value);
// 解析 TGS 应答明文主体。
TgsRepBody tgs_parse_rep_body(const Bytes& payload);
// 序列化 V 认证请求 payload。
Bytes v_auth_build_req(const VAuthReq& value);
// 解析 V 认证请求 payload。
VAuthReq v_auth_parse_req(const Bytes& payload);
// 序列化 V 认证应答明文主体。
Bytes v_auth_build_rep_body(const VAuthRepBody& value);
// 解析 V 认证应答明文主体。
VAuthRepBody v_auth_parse_rep_body(const Bytes& payload);
} // namespace cyber
