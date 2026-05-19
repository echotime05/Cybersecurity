# Code Style Simplification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the final AS/TGS/V/Client code easier to explain and modify by clarifying core flows, removing duplicate binary codec helpers, keeping only the encrypted game path, and aligning shared header names.

**Architecture:** The four role implementations stay in `src/roles/*`, while reusable protocol/game/network/runtime code stays in `src/shared/*`. The public include tree mirrors this: role headers remain under `include/cyber/roles/*`, and cross-role utility headers move from `include/cyber/common/*` to `include/cyber/shared/*`.

**Tech Stack:** C++17, CMake, MinGW build directory `build-mingw`, PowerShell scripts, browser Web UI served by Vite.

---

## File Structure

- Create `include/cyber/protocol/binary_codec.hpp`: inline helpers for big-endian integer, byte-vector, and float payload encoding/decoding.
- Modify `src/shared/protocol/kerberos_messages.cpp`: remove local integer/vector helpers and call `binary_codec.hpp`.
- Modify `src/shared/protocol/certificate_messages.cpp`: remove local integer/vector helpers and call `binary_codec.hpp`.
- Modify `src/shared/protocol/app_envelope.cpp`: remove local integer/vector helpers and call `binary_codec.hpp`.
- Modify `src/shared/game/game_protocol.cpp`: remove local integer/float helpers and call `binary_codec.hpp`.
- Modify `include/cyber/game/app_payload_codec.hpp` and `src/shared/game/app_payload_codec.cpp`: remove the plaintext/encrypted switch from the public API; final game payloads are encrypted.
- Modify `include/cyber/game/game_non_repudiation.hpp` and `src/shared/game/game_non_repudiation.cpp`: remove the plaintext/encrypted switch from signed game packet helpers.
- Modify `include/cyber/roles/client/tank_game_client.hpp`, `src/roles/client/tank_game_client.cpp`, `include/cyber/roles/v/tank_game_server.hpp`, and `src/roles/v/tank_game_server.cpp`: remove runtime plaintext branches and keep the encrypted final path.
- Modify `src/shared/runtime/role_runtime.cpp`: construct final client/V game services without an encryption-mode flag.
- Move `include/cyber/common/*.hpp` to `include/cyber/shared/*.hpp`: keep the same file responsibilities but use one naming convention that matches `src/shared`.
- Update include directives in `include/`, `src/`, `tests/`, `README.md`, and docs from `cyber/common/...` to `cyber/shared/...`.
- Add short comments to `src/roles/as/as_service.cpp`, `src/roles/tgs/tgs_service.cpp`, `src/roles/v/v_auth_service.cpp`, `src/roles/client/client_auth_flow.cpp`, and `src/roles/v/tank_game_server.cpp` explaining the role-level flow.

## Task 1: Commit Planning Baseline

**Files:**
- Create: `docs/superpowers/plans/2026-05-19-code-style-simplification.md`

- [ ] **Step 1: Commit the plan**

Run:

```powershell
git add docs/superpowers/plans/2026-05-19-code-style-simplification.md
git commit -m "docs: plan code style simplification"
```

Expected: one docs-only commit.

## Task 2: Add Core Flow Comments

**Files:**
- Modify: `src/roles/as/as_service.cpp`
- Modify: `src/roles/tgs/tgs_service.cpp`
- Modify: `src/roles/v/v_auth_service.cpp`
- Modify: `src/roles/client/client_auth_flow.cpp`
- Modify: `src/roles/v/tank_game_server.cpp`

- [ ] **Step 1: Add comments before role-level flow blocks**

Add concise English comments that explain the role intent, for example:

```cpp
// AS handles the first Kerberos hop: verify the client password-derived key,
// create Kc_tgs, and return the TGS ticket encrypted for TGS.
```

and:

```cpp
// V owns the authoritative game loop. Client input changes local room state,
// while state snapshots are signed, encrypted, and broadcast from this loop.
```

- [ ] **Step 2: Verify no formatting-only churn**

Run:

```powershell
git diff -- src/roles/as/as_service.cpp src/roles/tgs/tgs_service.cpp src/roles/v/v_auth_service.cpp src/roles/client/client_auth_flow.cpp src/roles/v/tank_game_server.cpp
```

Expected: comments only, or comments plus nearby whitespace needed for readability.

- [ ] **Step 3: Commit comments**

Run:

```powershell
git add src/roles/as/as_service.cpp src/roles/tgs/tgs_service.cpp src/roles/v/v_auth_service.cpp src/roles/client/client_auth_flow.cpp src/roles/v/tank_game_server.cpp
git commit -m "docs: clarify core role flows"
```

## Task 3: Extract Binary Codec Helpers

**Files:**
- Create: `include/cyber/protocol/binary_codec.hpp`
- Modify: `src/shared/protocol/kerberos_messages.cpp`
- Modify: `src/shared/protocol/certificate_messages.cpp`
- Modify: `src/shared/protocol/app_envelope.cpp`
- Modify: `src/shared/game/game_protocol.cpp`
- Modify: `README.md`

- [ ] **Step 1: Add the helper header**

Create inline helpers in `namespace cyber::protocol::detail`:

```cpp
void binary_write_u16(Bytes& out, std::uint16_t value);
void binary_write_u32(Bytes& out, std::uint32_t value);
void binary_write_u64(Bytes& out, std::uint64_t value);
void binary_write_f32(Bytes& out, float value);
std::uint16_t binary_read_u16(const Bytes& in, std::size_t& offset, std::string_view context);
std::uint32_t binary_read_u32(const Bytes& in, std::size_t& offset, std::string_view context);
std::uint64_t binary_read_u64(const Bytes& in, std::size_t& offset, std::string_view context);
float binary_read_f32(const Bytes& in, std::size_t& offset, std::string_view context);
void binary_write_bytes_u16(Bytes& out, const Bytes& bytes);
Bytes binary_read_bytes_u16(const Bytes& in, std::size_t& offset, std::string_view context);
void binary_require_end(const Bytes& in, std::size_t offset, std::string_view context);
```

- [ ] **Step 2: Replace duplicated helpers**

Replace local helper functions in the four implementation files with calls to `cyber::protocol::detail::*`.

- [ ] **Step 3: Run focused codec tests**

Run:

```powershell
cmake --build build-mingw
.\build-mingw\tests\protocol_selftest.exe
.\build-mingw\tests\game_protocol_selftest.exe
.\build-mingw\tests\auth_payload_selftest.exe
```

Expected: build succeeds and all three executables exit with code 0.

- [ ] **Step 4: Commit codec helper extraction**

Run:

```powershell
git add include/cyber/protocol/binary_codec.hpp src/shared/protocol/kerberos_messages.cpp src/shared/protocol/certificate_messages.cpp src/shared/protocol/app_envelope.cpp src/shared/game/game_protocol.cpp README.md
git commit -m "refactor: centralize binary protocol codec helpers"
```

## Task 4: Keep Only the Final Encrypted Game Path

**Files:**
- Modify: `include/cyber/game/app_payload_codec.hpp`
- Modify: `src/shared/game/app_payload_codec.cpp`
- Modify: `include/cyber/game/game_non_repudiation.hpp`
- Modify: `src/shared/game/game_non_repudiation.cpp`
- Modify: `include/cyber/roles/client/tank_game_client.hpp`
- Modify: `src/roles/client/tank_game_client.cpp`
- Modify: `include/cyber/roles/v/tank_game_server.hpp`
- Modify: `src/roles/v/tank_game_server.cpp`
- Modify: `src/shared/runtime/role_runtime.cpp`
- Modify: affected tests under `tests/`

- [ ] **Step 1: Remove encryption-mode booleans from game payload APIs**

Change these signatures:

```cpp
Bytes app_encode_payload(const Bytes& plain_payload, std::uint64_t kc_v);
Bytes app_decode_payload(const Bytes& wire_payload, std::uint64_t kc_v);
```

and make both always use `des_encrypt` / `des_decrypt`.

- [ ] **Step 2: Remove encryption-mode booleans from non-repudiation helpers**

Change signed packet helpers so callers no longer pass an encryption flag:

```cpp
Packet app_build_signed_game_packet(EntityId src, EntityId dst, AppCode code, const Bytes& game_payload, const KeyPair& sender_key, std::uint64_t kc_v);
VerifiedGamePacket app_decode_signed_packet(const Packet& packet, const PublicKey& sender_public_key, std::uint64_t kc_v);
Packet ack_build_signed_packet(EntityId src, EntityId dst, const AckRecord& ack, const KeyPair& sender_key, std::uint64_t kc_v);
```

- [ ] **Step 3: Simplify Client/V constructors**

Remove `encrypt_app_payloads` constructor parameters and members. Keep `require_auth` on V because final V still needs authenticated sessions before accepting gameplay.

- [ ] **Step 4: Update tests**

Update call sites in tests to the new API. Keep `encrypted_plaintext_rejection_client` as the negative test proving V rejects plaintext game payloads.

- [ ] **Step 5: Run final-link tests**

Run:

```powershell
cmake --build build-mingw
.\build-mingw\tests\app_payload_codec_selftest.exe
.\build-mingw\tests\tank_game_server_flow_selftest.exe
powershell -ExecutionPolicy Bypass -File .\tests\auth_encrypted_game_flow_selftest.ps1
```

Expected: build succeeds and final encrypted flow passes.

- [ ] **Step 6: Commit final-link cleanup**

Run:

```powershell
git add include src tests README.md docs
git commit -m "refactor: keep encrypted game payload path only"
```

## Task 5: Rename common Headers to shared

**Files:**
- Move: `include/cyber/common/*.hpp` to `include/cyber/shared/*.hpp`
- Modify: all include references in `include/`, `src/`, `tests/`, `README.md`, and docs

- [ ] **Step 1: Move headers with git mv**

Run:

```powershell
New-Item -ItemType Directory -Force include/cyber/shared
git mv include/cyber/common/*.hpp include/cyber/shared/
Remove-Item include/cyber/common -Force
```

- [ ] **Step 2: Replace include paths**

Replace every `cyber/common/` include with `cyber/shared/`.

- [ ] **Step 3: Update documentation**

Update the source responsibility index and any docs that mention `include/cyber/common`.

- [ ] **Step 4: Run build and broad tests**

Run:

```powershell
cmake --build build-mingw
.\build-mingw\tests\protocol_selftest.exe
.\build-mingw\tests\game_protocol_selftest.exe
.\build-mingw\tests\app_payload_codec_selftest.exe
.\build-mingw\tests\tank_game_server_flow_selftest.exe
powershell -ExecutionPolicy Bypass -File .\tests\auth_encrypted_game_flow_selftest.ps1
```

Expected: build succeeds and all listed tests pass.

- [ ] **Step 5: Commit shared naming cleanup**

Run:

```powershell
git add include src tests README.md docs
git commit -m "refactor: rename common headers to shared"
```

## Task 6: Final Verification and Push

**Files:**
- No intentional source changes after this task.

- [ ] **Step 1: Run repository checks**

Run:

```powershell
git diff --check
git status --short --branch
```

Expected: no whitespace errors; branch is ahead of origin only by intended commits.

- [ ] **Step 2: Push**

Run:

```powershell
git push origin main
```

Expected: push succeeds.

