#pragma once

#include "cyber/protocol/packet.hpp"
#include "cyber/shared/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
// 演示用 RSA 公钥，n/e 用于证书验证和签名验签。
struct RsaPublicKey
{
    std::uint64_t n = 0;
    std::uint64_t e = 0;
};

// 演示用 RSA 私钥，n/d 用于对报文摘要签名。
struct RsaPrivateKey
{
    std::uint64_t n = 0;
    std::uint64_t d = 0;
};

// 一个角色的 RSA 公私钥对。
struct RsaKeyPair
{
    RsaPublicKey public_key;
    RsaPrivateKey private_key;
};

// CA 签发的简化证书，绑定 subject_id 和 subject_pk。
struct Certificate
{
    EntityId subject_id = EntityId::unknown;
    RsaPublicKey subject_pk;
    Bytes ca_signature;
};

// 使用 56 位 DES 密钥加密 payload 字节。
Bytes des_encrypt_payload(const Bytes& plain, std::uint64_t key56);
// 使用 56 位 DES 密钥解密 payload 字节。
Bytes des_decrypt_payload(const Bytes& cipher, std::uint64_t key56);

// 计算项目内签名和 ACK 引用使用的 64 位摘要。
std::uint64_t hash64(const Bytes& data);
// 生成一把演示用 56 位 DES 会话密钥。
std::uint64_t generate_des_key56();

// 将十六进制整数截断到 64 位，用于读取演示 RSA 参数。
std::uint64_t integer_from_hex_truncated(const std::string& hex);
// 将整数按大端序转换成字节，可指定最小输出长度。
Bytes integer_to_bytes(std::uint64_t value, std::size_t min_size = 0);
// 从字节串中按大端序截断读取 64 位整数。
std::uint64_t integer_from_bytes_truncated(const Bytes& bytes);

// 根据配置里的十六进制 n/e/d 构造 CA 密钥对。
RsaKeyPair ca_key_pair_from_hex(const std::string& n_hex, const std::string& e_hex,
                                const std::string& d_hex);
// 为指定角色生成固定演示 RSA 密钥对，保证每次运行可复现。
RsaKeyPair demo_rsa_key_pair_for(EntityId id);
// 对 64 位摘要做 RSA 私钥签名，应用层不可否认使用它。
Bytes rsa_sign_hash(std::uint64_t hash, const RsaPrivateKey& private_key);
// 用 RSA 公钥验证摘要签名是否匹配。
bool rsa_verify_hash(std::uint64_t hash, const Bytes& signature, const RsaPublicKey& public_key);

// 将 RSA 公钥序列化到证书交换 payload。
Bytes serialize_public_key(const RsaPublicKey& key);
// 从证书交换 payload 解析 RSA 公钥。
RsaPublicKey parse_public_key(const Bytes& bytes);
// 将证书结构序列化为网络字节。
Bytes serialize_certificate(const Certificate& cert);
// 从网络字节解析证书结构。
Certificate parse_certificate(const Bytes& bytes);
// 由 CA 私钥为角色公钥生成证书。
Certificate make_certificate(EntityId subject_id, const RsaPublicKey& subject_pk,
                             const RsaPrivateKey& ca_private_key);
// 用 CA 公钥验证证书签名。
bool verify_certificate(const Certificate& cert, const RsaPublicKey& ca_public_key);
} // namespace cyber
