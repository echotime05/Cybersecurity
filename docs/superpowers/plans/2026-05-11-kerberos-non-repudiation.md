# Kerberos Non-Repudiation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the normal Kerberos authentication chain, encrypted certificate exchange, and signed `GAME_JOIN_REQ` / `APP_ACK` non-repudiation path behind `client --auth-test`.

**Architecture:** Keep the current packet and socket layer intact. Add focused common modules for cryptography, binary payload layouts, and authentication flow, then wire them into the existing role runtime without removing `--connect-test`.

**Tech Stack:** C++17, CMake, MinGW/Ninja or MSVC, Boost.Multiprecision header-only `cpp_int`, PowerShell integration tests.

---

## File Structure

Create:

- `include/cyber/common/crypto.hpp`: DES/RSA/hash/certificate public API.
- `src/common/crypto.cpp`: DES block encryption, padding, hash64, RSA sign/verify, certificate helpers.
- `include/cyber/common/protocol_payloads.hpp`: typed payload structs and builders/parsers for Kerberos, certificate exchange, signed app payloads, and ACK.
- `src/common/protocol_payloads.cpp`: big-endian binary encoding/decoding implementations.
- `include/cyber/common/auth_flow.hpp`: `AuthClientState`, `AuthSessionTable`, and role-flow function declarations.
- `src/common/auth_flow.cpp`: AS/TGS/V request handlers and Client `--auth-test` sequence.
- `tests/crypto_selftest.cpp`: deterministic crypto and certificate tests.
- `tests/auth_payload_selftest.cpp`: payload layout roundtrip tests.
- `tests/auth_flow_selftest.ps1`: local AS/TGS/V process test for `client --auth-test`.

Modify:

- `CMakeLists.txt`: add new common sources and test targets.
- `src/common/role_runtime.cpp`: add `--auth-test`; dispatch `--serve` connections to real auth handlers while preserving `--connect-test`.
- `src/common/net_packet.cpp`: improve packet payload summaries for encrypted auth/app messages without exposing secrets.
- `README.md`: document `--auth-test` and four-host acceptance commands.

Do not commit `config/course_config.txt`; it remains host-local.

---

### Task 1: Crypto API and Self-Test

**Files:**
- Create: `include/cyber/common/crypto.hpp`
- Create: `src/common/crypto.cpp`
- Create: `tests/crypto_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing crypto self-test**

Create `tests/crypto_selftest.cpp` with this test scaffold:

```cpp
#include "cyber/common/crypto.hpp"
#include "cyber/common/packet.hpp"
#include "cyber/common/types.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

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
        const cyber::Bytes short_plain = {0x41, 0x42, 0x43};
        const cyber::Bytes exact_plain = {0, 1, 2, 3, 4, 5, 6, 7};
        const std::uint64_t key56 = 0x001c24deeecc136eULL;

        const cyber::Bytes short_cipher = cyber::des_encrypt_payload(short_plain, key56);
        require(short_cipher != short_plain, "short DES cipher should differ from plaintext");
        require(cyber::des_decrypt_payload(short_cipher, key56) == short_plain,
                "short DES roundtrip failed");

        const cyber::Bytes exact_cipher = cyber::des_encrypt_payload(exact_plain, key56);
        require(exact_cipher.size() == 16U, "8-byte plaintext should receive a full padding block");
        require(cyber::des_decrypt_payload(exact_cipher, key56) == exact_plain,
                "exact-block DES roundtrip failed");

        const std::uint64_t h1 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x03});
        const std::uint64_t h2 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x03});
        const std::uint64_t h3 = cyber::hash64(cyber::Bytes{0x01, 0x02, 0x04});
        require(h1 == h2, "hash64 must be stable");
        require(h1 != h3, "hash64 should distinguish nearby inputs");

        const cyber::RsaKeyPair pair = cyber::demo_rsa_key_pair_for(cyber::EntityId::client1);
        const cyber::Bytes signature = cyber::rsa_sign_hash(h1, pair.private_key);
        require(cyber::rsa_verify_hash(h1, signature, pair.public_key), "RSA verify failed");
        require(!cyber::rsa_verify_hash(h3, signature, pair.public_key),
                "RSA verify should reject a different hash");

        const cyber::RsaKeyPair ca_pair = cyber::ca_key_pair_from_hex(
            "0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3",
            "0x10001",
            "0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69");
        const cyber::Certificate cert =
            cyber::make_certificate(cyber::EntityId::client1, pair.public_key, ca_pair.private_key);
        require(cyber::verify_certificate(cert, ca_pair.public_key),
                "CA certificate verification failed");
        require(cert.subject_id == cyber::EntityId::client1, "certificate subject mismatch");

        std::cout << "crypto_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "crypto_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
```

- [ ] **Step 2: Add the target and verify the test fails to compile**

Modify `CMakeLists.txt` by adding `src/common/crypto.cpp` to `cyber_common` and adding:

```cmake
add_executable(crypto_selftest tests/crypto_selftest.cpp)
target_link_libraries(crypto_selftest PRIVATE cyber_common)
add_test(NAME crypto_selftest COMMAND crypto_selftest)
```

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\CMake_64\bin;' + $env:PATH
cmake --build build-mingw --target crypto_selftest
```

Expected: compile fails because `cyber/common/crypto.hpp` does not exist.

- [ ] **Step 3: Add the crypto public API**

Create `include/cyber/common/crypto.hpp` with:

```cpp
#pragma once

#include "cyber/common/packet.hpp"
#include "cyber/common/types.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <cstdint>
#include <string>

namespace cyber
{
struct RsaPublicKey
{
    boost::multiprecision::cpp_int n;
    boost::multiprecision::cpp_int e;
};

struct RsaPrivateKey
{
    boost::multiprecision::cpp_int n;
    boost::multiprecision::cpp_int d;
};

struct RsaKeyPair
{
    RsaPublicKey public_key;
    RsaPrivateKey private_key;
};

struct Certificate
{
    EntityId subject_id = EntityId::unknown;
    RsaPublicKey subject_pk;
    Bytes ca_signature;
};

Bytes des_encrypt_payload(const Bytes& plain, std::uint64_t key56);
Bytes des_decrypt_payload(const Bytes& cipher, std::uint64_t key56);

std::uint64_t hash64(const Bytes& data);
std::uint64_t generate_des_key56();

boost::multiprecision::cpp_int cpp_int_from_hex(const std::string& hex);
Bytes cpp_int_to_bytes(const boost::multiprecision::cpp_int& value, std::size_t min_size = 0);
boost::multiprecision::cpp_int cpp_int_from_bytes(const Bytes& bytes);

RsaKeyPair ca_key_pair_from_hex(const std::string& n_hex, const std::string& e_hex,
                                const std::string& d_hex);
RsaKeyPair demo_rsa_key_pair_for(EntityId id);
Bytes rsa_sign_hash(std::uint64_t hash, const RsaPrivateKey& private_key);
bool rsa_verify_hash(std::uint64_t hash, const Bytes& signature, const RsaPublicKey& public_key);

Bytes serialize_public_key(const RsaPublicKey& key);
RsaPublicKey parse_public_key(const Bytes& bytes);
Bytes serialize_certificate(const Certificate& cert);
Certificate parse_certificate(const Bytes& bytes);
Certificate make_certificate(EntityId subject_id, const RsaPublicKey& subject_pk,
                             const RsaPrivateKey& ca_private_key);
bool verify_certificate(const Certificate& cert, const RsaPublicKey& ca_public_key);
} // namespace cyber
```

- [ ] **Step 4: Implement crypto internals**

Create `src/common/crypto.cpp`.

Implementation requirements:

- Use Boost.Multiprecision `cpp_int` for RSA modular exponentiation.
- Use FNV-1a 64-bit for `hash64`.
- Use deterministic demo RSA key pairs for Client/V app signatures:

```text
Client1: n=9173503,  e=65537, d=4922825
Client2: n=11948269, e=65537, d=11461409
Client3: n=12263803, e=65537, d=10494993
Client4: n=13879469, e=65537, d=11097473
V:       n=13822969, e=65537, d=2096225
```

- For `rsa_sign_hash`, sign `hash % private_key.n`.
- For `rsa_verify_hash`, verify `signature^e mod n == hash % n`.
- For `serialize_public_key`, encode `n` as 32 bytes big-endian followed by `e` as 4 bytes big-endian.
- For `serialize_certificate`, encode:

```text
subject_id(1B) || subject_pk_len(2B) || subject_pk || ca_signature_len(2B) || ca_signature
```

- For DES, implement 64-bit block DES with the standard DES IP, IP inverse, E expansion, P permutation, PC-1, PC-2, S-boxes, and 16-round shift schedule. Treat `key56` as the effective 56-bit key before PC-2. The payload helpers split padded input into 8-byte blocks, encrypt/decrypt each block, then remove padding.

Use these function names inside `crypto.cpp` so future workers can test individual sections if needed:

```cpp
namespace
{
std::uint64_t read_u64_be(const Bytes& bytes, std::size_t offset);
void write_u64_be(Bytes& out, std::uint64_t value);
Bytes apply_des_padding(const Bytes& plain);
Bytes remove_des_padding(const Bytes& padded);
std::uint64_t des_encrypt_block(std::uint64_t block, std::uint64_t key56);
std::uint64_t des_decrypt_block(std::uint64_t block, std::uint64_t key56);
boost::multiprecision::cpp_int mod_pow(boost::multiprecision::cpp_int base,
                                       boost::multiprecision::cpp_int exp,
                                       const boost::multiprecision::cpp_int& mod);
}
```

- [ ] **Step 5: Run crypto self-test**

Run:

```powershell
cmake --build build-mingw --target crypto_selftest
.\build-mingw\crypto_selftest.exe
```

Expected:

```text
crypto_selftest: ok
```

- [ ] **Step 6: Commit crypto module**

```powershell
git add CMakeLists.txt include/cyber/common/crypto.hpp src/common/crypto.cpp tests/crypto_selftest.cpp
git commit -m "Add crypto primitives for auth flow"
```

---

### Task 2: Kerberos Payload Builders and Parsers

**Files:**
- Create: `include/cyber/common/protocol_payloads.hpp`
- Create: `src/common/protocol_payloads.cpp`
- Create: `tests/auth_payload_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing payload self-test**

Create `tests/auth_payload_selftest.cpp` with tests for every payload struct:

```cpp
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

        std::cout << "auth_payload_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_payload_selftest failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
```

- [ ] **Step 2: Add the target and verify the test fails**

Modify `CMakeLists.txt`:

```cmake
add_executable(auth_payload_selftest tests/auth_payload_selftest.cpp)
target_link_libraries(auth_payload_selftest PRIVATE cyber_common)
add_test(NAME auth_payload_selftest COMMAND auth_payload_selftest)
```

Run:

```powershell
cmake --build build-mingw --target auth_payload_selftest
```

Expected: compile fails because `cyber/common/protocol_payloads.hpp` does not exist.

- [ ] **Step 3: Add payload structs and function declarations**

Create `include/cyber/common/protocol_payloads.hpp` with structs named exactly as used in the test:

```cpp
#pragma once

#include "cyber/common/crypto.hpp"
#include "cyber/common/packet.hpp"

#include <cstdint>

namespace cyber
{
struct AsReq { EntityId idc; EntityId idtgs; std::uint64_t ts1; };
struct TicketTgsBody {
    std::uint64_t kc_tgs; EntityId idc; std::uint32_t adc; EntityId idtgs;
    std::uint64_t ts2; std::uint64_t lifetime2;
};
struct AsRepBody {
    std::uint64_t kc_tgs; EntityId idtgs; std::uint64_t ts2;
    std::uint64_t lifetime2; Bytes ticket_tgs;
};
struct AuthenticatorBody { EntityId idc; std::uint32_t adc; std::uint64_t ts; };
struct TgsReq { EntityId idv; Bytes ticket_tgs; Bytes authenticator_tgs; };
struct TicketVBody {
    std::uint64_t kc_v; EntityId idc; std::uint32_t adc; EntityId idv;
    std::uint64_t ts4; std::uint64_t lifetime4;
};
struct TgsRepBody { std::uint64_t kc_v; EntityId idv; std::uint64_t ts4; Bytes ticket_v; };
struct VAuthReq { Bytes ticket_v; Bytes authenticator_v; };
struct VAuthRepBody { std::uint64_t ts5_plus_1; };
struct CertC2VBody { EntityId client_id; Bytes cert; };
struct CertV2CBody { EntityId v_id; Bytes cert; };
struct AppAckPayload {
    MsgType acked_msg_type; AppCode acked_app_code; EntityId acked_src;
    EntityId acked_dst; std::uint32_t acked_payload_len; std::uint64_t acked_payload_hash;
};
struct SignedAppPayload { AppCode app_code; Bytes app_payload; Bytes signature; };

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
} // namespace cyber
```

- [ ] **Step 4: Implement payload encoding**

Create `src/common/protocol_payloads.cpp`.

Implementation rules:

- Use big-endian integer helpers for `uint16`, `uint32`, and `uint64`.
- Throw `PacketError` with a concrete message when a payload is too short or has trailing bytes.
- Length-prefixed byte fields use `uint16` length.
- `DESKey` is encoded as 8 bytes big-endian, with only the low 56 bits meaningful.
- `build_signed_app_payload()` signs `hash64(APP_code || app_payload)`.
- `verify_signed_app_payload()` verifies the same logical bytes.

Use these local helper signatures:

```cpp
namespace
{
void write_u16(Bytes& out, std::uint16_t value);
void write_u32(Bytes& out, std::uint32_t value);
void write_u64(Bytes& out, std::uint64_t value);
std::uint16_t read_u16(const Bytes& in, std::size_t& offset);
std::uint32_t read_u32(const Bytes& in, std::size_t& offset);
std::uint64_t read_u64(const Bytes& in, std::size_t& offset);
void write_bytes_u16(Bytes& out, const Bytes& bytes);
Bytes read_bytes_u16(const Bytes& in, std::size_t& offset);
void require_end(const Bytes& in, std::size_t offset, const char* name);
}
```

- [ ] **Step 5: Run payload self-test**

Run:

```powershell
cmake --build build-mingw --target auth_payload_selftest
.\build-mingw\auth_payload_selftest.exe
```

Expected:

```text
auth_payload_selftest: ok
```

- [ ] **Step 6: Commit payload module**

```powershell
git add CMakeLists.txt include/cyber/common/protocol_payloads.hpp src/common/protocol_payloads.cpp tests/auth_payload_selftest.cpp
git commit -m "Add Kerberos payload builders"
```

---

### Task 3: Auth Flow Module

**Files:**
- Create: `include/cyber/common/auth_flow.hpp`
- Create: `src/common/auth_flow.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the auth flow header**

Create `include/cyber/common/auth_flow.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_payloads.hpp"

#include <map>
#include <mutex>
#include <string>

namespace cyber
{
struct AuthClientState
{
    EntityId client_id = EntityId::unknown;
    std::uint32_t adc = 0x7F000001U;
    std::uint64_t kc = 0;
    std::uint64_t kc_tgs = 0;
    std::uint64_t kc_v = 0;
    Bytes ticket_tgs;
    Bytes ticket_v;
    RsaKeyPair client_key_pair;
    RsaPublicKey v_public_key;
};

struct AuthSession
{
    EntityId client_id = EntityId::unknown;
    std::uint32_t adc = 0;
    std::uint64_t kc_v = 0;
    RsaPublicKey client_public_key;
    bool v_auth_done = false;
    bool cert_done = false;
};

class AuthSessionTable
{
public:
    void put_v_auth(EntityId client_id, std::uint32_t adc, std::uint64_t kc_v);
    AuthSession get(EntityId client_id) const;
    void put_client_public_key(EntityId client_id, const RsaPublicKey& public_key);

private:
    mutable std::mutex mutex_;
    std::map<EntityId, AuthSession> sessions_;
};

struct AuthRuntime
{
    RsaKeyPair ca_key_pair;
    RsaKeyPair v_key_pair;
    AuthSessionTable v_sessions;
};

AuthRuntime make_auth_runtime(const Config& config);

void handle_auth_connection(RoleKind role, SocketHandle socket, const Config& config,
                            AuthRuntime& runtime, Logger& logger, const std::string& thread_name);

void run_client_auth_test(const Config& config, Logger& logger);
} // namespace cyber
```

- [ ] **Step 2: Add source to CMake and verify compile fails**

Modify `CMakeLists.txt` by adding `src/common/auth_flow.cpp` to `cyber_common`.

Run:

```powershell
cmake --build build-mingw --target cyber_common
```

Expected: compile fails until `src/common/auth_flow.cpp` exists.

- [ ] **Step 3: Implement shared auth helpers**

Create `src/common/auth_flow.cpp` with these helper responsibilities:

- `make_auth_runtime()` reads `PK_CA_N`, `PK_CA_E`, `SK_CA_D` with `Config::get_string()` and builds CA key pair.
- `make_auth_runtime()` initializes `v_key_pair = demo_rsa_key_pair_for(EntityId::v)`.
- `client_secret_for(config, client_id)` returns the matching `ClientSecret`.
- `now_ms()` returns `uint64_t` Unix epoch milliseconds.
- `lifetime_ms()` returns `5 * 60 * 1000`.
- `entity_log_name(EntityId)` returns `Client1`, `Client2`, `Client3`, `Client4`, `AS`, `TGS`, or `V`.
- `encrypted_packet(type, src, dst, plain, key)` wraps `des_encrypt_payload(plain, key)` into a `Packet`.

- [ ] **Step 4: Implement AS handler**

In `handle_auth_connection()` route `RoleKind::as_server` to:

```text
recv MSG_AS_REQ
parse AsReq
find Kc for IDc
generate Kc_tgs
build Ticket_tgs encrypted by KTGS
build AS_REP_BODY
send MSG_AS_REP encrypted by Kc
log AUTH_STATE AS_REP_SENT client=<ClientN>
close socket
```

Use `send_packet_logged()` and `recv_packet_logged()` for all socket IO.

- [ ] **Step 5: Implement TGS handler**

Route `RoleKind::tgs_server` to:

```text
recv MSG_TGS_REQ
parse TgsReq
decrypt Ticket_tgs with KTGS
decrypt Authenticator_tgs with Kc_tgs
verify IDc from ticket equals IDc from authenticator
generate Kc_v
build Ticket_v encrypted by KV
build TGS_REP_BODY
send MSG_TGS_REP encrypted by Kc_tgs
log AUTH_STATE TGS_REP_SENT client=<ClientN>
close socket
```

For this pass, if verification fails, throw a runtime error; the outer connection handler logs `ERROR`.

- [ ] **Step 6: Implement V handler**

Route `RoleKind::v_server` by first packet type:

```text
MSG_V_AUTH_REQ:
  decrypt Ticket_v with KV
  decrypt Authenticator_v with Kc_v
  verify IDc from ticket equals IDc from authenticator
  runtime.v_sessions.put_v_auth(IDc, ADc, Kc_v)
  send MSG_V_AUTH_REP encrypted by Kc_v with TS5 + 1
  log AUTH_STATE V_AUTH_REP_SENT client=<ClientN>
  close socket

MSG_CERT_C2V:
  identify client from encrypted body after decrypting with each stored Kc_v until one parses
  verify Cert_C with PK_CA
  runtime.v_sessions.put_client_public_key(client_id, cert.subject_pk)
  send MSG_CERT_V2C encrypted by Kc_v
  log AUTH_STATE CERT_V2C_SENT client=<ClientN>
  close socket

MSG_APP:
  decrypt signed app payload with session Kc_v
  parse GAME_JOIN_REQ
  verify Client signature with cached client public key
  log APP_NON_REPUDIATION GAME_JOIN_REQ_VERIFIED
  build signed APP_ACK using V private key
  send encrypted MSG_APP APP_ACK
  log APP_NON_REPUDIATION APP_ACK_SIGNED
  close socket
```

The certificate step may try sessions in deterministic order `Client1..Client4` because `CERT_C2V` body is encrypted and the packet header only identifies the source. Once decrypted, require body `client_id` to match the tested session.

- [ ] **Step 7: Implement Client auth sequence**

Implement `run_client_auth_test()`:

```text
load local client id and Kc
create client RSA key pair
connect AS, send AS_REQ, decrypt AS_REP, log and print AUTH_STATE AS_OK
connect TGS, send TGS_REQ, decrypt TGS_REP, log and print AUTH_STATE TGS_OK
connect V, send V_AUTH_REQ, decrypt V_AUTH_REP, log and print AUTH_STATE V_AUTH_OK
connect V, send CERT_C2V, decrypt CERT_V2C, verify V certificate, log and print AUTH_STATE AUTH_DONE
connect V, send signed encrypted GAME_JOIN_REQ, log and print GAME_JOIN_REQ_SIGNED
receive signed encrypted APP_ACK, verify V signature, log and print APP_ACK_VERIFIED
print auth-test: ok
```

Use `connect_tcp()` with configured `AS_IP`, `TGS_IP`, and `V_IP`.

- [ ] **Step 8: Build role targets**

Run:

```powershell
cmake --build build-mingw --target as_server tgs_server v_server client
```

Expected: all four targets build.

- [ ] **Step 9: Commit auth flow module**

```powershell
git add CMakeLists.txt include/cyber/common/auth_flow.hpp src/common/auth_flow.cpp
git commit -m "Add Kerberos auth flow"
```

---

### Task 4: Wire Runtime CLI and Server Dispatch

**Files:**
- Modify: `src/common/role_runtime.cpp`

- [ ] **Step 1: Add `--auth-test` parsing**

In `run_role_main()`, add:

```cpp
bool auth_test = false;
```

Add parser branch:

```cpp
else if (arg == "--auth-test")
{
    auth_test = true;
}
```

Update usage text to include `[--auth-test]`.

- [ ] **Step 2: Add AuthRuntime lifetime to server mode**

In `run_server()`, create one `AuthRuntime` before the accept loop:

```cpp
AuthRuntime auth_runtime = make_auth_runtime(config);
```

Pass it by shared pointer or reference wrapper to each worker thread so V's `AuthSessionTable` survives across V_AUTH, CERT, and APP connections. AS/TGS can also receive it; they only need CA parameters indirectly.

- [ ] **Step 3: Preserve `--connect-test`**

Keep `make_probe_response()` and `run_client_connect_test()` unchanged for `--connect-test`.

Inside `handle_probe_connection()`, rename it to a neutral worker such as `handle_server_connection()` and route based on a new enum or boolean:

```cpp
if (server_mode == ServerMode::auth)
{
    handle_auth_connection(role, socket, config, *auth_runtime, *logger, thread_name);
}
else
{
    const Packet request = recv_packet_logged(socket, *logger, spec.name, thread_name);
    const Packet response = make_probe_response(role, spec, request);
    send_packet_logged(socket, response, *logger, spec.name, thread_name);
}
```

Use auth mode for normal `--serve`. The existing `connect_probe_selftest.ps1` will still pass after Task 5 because it only checks message names and auth flow emits those real messages.

- [ ] **Step 4: Call `run_client_auth_test()`**

In `run_role_main()` after `connect_test`:

```cpp
else if (auth_test)
{
    if (role != RoleKind::client)
    {
        throw std::runtime_error("--auth-test is only supported by client");
    }
    Logger logger(role_log_path(RoleKind::client, config));
    run_client_auth_test(config, logger);
}
```

- [ ] **Step 5: Build and smoke test help**

Run:

```powershell
cmake --build build-mingw --target client
.\build-mingw\client.exe --help
```

Expected: usage includes `--auth-test`.

- [ ] **Step 6: Commit runtime wiring**

```powershell
git add src/common/role_runtime.cpp
git commit -m "Wire auth-test runtime"
```

---

### Task 5: Local Auth Flow Integration Test

**Files:**
- Create: `tests/auth_flow_selftest.ps1`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the PowerShell integration test**

Create `tests/auth_flow_selftest.ps1` by copying the structure of `tests/connect_probe_selftest.ps1` and changing the client invocation to:

```powershell
& (Join-Path $BuildDir 'client.exe') --config $config --auth-test
```

Use these process connection counts:

```powershell
$processes += Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '1') 'as_server' $tmp
$processes += Start-RoleProcess 'tgs_server.exe' @('--config', $config, '--serve', '--max-connections', '1') 'tgs_server' $tmp
$processes += Start-RoleProcess 'v_server.exe' @('--config', $config, '--serve', '--max-connections', '3') 'v_server' $tmp
```

Assert these client output and log lines:

```powershell
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] AS_OK'
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] TGS_OK'
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] V_AUTH_OK'
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] AUTH_DONE'
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'APP_NON_REPUDIATION] GAME_JOIN_REQ_SIGNED'
Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'APP_NON_REPUDIATION] APP_ACK_VERIFIED'
Assert-Contains (Join-Path $tmp 'logs\v.log') 'APP_NON_REPUDIATION] GAME_JOIN_REQ_VERIFIED'
Assert-Contains (Join-Path $tmp 'logs\v.log') 'APP_NON_REPUDIATION] APP_ACK_SIGNED'
```

The test should write:

```powershell
Write-Host 'auth_flow_selftest: ok'
```

- [ ] **Step 2: Add CTest entry**

Modify `CMakeLists.txt`:

```cmake
if(WIN32)
    add_test(
        NAME auth_flow_selftest
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_flow_selftest.ps1
                -BuildDir $<TARGET_FILE_DIR:client>
    )
endif()
```

- [ ] **Step 3: Run auth flow self-test**

Run:

```powershell
cmake --build build-mingw
ctest --test-dir build-mingw -R auth_flow_selftest --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 4: Run all tests**

Run:

```powershell
ctest --test-dir build-mingw --output-on-failure
```

Expected: every listed test passes:

```text
protocol_selftest
log_selftest
net_packet_selftest
crypto_selftest
auth_payload_selftest
connect_probe_selftest
auth_flow_selftest
```

- [ ] **Step 5: Commit integration test**

```powershell
git add CMakeLists.txt tests/auth_flow_selftest.ps1
git commit -m "Add auth flow integration test"
```

---

### Task 6: Packet Log Summaries and Documentation

**Files:**
- Modify: `src/common/net_packet.cpp`
- Modify: `README.md`
- Modify: `docs/four-host-connect-test.md`

- [ ] **Step 1: Improve packet summary without leaking secrets**

In `format_payload_summary()`:

- For `MSG_AS_REP`, `MSG_TGS_REP`, `MSG_V_AUTH_REP`, `MSG_CERT_C2V`, and `MSG_CERT_V2C`, print this shape with a 16-digit uppercase hash:

```text
payload{encrypted=true, cipher_hash=0x1234567890ABCDEF, cipher_len=64}
```

- For encrypted `MSG_APP`, print this shape with a 16-digit uppercase hash:

```text
MSG_APP{encrypted=true, cipher_hash=0x1234567890ABCDEF, cipher_len=64}
```

Use `hash64(packet.payload)` from `crypto.hpp`. Do not decrypt inside `net_packet.cpp`.

- [ ] **Step 2: Run log-related tests**

Run:

```powershell
cmake --build build-mingw --target log_selftest net_packet_selftest
.\build-mingw\log_selftest.exe
.\build-mingw\net_packet_selftest.exe
```

Expected:

```text
log_selftest: ok
net_packet_selftest: ok
```

- [ ] **Step 3: Document `--auth-test`**

Update `README.md` with this section:

````markdown
## 真实认证与双向不可否认验收

启动 AS/TGS/V 后，任一 Client 主机运行：

```powershell
.\build-mingw\client.exe --auth-test
```

成功输出：

```text
AUTH_STATE AS_OK
AUTH_STATE TGS_OK
AUTH_STATE V_AUTH_OK
AUTH_STATE AUTH_DONE
APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED
APP_NON_REPUDIATION APP_ACK_VERIFIED
auth-test: ok
```
````

Update `docs/four-host-connect-test.md` with a section named `完整认证验收` using the same command.

- [ ] **Step 4: Run full validation**

Run:

```powershell
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 5: Commit docs and log summaries**

```powershell
git add src/common/net_packet.cpp README.md docs/four-host-connect-test.md
git commit -m "Document auth-test acceptance"
```

---

### Task 7: Push and Four-Host Handoff

**Files:**
- No code files.

- [ ] **Step 1: Check worktree**

Run:

```powershell
git status --short
```

Expected: only host-local `config/course_config.txt` may be modified.

- [ ] **Step 2: Push commits**

Run:

```powershell
git pull --rebase --autostash
git push
```

Expected: push succeeds. If autostash conflicts only in `config/course_config.txt`, restore this host's runtime config:

```powershell
Copy-Item .\config\lan\host4_v_client4.txt .\config\course_config.txt -Force
git add config/course_config.txt
git restore --staged config/course_config.txt
```

- [ ] **Step 3: Give operators the exact commands**

Host 2:

```powershell
cd <repo>\code
git pull
cmake --build build-mingw
Copy-Item .\config\lan\host2_as_client2.txt .\config\course_config.txt -Force
.\build-mingw\as_server.exe --serve
```

Host 3:

```powershell
cd <repo>\code
git pull
cmake --build build-mingw
Copy-Item .\config\lan\host3_tgs_client3.txt .\config\course_config.txt -Force
.\build-mingw\tgs_server.exe --serve
```

Host 4:

```powershell
cd E:\zhuomian\cybersecurity\code
git pull
cmake --build build-mingw
.\run_v.bat
```

Any client host:

```powershell
cd <repo>\code
.\build-mingw\client.exe --auth-test
```

Expected client output:

```text
AUTH_STATE AS_OK
AUTH_STATE TGS_OK
AUTH_STATE V_AUTH_OK
AUTH_STATE AUTH_DONE
APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED
APP_NON_REPUDIATION APP_ACK_VERIFIED
auth-test: ok
```
