# Kerberos Authentication and Non-Repudiation Design

## Source of Truth

This design follows `E:\zhuomian\cybersecurity\final_code\网络安全设计报告_日志设计修订版.docx`.

One deployment detail intentionally follows the real lab setup instead of the document paragraph about physical machines:

```text
Host 1: Client only
Host 2: Client + AS
Host 3: Client + TGS
Host 4: Client + V
```

All protocol field layouts, message meanings, logging constraints, and security-stage ordering follow the revised report unless this document explicitly says otherwise.

## Goal

Implement the normal security path through:

```text
AS_REQ / AS_REP
TGS_REQ / TGS_REP
V_AUTH_REQ / V_AUTH_REP
CERT_C2V / CERT_V2C
GAME_JOIN_REQ + signed APP_ACK
```

The result should prove that the four Kerberos-style stages, encrypted payloads, certificate exchange, and normal bidirectional non-repudiation path work across the current four-host deployment.

## Out of Scope for This Pass

These features remain intentionally deferred:

- Password-error retry and UI password fallback.
- Ticket expiration error handling.
- Authenticator/Ticket identity mismatch error handling.
- Replay detection.
- Tampered message or tampered ACK demo switches.
- `GAME_START`, `GAME_STATE`, tank movement, collision, bullets, and full game sync.
- Qt/Web visualization.

The implementation may still return `MSG_ERROR` for truly unsupported messages, but no complete error recovery state machine is required in this pass.

## Command Surface

Keep the existing light connectivity probe:

```powershell
.\build-mingw\client.exe --connect-test
```

Add a real authentication and non-repudiation acceptance entry point:

```powershell
.\build-mingw\client.exe --auth-test
```

Expected successful client output:

```text
AUTH_STATE AS_OK
AUTH_STATE TGS_OK
AUTH_STATE V_AUTH_OK
AUTH_STATE AUTH_DONE
APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED
APP_NON_REPUDIATION APP_ACK_VERIFIED
auth-test: ok
```

## Module Boundaries

Add three focused common modules.

`crypto.hpp/cpp` owns byte-level security helpers:

- `des_encrypt_payload(Bytes plain, std::uint64_t key56) -> Bytes`
- `des_decrypt_payload(Bytes cipher, std::uint64_t key56) -> Bytes`
- `hash64(Bytes data) -> std::uint64_t`
- `rsa_sign_hash(hash, private_key) -> Bytes`
- `rsa_verify_hash(hash, signature, public_key) -> bool`
- `generate_des_key56() -> std::uint64_t`
- certificate creation and verification helpers

`protocol_payloads.hpp/cpp` owns binary payload builders/parsers. It must not open sockets, write logs, or know role state.

`auth_flow.hpp/cpp` owns AS/TGS/V/Client authentication flow helpers. It may use sockets and logs through existing `send_packet_logged()` / `recv_packet_logged()`.

This split keeps packet serialization, cryptography, payload layout, and role runtime behavior separately testable.

## Cryptography Design

The revised report requires DES for symmetric encryption and RSA for signatures.

DES encrypts payload bytes only, never the fixed packet header. The encrypted input is either:

```text
M
M || signature
```

The report describes padding with `0x0K`. To make decryption unambiguous, implement the reversible equivalent:

```text
If K bytes are needed, append K bytes, each with value K.
If input length is already a multiple of 8, append 8 bytes of 0x08.
```

This preserves the report's block-DES intent while avoiding data loss when the original payload ends in zero bytes.

RSA is used for signing `hash(payload)`, not encrypting full payloads. `hash64()` is the shared digest used for tickets, logs, signatures, certificate signatures, and ACK correlation. The first implementation can use the existing local RSA experiment code as a source, but the common API must hide the implementation details.

## Certificate Design

Certificate layout follows the report:

```text
Certificate {
  uint8 subject_id
  PublicKey subject_pk
  bytes ca_signature
}

PublicKey {
  uint256 n
  uint32 e
}
```

The CA signature is:

```text
Sig_SK_CA(Hash(subject_id || subject_pk))
```

The CA public/private parameters come from `config/course_config.txt`.

For this pass, each process may generate its own runtime keypair on startup. Keys do not need to be persisted to disk because the certificate exchange runs after each successful V_AUTH stage. V stores the Client public key in its in-memory auth session table; Client stores V's public key after `CERT_V2C`.

## Payload Layouts

All integer fields are encoded big-endian. Entity IDs are `uint8_t`. Timestamps use `uint64_t` Unix epoch milliseconds. Address field `ADc` is `uint32_t`; for this pass it can be derived from the local configured client address or set to a stable configured value, as long as Client/TGS/V use the same value for normal verification.

Implemented payloads:

```text
AS_REQ {
  uint8  IDc
  uint8  IDtgs
  uint64 TS1
}

AS_REP {
  bytes E_Kc_AS_REP_BODY
}

AS_REP_BODY {
  DESKey Kc_tgs
  uint8  IDtgs
  uint64 TS2
  uint64 Lifetime2
  bytes  Ticket_tgs
}

Ticket_tgs = E_Ktgs(TICKET_TGS_BODY)

TICKET_TGS_BODY {
  DESKey Kc_tgs
  uint8  IDc
  uint32 ADc
  uint8  IDtgs
  uint64 TS2
  uint64 Lifetime2
}

TGS_REQ {
  uint8  IDv
  uint16 ticket_tgs_len
  bytes  Ticket_tgs
  uint16 authenticator_len
  bytes  Authenticator_tgs
}

Authenticator_tgs = E_Kc_tgs(AUTHENTICATOR_TGS_BODY)

AUTHENTICATOR_TGS_BODY {
  uint8  IDc
  uint32 ADc
  uint64 TS3
}

TGS_REP {
  bytes E_Kc_tgs_TGS_REP_BODY
}

TGS_REP_BODY {
  DESKey Kc_v
  uint8  IDv
  uint64 TS4
  bytes  Ticket_v
}

Ticket_v = E_Kv(TICKET_V_BODY)

TICKET_V_BODY {
  DESKey Kc_v
  uint8  IDc
  uint32 ADc
  uint8  IDv
  uint64 TS4
  uint64 Lifetime4
}

V_AUTH_REQ {
  uint16 ticket_v_len
  bytes  Ticket_v
  uint16 authenticator_len
  bytes  Authenticator_v
}

Authenticator_v = E_Kc_v(AUTHENTICATOR_V_BODY)

AUTHENTICATOR_V_BODY {
  uint8  IDc
  uint32 ADc
  uint64 TS5
}

V_AUTH_REP {
  bytes E_Kc_v_V_AUTH_REP_BODY
}

V_AUTH_REP_BODY {
  uint64 TS5_plus_1
}

CERT_C2V {
  bytes E_Kc_v_CERT_C2V_BODY
}

CERT_C2V_BODY {
  uint8  client_id
  uint16 cert_len
  bytes  Cert_C
}

CERT_V2C {
  bytes E_Kc_v_CERT_V2C_BODY
}

CERT_V2C_BODY {
  uint8  v_id
  uint16 cert_len
  bytes  Cert_V
}

APP_ACK app_payload {
  uint8  acked_msg_type
  uint8  acked_app_code
  uint8  acked_src_ID
  uint8  acked_dst_ID
  uint32 acked_payload_len
  uint64 acked_payload_hash
}
```

## Role Flows

### AS

AS accepts a short connection, receives `MSG_AS_REQ`, and:

```text
parse AS_REQ
look up Kc by IDc
generate Kc_tgs
build Ticket_tgs = E_Ktgs(TICKET_TGS_BODY)
build AS_REP_BODY
send MSG_AS_REP with payload E_Kc(AS_REP_BODY)
close connection
```

Client decrypts the response with its configured `Kc`, stores `Kc_tgs` and `Ticket_tgs`, then logs:

```text
AUTH_STATE AS_OK
```

### TGS

TGS accepts a short connection, receives `MSG_TGS_REQ`, and:

```text
decrypt Ticket_tgs with Ktgs
decrypt Authenticator_tgs with Kc_tgs
check IDc, IDtgs, and requested IDv for the normal path
generate Kc_v
build Ticket_v = E_Kv(TICKET_V_BODY)
build TGS_REP_BODY
send MSG_TGS_REP with payload E_Kc_tgs(TGS_REP_BODY)
close connection
```

Client decrypts with `Kc_tgs`, stores `Kc_v` and `Ticket_v`, then logs:

```text
AUTH_STATE TGS_OK
```

### V_AUTH

V accepts a short connection, receives `MSG_V_AUTH_REQ`, and:

```text
decrypt Ticket_v with Kv
decrypt Authenticator_v with Kc_v
check IDc and IDv for the normal path
store AuthSessionTable[client_id] = { kc_v, v_auth_done=true }
send MSG_V_AUTH_REP with payload E_Kc_v(TS5 + 1)
close connection
```

Client decrypts with `Kc_v`, verifies `TS5 + 1`, then logs:

```text
AUTH_STATE V_AUTH_OK
```

### Certificate Exchange

Client opens another short connection to V and sends:

```text
MSG_CERT_C2V payload = E_Kc_v(client_id || cert_len || Cert_C)
```

V:

```text
find AuthSessionTable[client_id]
decrypt with Kc_v
verify Cert_C with PK_CA
store client public key
mark cert_done=true
send MSG_CERT_V2C payload = E_Kc_v(v_id || cert_len || Cert_V)
close connection
```

Client decrypts `CERT_V2C`, verifies V's certificate with `PK_CA`, stores `PK_V`, then logs:

```text
AUTH_STATE AUTH_DONE
```

### Application Non-Repudiation

Client opens an application connection to V and sends one signed `GAME_JOIN_REQ`.

Logical payload:

```text
APP_code || app_payload
```

Signed payload:

```text
APP_code || app_payload || signature(hash(APP_code || app_payload))
```

Network payload:

```text
E_Kc_v(APP_code || app_payload || signature)
```

V:

```text
decrypt with Kc_v
parse GAME_JOIN_REQ
verify signature with cached client public key
log APP_NON_REPUDIATION GAME_JOIN_REQ_VERIFIED
build signed APP_ACK
send MSG_APP with encrypted signed APP_ACK
```

Client:

```text
decrypt APP_ACK with Kc_v
verify signature with cached V public key
check acked_msg_type, acked_app_code, src, dst, payload_len, payload_hash
log APP_NON_REPUDIATION APP_ACK_VERIFIED
```

## V Auth Session Table

V keeps this table in memory:

```text
AuthSession {
  EntityId client_id
  std::uint64_t kc_v
  PublicKey client_public_key
  bool v_auth_done
  bool cert_done
}
```

No persistence is required. A V process restart clears the table; clients must rerun `--auth-test`.

## Logging

Keep the fixed log line format:

```text
[实体][线程名][事件] 内容
```

Sensitive data must not be logged in plaintext:

- `Kc`, `Kc_tgs`, `Kc_v`, `Ktgs`, `Kv`, RSA private keys: `MASKED`
- Tickets and encrypted payloads: `cipher(hash=0x...)`
- Signatures: `signature(hash=0x...)`

Important events:

- `AUTH_STATE`
- `PACKET_SEND`
- `PACKET_RECV`
- `APP_NON_REPUDIATION`
- `LOG`
- `ERROR`

The `send_packet_logged()` and `recv_packet_logged()` rule remains mandatory for network packet IO.

## Tests

Add `crypto_selftest`:

- DES encrypt/decrypt roundtrip for arbitrary lengths.
- DES padding roundtrip when plaintext length is exactly 8 bytes.
- `hash64()` stable output for same input.
- RSA sign/verify succeeds.
- Certificate creation and CA verification succeeds.

Add `auth_payload_selftest`:

- Build/parse each Kerberos payload listed above.
- Encrypt/decrypt `Ticket_tgs`, `Ticket_v`, authenticators, and certificate bodies.
- Build/parse `APP_ACK`.

Add `auth_flow_selftest`:

- Start AS/TGS/V locally with temporary config.
- Run `client --auth-test`.
- Assert client output contains `auth-test: ok`.
- Assert logs contain the expected `AUTH_STATE` and `APP_NON_REPUDIATION` lines.

Existing `connect_probe_selftest` remains as a fast network skeleton test and should not be removed.

## Four-Host Acceptance

Host 2:

```powershell
Copy-Item .\config\lan\host2_as_client2.txt .\config\course_config.txt -Force
.\build-mingw\as_server.exe --serve
```

Host 3:

```powershell
Copy-Item .\config\lan\host3_tgs_client3.txt .\config\course_config.txt -Force
.\build-mingw\tgs_server.exe --serve
```

Host 4:

```powershell
.\run_v.bat
```

Any client host:

```powershell
.\build-mingw\client.exe --auth-test
```

Successful output:

```text
AUTH_STATE AS_OK
AUTH_STATE TGS_OK
AUTH_STATE V_AUTH_OK
AUTH_STATE AUTH_DONE
APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED
APP_NON_REPUDIATION APP_ACK_VERIFIED
auth-test: ok
```

## Implementation Notes

`--auth-test` should use the real flow. It should not reuse temporary probe payloads.

`--connect-test` remains useful for quick LAN reachability and should keep its current lightweight behavior.

The first implementation should prefer correctness and log clarity over optimizing DES subkey caching or parallel block encryption. The report's optimized DES section can be implemented later without changing the public `crypto` API.
