#include "cyber/protocol/certificate_messages.hpp"

#include "cyber/protocol/binary_codec.hpp"

namespace cyber
{
namespace
{
using protocol::detail::binary_read_bytes_u16;
using protocol::detail::binary_require_end;
using protocol::detail::binary_write_bytes_u16;
} // namespace

// 序列化 Client 发给 V 的证书 payload。
Bytes cert_build_c2v_body(const CertC2VBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.client_id));
    binary_write_bytes_u16(out, value.cert);
    return out;
}

// 解析 Client 发给 V 的证书 payload。
CertC2VBody cert_parse_c2v_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertC2VBody value;
    value.client_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "CERT_C2V_BODY");
    return value;
}

// 序列化 V 发给 Client 的证书 payload。
Bytes cert_build_v2c_body(const CertV2CBody& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.v_id));
    binary_write_bytes_u16(out, value.cert);
    return out;
}

// 解析 V 发给 Client 的证书 payload。
CertV2CBody cert_parse_v2c_body(const Bytes& payload)
{
    std::size_t offset = 0;
    CertV2CBody value;
    value.v_id = static_cast<EntityId>(payload.at(offset++));
    value.cert = binary_read_bytes_u16(payload, offset, "payload");
    binary_require_end(payload, offset, "CERT_V2C_BODY");
    return value;
}
} // namespace cyber
