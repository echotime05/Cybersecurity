#pragma once

#include "cyber/protocol/packet.hpp"

namespace cyber
{
// Client 发给 V 的证书交换消息，携带 Client 自己的证书。
struct CertC2VBody
{
    EntityId client_id = EntityId::unknown;
    Bytes cert;
};

// V 发给 Client 的证书交换消息，携带 V 的证书。
struct CertV2CBody
{
    EntityId v_id = EntityId::unknown;
    Bytes cert;
};

// 序列化 Client 到 V 的证书消息。
Bytes cert_build_c2v_body(const CertC2VBody& value);
// 解析 Client 到 V 的证书消息。
CertC2VBody cert_parse_c2v_body(const Bytes& payload);
// 序列化 V 到 Client 的证书消息。
Bytes cert_build_v2c_body(const CertV2CBody& value);
// 解析 V 到 Client 的证书消息。
CertV2CBody cert_parse_v2c_body(const Bytes& payload);
} // namespace cyber
