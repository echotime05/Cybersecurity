# Codebase Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove unused application-layer code and reduce misleading legacy names/docs without changing the supported encrypted tank runtime.

**Architecture:** Keep the public runtime centered on `--game-auth-encrypted`. First finalize the already-identified dead `PlainGameClient` removal, then rename live game server/client classes so their names match their current responsibility, and finally archive historical planning docs that still describe removed plaintext modes. Do not remove the `PlainGameServer` behavior currently used by encrypted V; rename it instead.

**Tech Stack:** Windows C++17, CMake/Ninja, PowerShell, TypeScript/Vite Web UI.

---

## Scope

In scope:

- Delete the unused `PlainGameClient` class and source files.
- Rename live C++ game runtime classes:
  - `PlainGameServer` -> `TankGameServer`
  - `AuthPlainGameClient` -> `TankGameClient`
- Rename misleading tests and CMake targets that refer to `plain_game_*` or `auth_plain_game_client_*`.
- Archive historical Superpowers specs/plans that document removed modes such as `--game-plain` and `--game-auth-plain`.
- Keep tests passing after every code task.

Out of scope:

- Do not remove `--connect-test` or `--auth-test`; current CTest still uses them.
- Do not remove unauthenticated/plain server constructor paths from the game server in this pass; they still provide regression coverage for auth gating and encrypted plaintext rejection.
- Do not touch the untracked `image_processing_platform/` directory.
- Do not change packet formats, crypto, non-repudiation, UI behavior, or runtime ports.

## File Structure

Files to delete:

- `include/cyber/game/plain_game_client.hpp`: unused legacy plaintext client class.
- `src/game/plain_game_client.cpp`: unused legacy plaintext client implementation.

Files to rename:

- `include/cyber/game/plain_game_server.hpp` -> `include/cyber/game/tank_game_server.hpp`
- `src/game/plain_game_server.cpp` -> `src/game/tank_game_server.cpp`
- `include/cyber/game/auth_plain_game_client.hpp` -> `include/cyber/game/tank_game_client.hpp`
- `src/game/auth_plain_game_client.cpp` -> `src/game/tank_game_client.cpp`
- `tests/plain_game_flow_selftest.cpp` -> `tests/tank_game_server_flow_selftest.cpp`
- `tests/auth_plain_game_client_options_selftest.cpp` -> `tests/tank_game_client_options_selftest.cpp`

Files to modify:

- `CMakeLists.txt`: remove deleted source, update renamed source files, executable names, and test names.
- `src/common/role_runtime.cpp`: update includes and class names.
- Renamed C++ files listed above: update includes, class names, constructor/destructor names, and log labels.
- `README.md`: ensure only supported runtime is documented; no `--game-plain` or `--game-auth-plain` in public run instructions.
- `docs/superpowers/*`: archive historical plans/specs that mention removed modes.

---

### Task 1: Commit Unused PlainGameClient Removal

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `src/common/role_runtime.cpp`
- Delete: `include/cyber/game/plain_game_client.hpp`
- Delete: `src/game/plain_game_client.cpp`

- [ ] **Step 1: Confirm no live code needs PlainGameClient**

Run:

```powershell
rg -n "\bPlainGameClient\b|cyber/game/plain_game_client\.hpp|src/game/plain_game_client\.cpp" CMakeLists.txt include src tests README.md scripts
```

Expected after the existing deletion:

```text
<no output>
```

Do not include `docs/` in this check; historical plans may still mention the deleted class until Task 4.

- [ ] **Step 2: Confirm CMake no longer builds the deleted source**

`CMakeLists.txt` must keep this game source block:

```cmake
    src/game/app_payload_codec.cpp
    src/game/game_protocol.cpp
    src/game/game_non_repudiation.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
    src/game/plain_game_server.cpp
    src/game/auth_plain_game_client.cpp
```

It must not contain:

```cmake
    src/game/plain_game_client.cpp
```

- [ ] **Step 3: Confirm role runtime no longer includes the deleted header**

`src/common/role_runtime.cpp` must contain:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"
#include "cyber/game/plain_game_server.hpp"
```

It must not contain:

```cpp
#include "cyber/game/plain_game_client.hpp"
```

- [ ] **Step 4: Build**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
```

Expected: build exits `0`.

- [ ] **Step 5: Run CTest**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 19
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/common/role_runtime.cpp include/cyber/game/plain_game_client.hpp src/game/plain_game_client.cpp
git commit -m "refactor: remove unused plain game client"
```

---

### Task 2: Rename PlainGameServer To TankGameServer

**Files:**

- Move: `include/cyber/game/plain_game_server.hpp` -> `include/cyber/game/tank_game_server.hpp`
- Move: `src/game/plain_game_server.cpp` -> `src/game/tank_game_server.cpp`
- Move: `tests/plain_game_flow_selftest.cpp` -> `tests/tank_game_server_flow_selftest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/common/role_runtime.cpp`
- Modify: `tests/tank_game_server_flow_selftest.cpp`

- [ ] **Step 1: Move files**

Run:

```powershell
git mv include/cyber/game/plain_game_server.hpp include/cyber/game/tank_game_server.hpp
git mv src/game/plain_game_server.cpp src/game/tank_game_server.cpp
git mv tests/plain_game_flow_selftest.cpp tests/tank_game_server_flow_selftest.cpp
```

- [ ] **Step 2: Update CMake source and test names**

In `CMakeLists.txt`, replace the source entry:

```cmake
    src/game/plain_game_server.cpp
```

with:

```cmake
    src/game/tank_game_server.cpp
```

Replace the test target:

```cmake
add_executable(plain_game_flow_selftest tests/plain_game_flow_selftest.cpp)
target_link_libraries(plain_game_flow_selftest PRIVATE cyber_common)
```

with:

```cmake
add_executable(tank_game_server_flow_selftest tests/tank_game_server_flow_selftest.cpp)
target_link_libraries(tank_game_server_flow_selftest PRIVATE cyber_common)
```

Replace the CTest entry:

```cmake
add_test(NAME plain_game_flow_selftest COMMAND plain_game_flow_selftest)
```

with:

```cmake
add_test(NAME tank_game_server_flow_selftest COMMAND tank_game_server_flow_selftest)
```

- [ ] **Step 3: Update the header class name**

In `include/cyber/game/tank_game_server.hpp`, replace the class declaration with:

```cpp
class TankGameServer
{
public:
    explicit TankGameServer(TcpEndpoint endpoint);
    TankGameServer(TcpEndpoint endpoint, Config config, bool require_auth);
    TankGameServer(TcpEndpoint endpoint, Config config, bool require_auth,
                   bool encrypt_app_payloads);
    ~TankGameServer();

    void run();
    std::uint16_t start_for_test();
    void run_until_stopped();
    void stop();
```

Keep the existing private members and method declarations, but replace every `PlainGameServer` identifier in this header with `TankGameServer`.

- [ ] **Step 4: Update implementation include and method qualifiers**

In `src/game/tank_game_server.cpp`, replace:

```cpp
#include "cyber/game/plain_game_server.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_server.hpp"
```

Replace every `PlainGameServer::` method qualifier with `TankGameServer::`.

Constructor/destructor definitions must become:

```cpp
TankGameServer::TankGameServer(TcpEndpoint endpoint)
    : endpoint_(std::move(endpoint)),
      logger_(std::filesystem::path("logs") / "v_game.log"),
      ack_logger_(std::filesystem::path("logs") / "v_ack.log")
{
}

TankGameServer::TankGameServer(TcpEndpoint endpoint, Config config, bool require_auth)
    : TankGameServer(std::move(endpoint), std::move(config), require_auth, false)
{
}

TankGameServer::TankGameServer(TcpEndpoint endpoint, Config config, bool require_auth,
                               bool encrypt_app_payloads)
```

Keep the existing initializer list and function bodies after that third constructor signature.

- [ ] **Step 5: Update thread method pointer**

In `src/game/tank_game_server.cpp`, replace:

```cpp
client_threads_.emplace_back(&PlainGameServer::client_loop, this, accepted, peer);
```

with:

```cpp
client_threads_.emplace_back(&TankGameServer::client_loop, this, accepted, peer);
```

- [ ] **Step 6: Update role runtime include and construction**

In `src/common/role_runtime.cpp`, replace:

```cpp
#include "cyber/game/plain_game_server.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_server.hpp"
```

Replace:

```cpp
cyber::game::PlainGameServer server(bind_endpoint(config, spec), config, true, true);
```

with:

```cpp
cyber::game::TankGameServer server(bind_endpoint(config, spec), config, true, true);
```

- [ ] **Step 7: Update server selftest**

In `tests/tank_game_server_flow_selftest.cpp`, replace:

```cpp
#include "cyber/game/plain_game_server.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_server.hpp"
```

Replace every `cyber::game::PlainGameServer` with `cyber::game::TankGameServer`.

Replace the success line:

```cpp
std::cout << "plain_game_flow_selftest: ok\n";
```

with:

```cpp
std::cout << "tank_game_server_flow_selftest: ok\n";
```

Replace the failure line:

```cpp
std::cerr << "plain_game_flow_selftest failed: " << ex.what() << '\n';
```

with:

```cpp
std::cerr << "tank_game_server_flow_selftest failed: " << ex.what() << '\n';
```

- [ ] **Step 8: Verify no live old server names remain**

Run:

```powershell
rg -n "\bPlainGameServer\b|cyber/game/plain_game_server\.hpp|src/game/plain_game_server\.cpp|plain_game_flow_selftest" CMakeLists.txt include src tests README.md scripts
```

Expected:

```text
<no output>
```

- [ ] **Step 9: Build and run targeted test**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target tank_game_server_flow_selftest v_server
.\build-mingw\tank_game_server_flow_selftest.exe
```

Expected:

```text
tank_game_server_flow_selftest: ok
```

- [ ] **Step 10: Commit**

Run:

```powershell
git add CMakeLists.txt src/common/role_runtime.cpp include/cyber/game/tank_game_server.hpp src/game/tank_game_server.cpp tests/tank_game_server_flow_selftest.cpp
git add -u include/cyber/game/plain_game_server.hpp src/game/plain_game_server.cpp tests/plain_game_flow_selftest.cpp
git commit -m "refactor: rename tank game server"
```

---

### Task 3: Rename AuthPlainGameClient To TankGameClient

**Files:**

- Move: `include/cyber/game/auth_plain_game_client.hpp` -> `include/cyber/game/tank_game_client.hpp`
- Move: `src/game/auth_plain_game_client.cpp` -> `src/game/tank_game_client.cpp`
- Move: `tests/auth_plain_game_client_options_selftest.cpp` -> `tests/tank_game_client_options_selftest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/common/role_runtime.cpp`
- Modify: `tests/tank_game_client_options_selftest.cpp`

- [ ] **Step 1: Move files**

Run:

```powershell
git mv include/cyber/game/auth_plain_game_client.hpp include/cyber/game/tank_game_client.hpp
git mv src/game/auth_plain_game_client.cpp src/game/tank_game_client.cpp
git mv tests/auth_plain_game_client_options_selftest.cpp tests/tank_game_client_options_selftest.cpp
```

- [ ] **Step 2: Update CMake source and test names**

In `CMakeLists.txt`, replace:

```cmake
    src/game/auth_plain_game_client.cpp
```

with:

```cmake
    src/game/tank_game_client.cpp
```

Replace:

```cmake
add_executable(auth_plain_game_client_options_selftest
    tests/auth_plain_game_client_options_selftest.cpp)
target_link_libraries(auth_plain_game_client_options_selftest PRIVATE cyber_common)
```

with:

```cmake
add_executable(tank_game_client_options_selftest
    tests/tank_game_client_options_selftest.cpp)
target_link_libraries(tank_game_client_options_selftest PRIVATE cyber_common)
```

Replace:

```cmake
add_test(NAME auth_plain_game_client_options_selftest
         COMMAND auth_plain_game_client_options_selftest)
```

with:

```cmake
add_test(NAME tank_game_client_options_selftest
         COMMAND tank_game_client_options_selftest)
```

- [ ] **Step 3: Update client header class name**

In `include/cyber/game/tank_game_client.hpp`, replace:

```cpp
class AuthPlainGameClient
{
public:
    AuthPlainGameClient(Config config, std::uint16_t ui_port,
                        bool encrypt_app_payloads);
    ~AuthPlainGameClient();
```

with:

```cpp
class TankGameClient
{
public:
    TankGameClient(Config config, std::uint16_t ui_port, bool encrypt_app_payloads);
    ~TankGameClient();
```

Keep the existing private members and methods.

- [ ] **Step 4: Update implementation include and method qualifiers**

In `src/game/tank_game_client.cpp`, replace:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_client.hpp"
```

Replace every `AuthPlainGameClient::` with `TankGameClient::`.

Constructor/destructor definitions must begin:

```cpp
TankGameClient::TankGameClient(Config config, std::uint16_t ui_port,
                               bool encrypt_app_payloads)
```

and:

```cpp
TankGameClient::~TankGameClient()
```

- [ ] **Step 5: Update client log labels**

In `src/game/tank_game_client.cpp`, replace string labels:

```cpp
"AuthPlainGameClient"
```

with:

```cpp
"TankGameClient"
```

Do not rename thread names `AuthPlainGameTx`, `AuthPlainGameRx`, or `AuthPlainGameAck` in this task; those describe the authenticated game flow and are useful for continuity in existing logs.

- [ ] **Step 6: Update role runtime include and construction**

In `src/common/role_runtime.cpp`, replace:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_client.hpp"
```

Replace:

```cpp
cyber::game::AuthPlainGameClient client(config, ui_port, true);
```

with:

```cpp
cyber::game::TankGameClient client(config, ui_port, true);
```

- [ ] **Step 7: Update client options selftest**

In `tests/tank_game_client_options_selftest.cpp`, replace:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"
```

with:

```cpp
#include "cyber/game/tank_game_client.hpp"
```

Replace:

```cpp
cyber::game::AuthPlainGameClient encrypted(config, 7002, true);
std::cout << "auth_plain_game_client_options_selftest: ok\n";
```

with:

```cpp
cyber::game::TankGameClient encrypted(config, 7002, true);
std::cout << "tank_game_client_options_selftest: ok\n";
```

Replace:

```cpp
std::cerr << "auth_plain_game_client_options_selftest failed: " << ex.what() << '\n';
```

with:

```cpp
std::cerr << "tank_game_client_options_selftest failed: " << ex.what() << '\n';
```

- [ ] **Step 8: Verify no live old client names remain**

Run:

```powershell
rg -n "\bAuthPlainGameClient\b|cyber/game/auth_plain_game_client\.hpp|src/game/auth_plain_game_client\.cpp|auth_plain_game_client_options_selftest" CMakeLists.txt include src tests README.md scripts
```

Expected:

```text
<no output>
```

- [ ] **Step 9: Build and run targeted test**

Run:

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target tank_game_client_options_selftest client
.\build-mingw\tank_game_client_options_selftest.exe
```

Expected:

```text
tank_game_client_options_selftest: ok
```

- [ ] **Step 10: Commit**

Run:

```powershell
git add CMakeLists.txt src/common/role_runtime.cpp include/cyber/game/tank_game_client.hpp src/game/tank_game_client.cpp tests/tank_game_client_options_selftest.cpp
git add -u include/cyber/game/auth_plain_game_client.hpp src/game/auth_plain_game_client.cpp tests/auth_plain_game_client_options_selftest.cpp
git commit -m "refactor: rename tank game client"
```

---

### Task 4: Archive Historical Superpowers Plans And Specs

**Files:**

- Create: `docs/archive/superpowers/plans/`
- Create: `docs/archive/superpowers/specs/`
- Move historical plans/specs listed below.
- Create: `docs/archive/superpowers/README.md`

- [ ] **Step 1: Create archive directories**

Run:

```powershell
New-Item -ItemType Directory -Force docs\archive\superpowers\plans | Out-Null
New-Item -ItemType Directory -Force docs\archive\superpowers\specs | Out-Null
```

- [ ] **Step 2: Move historical plans**

Run:

```powershell
git mv docs\superpowers\plans\2026-05-11-kerberos-non-repudiation.md docs\archive\superpowers\plans\2026-05-11-kerberos-non-repudiation.md
git mv docs\superpowers\plans\2026-05-14-plaintext-tank-application.md docs\archive\superpowers\plans\2026-05-14-plaintext-tank-application.md
git mv docs\superpowers\plans\2026-05-14-kerberos-gated-tank-ui.md docs\archive\superpowers\plans\2026-05-14-kerberos-gated-tank-ui.md
git mv docs\superpowers\plans\2026-05-14-encrypted-game-payloads.md docs\archive\superpowers\plans\2026-05-14-encrypted-game-payloads.md
```

Leave the current cleanup plan under `docs/superpowers/plans/`.

- [ ] **Step 3: Move historical specs**

Run:

```powershell
git mv docs\superpowers\specs\2026-05-11-kerberos-non-repudiation-design.md docs\archive\superpowers\specs\2026-05-11-kerberos-non-repudiation-design.md
git mv docs\superpowers\specs\2026-05-14-plaintext-tank-application-design.md docs\archive\superpowers\specs\2026-05-14-plaintext-tank-application-design.md
git mv docs\superpowers\specs\2026-05-14-kerberos-gated-tank-ui-design.md docs\archive\superpowers\specs\2026-05-14-kerberos-gated-tank-ui-design.md
git mv docs\superpowers\specs\2026-05-14-encrypted-game-payloads-design.md docs\archive\superpowers\specs\2026-05-14-encrypted-game-payloads-design.md
```

- [ ] **Step 4: Add archive README**

Create `docs/archive/superpowers/README.md` with:

````markdown
# Archived Superpowers Documents

These plans and specs describe intermediate implementation stages. They are kept for traceability, but they are not the current runbook.

Use the repository root `README.md` for current build, runtime, and deployment commands.

Current supported runtime:

```text
as_server.exe --serve
tgs_server.exe --serve
v_server.exe --game-auth-encrypted
client.exe --game-auth-encrypted --ui-port 7001
monitor.exe --ui-port 7010
```
````

- [ ] **Step 5: Verify public docs no longer expose removed modes**

Run:

```powershell
rg -n -- "--game-plain|--game-auth-plain" README.md docs/four-host-connect-test.md docs/implementation-plan.md docs/terminal-first-implementation-plan.md docs/uncovered-implementation-issues.md
```

Expected:

```text
<no output>
```

If output appears in one of those non-archive docs, edit that file to either remove the removed command or explicitly say it is archived and unsupported.

- [ ] **Step 6: Commit**

Run:

```powershell
git add docs/archive docs/superpowers
git commit -m "docs: archive historical implementation plans"
```

---

### Task 5: Final Verification

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

If Task 2 and Task 3 rename test targets, the total test count should remain the same unless additional tests are intentionally added.

- [ ] **Step 4: Run source reference checks**

Run:

```powershell
rg -n "\bPlainGameClient\b|cyber/game/plain_game_client\.hpp|src/game/plain_game_client\.cpp" CMakeLists.txt include src tests README.md scripts
rg -n "\bPlainGameServer\b|cyber/game/plain_game_server\.hpp|src/game/plain_game_server\.cpp|plain_game_flow_selftest" CMakeLists.txt include src tests README.md scripts
rg -n "\bAuthPlainGameClient\b|cyber/game/auth_plain_game_client\.hpp|src/game/auth_plain_game_client\.cpp|auth_plain_game_client_options_selftest" CMakeLists.txt include src tests README.md scripts
```

Expected:

```text
<no output from all three commands>
```

- [ ] **Step 5: Check Git status**

Run:

```powershell
git status --short --branch
```

Expected:

```text
## main...origin/main
?? image_processing_platform/
```

The untracked `image_processing_platform/` directory is outside this cleanup plan.

- [ ] **Step 6: Push**

Run:

```powershell
git push origin main
```

Expected: `main -> main` push succeeds.
