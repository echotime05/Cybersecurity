#pragma once

#include "cyber/protocol/packet.hpp"

namespace cyber
{
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

Bytes cert_build_c2v_body(const CertC2VBody& value);
CertC2VBody cert_parse_c2v_body(const Bytes& payload);
Bytes cert_build_v2c_body(const CertV2CBody& value);
CertV2CBody cert_parse_v2c_body(const Bytes& payload);
} // namespace cyber
