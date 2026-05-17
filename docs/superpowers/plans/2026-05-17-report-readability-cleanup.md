# Report Readability Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Clean redundant naming and wrapper files first, then add focused comments that make the authentication, encryption, ACK evidence, and game-message flow easier to explain in the report.

**Architecture:** Keep the current encrypted tank runtime unchanged. The cleanup phase removes misleading legacy names from live logs and removes duplicate root `.bat` wrappers while keeping the documented PowerShell scripts. The comment phase adds short boundary comments at the protocol, authentication, game, and non-repudiation seams without adding line-by-line narration.

**Tech Stack:** Windows C++17, CMake/Ninja, PowerShell, TypeScript/Vite Web UI, Git.

---

## Scope

In scope:

- Rename live log thread labels that still say `Plain...` or `AuthPlain...`.
- Delete root `.bat` wrappers that only forward to `scripts/*.ps1`.
- Update active docs so they reference `scripts/*.ps1` directly.
- Add concise comments in core C++ files that explain report-relevant boundaries.
- Run the existing C++ and Web UI verification suites.

Out of scope:

- Do not change packet headers, payload formats, encryption, signatures, ACK semantics, ports, or UI behavior.
- Do not delete `scripts/run_local.ps1`, `scripts/run_v.ps1`, `scripts/run_web.ps1`, or `scripts/stop_local.ps1`.
- Do not touch generated directories such as `build-mingw/`, `build-web-tests/`, `logs/`, or `web-ui/dist/`.
- Do not touch the untracked `image_processing_platform/` directory.
- Do not edit archived docs under `docs/archive/` except through reference checks that explicitly exclude archives.

## File Structure

Files to modify during cleanup:

- `src/game/tank_game_server.cpp`: rename misleading live log thread labels.
- `src/game/tank_game_client.cpp`: rename misleading live log thread labels.
- `README.md`: remove root `.bat` alternatives from active run instructions.
- `docs/four-host-connect-test.md`: replace the active `run_v.bat` example with `scripts/run_v.ps1`.

Files to delete during cleanup:

- `run_local.bat`: duplicate wrapper for `scripts/run_local.ps1`.
- `run_v.bat`: duplicate wrapper for `scripts/run_v.ps1`.
- `run_web.bat`: duplicate wrapper for `scripts/run_web.ps1`.
- `stop_local.bat`: duplicate wrapper for `scripts/stop_local.ps1`.

Files to modify during comment pass:

- `include/cyber/common/auth_flow.hpp`: document the handoff from Kerberos to the game layer.
- `include/cyber/game/game_non_repudiation.hpp`: document signed game payload and ACK evidence boundaries.
- `src/common/protocol_payloads.cpp`: document the signature input bytes.
- `src/game/app_payload_codec.cpp`: document that encrypted mode wraps only `Packet.payload`.
- `src/game/tank_game_server.cpp`: document V's authoritative gameplay boundary.
- `src/game/tank_game_client.cpp`: document the browser input to signed packet path and ACK/state receive loop.

---

### Task 1: Rename Misleading Runtime Log Labels

**Files:**

- Modify: `src/game/tank_game_server.cpp`
- Modify: `src/game/tank_game_client.cpp`

- [ ] **Step 1: Replace server log labels**

In `src/game/tank_game_server.cpp`, replace every occurrence of these strings:

```text
PlainAccept
PlainClient
PlainClientAck
PlainGameAuth
PlainGameCert
PlainGameLoop
```

with:

```text
TankAccept
TankClient
TankClientAck
TankAuth
TankCert
TankGameLoop
```

The edited calls should include these exact labels:

```cpp
logger_.write("V", "TankAccept", "ACCEPT", "accept " + peer);
logger_.write("V", "TankClient", "THREAD_START", "client " + peer);
send_packet_logged(socket, ack, logger_, "V", "TankClientAck");
const Packet auth_packet = recv_packet_logged(socket, logger_, "V", "TankAuth");
const Packet cert_packet = recv_packet_logged(socket, logger_, "V", "TankCert");
send_packet_logged(target.socket, packet, logger_, "V", "TankGameLoop");
```

- [ ] **Step 2: Replace client log labels**

In `src/game/tank_game_client.cpp`, replace every occurrence of these strings:

```text
AuthPlainGameTx
AuthPlainGameRx
AuthPlainGameAck
```

with:

```text
TankGameTx
TankGameRx
TankGameAck
```

The edited calls should include these exact labels:

```cpp
send_packet_logged(v_socket_, packet, logger_, "Client", "TankGameTx");
const Packet packet = recv_packet_logged(v_socket_, logger_, "Client", "TankGameRx");
log_verified_ack_packet(ack_logger_, "Client", "TankGameRx", packet);
send_packet_logged(v_socket_, ack, logger_, "Client", "TankGameAck");
logger_.write("Client", "TankGameRx", "ERROR", ex.what());
```

- [ ] **Step 3: Verify old live labels are gone**

Run:

```powershell
rg -n "PlainAccept|PlainClient|PlainClientAck|PlainGameAuth|PlainGameCert|PlainGameLoop|AuthPlainGameTx|AuthPlainGameRx|AuthPlainGameAck" src include tests README.md scripts docs/four-host-connect-test.md docs/implementation-plan.md docs/terminal-first-implementation-plan.md docs/uncovered-implementation-issues.md
```

Expected:

```text
<no output>
```

Do not include `docs/archive/` in this command; archived documents may contain historical names.

- [ ] **Step 4: Build targeted game executables**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client v_server tank_game_server_flow_selftest tank_game_client_options_selftest
```

Expected: build exits `0`.

- [ ] **Step 5: Run targeted tests**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw -R "tank_game_server_flow_selftest|tank_game_client_options_selftest|auth_encrypted_game_flow_selftest" --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 3
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add src/game/tank_game_server.cpp src/game/tank_game_client.cpp
git commit -m "refactor: rename tank runtime log labels"
```

---

### Task 2: Remove Duplicate Root Batch Wrappers

**Files:**

- Delete: `run_local.bat`
- Delete: `run_v.bat`
- Delete: `run_web.bat`
- Delete: `stop_local.bat`
- Modify: `README.md`
- Modify: `docs/four-host-connect-test.md`

- [ ] **Step 1: Delete root `.bat` wrappers**

Run:

```powershell
Remove-Item -LiteralPath .\run_local.bat -Force
Remove-Item -LiteralPath .\run_v.bat -Force
Remove-Item -LiteralPath .\run_web.bat -Force
Remove-Item -LiteralPath .\stop_local.bat -Force
```

- [ ] **Step 2: Remove root `.bat` alternatives from README**

In `README.md`, remove this block:

````markdown
or:

```cmd
run_local.bat
```
````

Remove this block:

````markdown
or:

```cmd
run_web.bat
```
````

Remove this block:

````markdown
or:

```cmd
stop_local.bat
```
````

Keep the PowerShell commands:

```powershell
.\scripts\run_local.ps1
.\scripts\run_web.ps1
.\scripts\stop_local.ps1
```

- [ ] **Step 3: Update the active four-host V helper command**

In `docs/four-host-connect-test.md`, replace:

```powershell
.\run_v.bat
```

with:

```powershell
.\scripts\run_v.ps1
```

- [ ] **Step 4: Verify active docs no longer reference root batch wrappers**

Run:

```powershell
rg -n "run_local\.bat|run_v\.bat|run_web\.bat|stop_local\.bat" README.md docs/four-host-connect-test.md docs/implementation-plan.md docs/terminal-first-implementation-plan.md docs/uncovered-implementation-issues.md scripts CMakeLists.txt
```

Expected:

```text
<no output>
```

- [ ] **Step 5: Verify the PowerShell scripts still exist**

Run:

```powershell
Test-Path .\scripts\run_local.ps1
Test-Path .\scripts\run_v.ps1
Test-Path .\scripts\run_web.ps1
Test-Path .\scripts\stop_local.ps1
```

Expected:

```text
True
True
True
True
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add README.md docs/four-host-connect-test.md
git add -u run_local.bat run_v.bat run_web.bat stop_local.bat
git commit -m "chore: remove duplicate batch wrappers"
```

---

### Task 3: Add Report-Facing Boundary Comments

**Files:**

- Modify: `include/cyber/common/auth_flow.hpp`
- Modify: `include/cyber/game/game_non_repudiation.hpp`
- Modify: `src/common/protocol_payloads.cpp`
- Modify: `src/game/app_payload_codec.cpp`
- Modify: `src/game/tank_game_server.cpp`
- Modify: `src/game/tank_game_client.cpp`

- [ ] **Step 1: Document Kerberos-to-game handoff**

In `include/cyber/common/auth_flow.hpp`, add this comment immediately before `struct VAuthenticatedSocket`:

```cpp
// Handoff from Kerberos to the tank game layer. The authenticated socket keeps
// the open V connection plus Kc_v and public keys needed by encrypted signed
// MSG_APP traffic.
```

- [ ] **Step 2: Document game non-repudiation envelope**

In `include/cyber/game/game_non_repudiation.hpp`, add this comment inside `namespace cyber::game`, immediately before `AppCode app_code_for_game_message_type(GameMsgType type);`:

```cpp
// Game non-repudiation uses one envelope shape: AppCode + GameMessage is
// signed, then that signed payload is optionally encrypted with Kc_v. The
// fixed Packet header stays unchanged for routing and monitor visualization.
```

In the same file, add this comment immediately before `Packet build_signed_ack_packet(`:

```cpp
// ACK evidence references the received wire payload by length and hash.
// APP_ACK packets are evidence records and are not acknowledged again.
```

- [ ] **Step 3: Document signed application payload input**

In `src/common/protocol_payloads.cpp`, add this comment immediately before `Bytes signed_app_logical_bytes(AppCode code, const Bytes& app_payload)`:

```cpp
// Signature input is the logical application bytes only: AppCode followed by
// app_payload. Packet header fields are not part of the RSA signature.
```

- [ ] **Step 4: Document payload codec boundary**

In `src/game/app_payload_codec.cpp`, add this comment immediately before `Bytes encode_app_payload(`:

```cpp
// The game encryption switch wraps only Packet.payload. MsgType, src, dst,
// payload_len, and reserved remain in the fixed network header.
```

- [ ] **Step 5: Document authoritative V handling**

In `src/game/tank_game_server.cpp`, add this comment immediately before `void TankGameServer::handle_packet(`:

```cpp
// V is authoritative for gameplay: it verifies signed client payloads, writes
// ACK evidence, applies valid inputs to BattleRoom, and broadcasts state.
```

- [ ] **Step 6: Document client send and receive boundaries**

In `src/game/tank_game_client.cpp`, add this comment immediately before `void TankGameClient::send_game_message(`:

```cpp
// Browser input becomes a signed game MSG_APP here. The browser still renders
// only the authoritative GAME_STATE that V sends back.
```

In the same file, add this comment immediately before `void TankGameClient::receive_loop()`:

```cpp
// The receive loop handles ACK evidence and authoritative state messages.
// Non-ACK game packets from V are acknowledged with signed APP_ACK.
```

- [ ] **Step 7: Verify comments are present and concise**

Run:

```powershell
rg -n "Handoff from Kerberos|Game non-repudiation uses one envelope|ACK evidence references|Signature input is the logical application bytes|game encryption switch wraps only Packet.payload|V is authoritative for gameplay|Browser input becomes a signed game MSG_APP|receive loop handles ACK evidence" include src
```

Expected: exactly eight matching lines.

- [ ] **Step 8: Commit**

Run:

```powershell
git add include/cyber/common/auth_flow.hpp include/cyber/game/game_non_repudiation.hpp src/common/protocol_payloads.cpp src/game/app_payload_codec.cpp src/game/tank_game_server.cpp src/game/tank_game_client.cpp
git commit -m "docs: comment core auth and game boundaries"
```

---

### Task 4: Final Verification

**Files:**

- No expected source edits.

- [ ] **Step 1: Run Web UI verification**

Run:

```powershell
npm run verify
```

Working directory:

```text
web-ui
```

Expected:

```text
protocolPayloadSelftest: ok
vite build completes with "built in"
```

- [ ] **Step 2: Run full C++ build**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
```

Expected: build exits `0`.

- [ ] **Step 3: Run full CTest suite**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 19
```

- [ ] **Step 4: Run cleanup reference checks**

Run:

```powershell
rg -n "PlainAccept|PlainClient|PlainClientAck|PlainGameAuth|PlainGameCert|PlainGameLoop|AuthPlainGameTx|AuthPlainGameRx|AuthPlainGameAck" src include tests README.md scripts docs/four-host-connect-test.md docs/implementation-plan.md docs/terminal-first-implementation-plan.md docs/uncovered-implementation-issues.md
rg -n "run_local\.bat|run_v\.bat|run_web\.bat|stop_local\.bat" README.md docs/four-host-connect-test.md docs/implementation-plan.md docs/terminal-first-implementation-plan.md docs/uncovered-implementation-issues.md scripts CMakeLists.txt
```

Expected:

```text
<no output from both commands>
```

- [ ] **Step 5: Check Git status**

Run:

```powershell
git status --short --branch
```

Expected:

```text
## main...origin/main [ahead 3]
?? docs/superpowers/plans/2026-05-17-report-readability-cleanup.md
?? image_processing_platform/
```

The untracked plan file is expected because this plan is saved before execution.
The untracked `image_processing_platform/` directory is outside this plan.

- [ ] **Step 6: Stop before push**

Do not push automatically in this plan. Ask the user whether to push the three commits after verification.
