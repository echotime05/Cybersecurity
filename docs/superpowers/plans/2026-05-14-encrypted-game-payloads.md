# Encrypted Game Payloads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an independent `--game-auth-encrypted` tank mode that encrypts only C++ client-to-V `MsgType::app` game payloads with the Kerberos `Kc_v` session key.

**Architecture:** Keep packet headers, Kerberos flow, Web UI, and BattleRoom unchanged. Add a small game app-payload codec, then reuse the existing authenticated gameplay client/server with an `encrypt_app_payloads` flag. V stores each connected client's `Kc_v` and encrypts broadcasts separately per recipient.

**Tech Stack:** C++17, CMake/Ninja, Windows sockets, existing DES helpers in `cyber/common/crypto.hpp`, existing PowerShell CTest flow tests, Vite Web UI unchanged.

---

## File Structure

Create:

- `include/cyber/game/app_payload_codec.hpp`: Pure helpers for encoding and decoding logical game app payloads.
- `src/game/app_payload_codec.cpp`: DES-or-identity implementation of the helpers.
- `tests/app_payload_codec_selftest.cpp`: Fast unit tests for plaintext/encrypted roundtrips.
- `tests/auth_encrypted_game_flow_selftest.ps1`: End-to-end login/join/state test for `--game-auth-encrypted`.
- `tests/encrypted_plaintext_rejection_client.cpp`: Test helper that authenticates to V, then deliberately sends a plaintext game join over the encrypted mode socket.

Modify:

- `CMakeLists.txt`: Add the new source file, test executable, and CTest PowerShell test.
- `include/cyber/game/plain_game_server.hpp`: Add `encrypt_app_payloads` mode and `kc_v` per connection.
- `src/game/plain_game_server.cpp`: Decode incoming app payloads and encode outgoing state payloads when encryption is enabled.
- `include/cyber/game/auth_plain_game_client.hpp`: Add `encrypt_app_payloads` mode and stored `kc_v`.
- `src/game/auth_plain_game_client.cpp`: Encode outgoing game messages and decode incoming state messages when encryption is enabled.
- `src/common/role_runtime.cpp`: Parse and dispatch `--game-auth-encrypted`.
- `README.md`: Document how to run the encrypted authenticated mode.

Do not modify:

- `web-ui/*`: Browser communication stays plaintext local WebSocket.
- Packet header serialization in `packet.hpp/cpp`: Header remains unchanged.
- Kerberos payload formats: Existing AS/TGS/V auth encryption remains as-is.

---

### Task 1: Add Game App Payload Codec

**Files:**

- Create: `include/cyber/game/app_payload_codec.hpp`
- Create: `src/game/app_payload_codec.cpp`
- Create: `tests/app_payload_codec_selftest.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing codec selftest**

Create `tests/app_payload_codec_selftest.cpp`:

```cpp
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_protocol.hpp"

#include <cstdint>
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
        const std::uint64_t kc_v = 0x123456789abcdeULL;
        const cyber::Bytes plain = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "Client1"})});

        const cyber::Bytes identity =
            cyber::game::encode_app_payload(plain, kc_v, false);
        require(identity == plain, "plaintext codec should not change payload");
        require(cyber::game::decode_app_payload(identity, kc_v, false) == plain,
                "plaintext codec should roundtrip");

        const cyber::Bytes cipher =
            cyber::game::encode_app_payload(plain, kc_v, true);
        require(cipher != plain, "encrypted codec should change payload bytes");
        require(cipher.size() % 8U == 0U, "encrypted codec should produce DES blocks");

        const cyber::Bytes decoded =
            cyber::game::decode_app_payload(cipher, kc_v, true);
        require(decoded == plain, "encrypted codec should roundtrip");

        const cyber::game::GameMessage parsed =
            cyber::game::parse_game_message(decoded);
        require(parsed.type == cyber::game::GameMsgType::join,
                "decoded game message type mismatch");

        bool plaintext_rejected_by_encrypted_decoder = false;
        try
        {
            (void)cyber::game::decode_app_payload(plain, kc_v, true);
        }
        catch (const std::exception&)
        {
            plaintext_rejected_by_encrypted_decoder = true;
        }
        require(plaintext_rejected_by_encrypted_decoder,
                "encrypted decoder accepted plaintext payload");

        std::cout << "app_payload_codec_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "app_payload_codec_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the test before implementation**

Modify `CMakeLists.txt`:

```cmake
add_executable(app_payload_codec_selftest tests/app_payload_codec_selftest.cpp)
target_link_libraries(app_payload_codec_selftest PRIVATE cyber_common)
```

Add the CTest entry near the other fast selftests:

```cmake
add_test(NAME app_payload_codec_selftest COMMAND app_payload_codec_selftest)
```

- [ ] **Step 3: Run the focused build and confirm failure**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build-mingw -G Ninja -DCMAKE_MAKE_PROGRAM='E:\Qt\Tools\Ninja\ninja.exe' -DCMAKE_CXX_COMPILER='E:\Qt\Tools\mingw1120_64\bin\g++.exe'
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target app_payload_codec_selftest
```

Expected result: compile fails because `cyber/game/app_payload_codec.hpp` does not exist.

- [ ] **Step 4: Add the codec header**

Create `include/cyber/game/app_payload_codec.hpp`:

```cpp
#pragma once

#include "cyber/common/packet.hpp"

#include <cstdint>

namespace cyber::game
{
Bytes encode_app_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted);
Bytes decode_app_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted);
} // namespace cyber::game
```

- [ ] **Step 5: Add the codec implementation**

Create `src/game/app_payload_codec.cpp`:

```cpp
#include "cyber/game/app_payload_codec.hpp"

#include "cyber/common/crypto.hpp"

namespace cyber::game
{
Bytes encode_app_payload(const Bytes& plain, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return plain;
    }
    return des_encrypt_payload(plain, kc_v);
}

Bytes decode_app_payload(const Bytes& wire, std::uint64_t kc_v, bool encrypted)
{
    if (!encrypted)
    {
        return wire;
    }
    return des_decrypt_payload(wire, kc_v);
}
} // namespace cyber::game
```

- [ ] **Step 6: Add the implementation file to the common library**

Modify the `add_library(cyber_common ...)` block in `CMakeLists.txt` and add:

```cmake
    src/game/app_payload_codec.cpp
```

Place it beside the existing game source files:

```cmake
    src/game/game_protocol.cpp
    src/game/app_payload_codec.cpp
    src/game/game_world.cpp
```

- [ ] **Step 7: Run the focused test**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target app_payload_codec_selftest
.\build-mingw\app_payload_codec_selftest.exe
```

Expected output:

```text
app_payload_codec_selftest: ok
```

- [ ] **Step 8: Commit**

Run:

```powershell
git add CMakeLists.txt include/cyber/game/app_payload_codec.hpp src/game/app_payload_codec.cpp tests/app_payload_codec_selftest.cpp
git commit -m "feat: add game app payload codec"
```

---

### Task 2: Wire Codec Into PlainGameServer

**Files:**

- Modify: `include/cyber/game/plain_game_server.hpp`
- Modify: `src/game/plain_game_server.cpp`
- Modify: `tests/plain_game_flow_selftest.cpp`

- [ ] **Step 1: Write failing server encryption coverage**

Modify `tests/plain_game_flow_selftest.cpp`.

Add this include:

```cpp
#include "cyber/game/app_payload_codec.hpp"
```

Add this helper inside the anonymous namespace:

```cpp
bool wait_for_state(cyber::SocketHandle client, cyber::Logger& logger, std::uint64_t kc_v,
                    bool encrypted)
{
    for (int i = 0; i < 10; ++i)
    {
        const cyber::Packet packet =
            cyber::recv_packet_logged(client, logger, "Client", "PlainGameTest");
        const cyber::Bytes plain =
            cyber::game::decode_app_payload(packet.payload, kc_v, encrypted);
        const cyber::game::GameMessage message = cyber::game::parse_game_message(plain);
        if (message.type == cyber::game::GameMsgType::state)
        {
            const cyber::game::BattleStateSnapshot state =
                cyber::game::parse_state(message.payload);
            if (!state.tanks.empty())
            {
                return true;
            }
        }
    }
    return false;
}
```

Replace the existing repeated state receive loop in the first plaintext test with:

```cpp
require(wait_for_state(client, client_logger, 0, false),
        "did not receive state with joined tank");
```

After the existing auth-gated unauthenticated rejection block, add this encrypted server block:

```cpp
        cyber::game::PlainGameServer encrypted_server({"127.0.0.1", 0}, config, false, true);
        const std::uint16_t encrypted_port = encrypted_server.start_for_test();
        std::thread encrypted_thread([&]() { encrypted_server.run_until_stopped(); });

        cyber::SocketHandle encrypted_client =
            cyber::connect_tcp({"127.0.0.1", encrypted_port});
        const auto encrypted_join_plain = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "EncryptedClient"})});
        const cyber::Bytes encrypted_join =
            cyber::game::encode_app_payload(encrypted_join_plain, 0, true);
        cyber::send_packet_logged(encrypted_client,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     encrypted_join),
                                  client_logger, "Client", "EncryptedGameTest");
        require(wait_for_state(encrypted_client, client_logger, 0, true),
                "encrypted server did not return encrypted state");
        cyber::close_socket(encrypted_client);
        encrypted_server.stop();
        encrypted_thread.join();

        cyber::game::PlainGameServer reject_plain_server({"127.0.0.1", 0}, config, false, true);
        const std::uint16_t reject_plain_port = reject_plain_server.start_for_test();
        std::thread reject_plain_thread([&]() { reject_plain_server.run_until_stopped(); });

        cyber::SocketHandle plaintext_client =
            cyber::connect_tcp({"127.0.0.1", reject_plain_port});
        cyber::send_packet_logged(plaintext_client,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     encrypted_join_plain),
                                  client_logger, "Client", "EncryptedRejectPlainTest");
        bool plaintext_closed_or_failed = false;
        try
        {
            (void)cyber::recv_packet_logged(plaintext_client, client_logger, "Client",
                                            "EncryptedRejectPlainTest");
        }
        catch (const std::exception&)
        {
            plaintext_closed_or_failed = true;
        }
        require(plaintext_closed_or_failed,
                "encrypted server accepted plaintext game payload");
        cyber::close_socket(plaintext_client);
        reject_plain_server.stop();
        reject_plain_thread.join();
```

- [ ] **Step 2: Run the focused test and confirm failure**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target plain_game_flow_selftest
```

Expected result: compile fails because `PlainGameServer(endpoint, config, false, true)` does not exist.

- [ ] **Step 3: Extend the server header**

Modify `include/cyber/game/plain_game_server.hpp`:

```cpp
    PlainGameServer(TcpEndpoint endpoint, Config config, bool require_auth,
                    bool encrypt_app_payloads = false);
```

Extend `ClientConnection`:

```cpp
    struct ClientConnection
    {
        SocketHandle socket = 0;
        EntityId client_id = EntityId::unknown;
        std::uint64_t kc_v = 0;
    };
```

Change private method declarations:

```cpp
    void handle_packet(SocketHandle socket, const Packet& packet, std::uint64_t kc_v);
    bool authenticate_socket(SocketHandle socket, const std::string& peer, EntityId& client_id,
                             std::uint64_t& kc_v);
```

Add the mode member after `require_auth_`:

```cpp
    bool encrypt_app_payloads_ = false;
```

- [ ] **Step 4: Extend the server implementation**

Modify `src/game/plain_game_server.cpp`.

Add the codec include:

```cpp
#include "cyber/game/app_payload_codec.hpp"
```

Replace the auth constructor implementation with:

```cpp
PlainGameServer::PlainGameServer(TcpEndpoint endpoint, Config config, bool require_auth,
                                 bool encrypt_app_payloads)
    : endpoint_(std::move(endpoint)),
      config_(std::move(config)),
      require_auth_(require_auth),
      encrypt_app_payloads_(encrypt_app_payloads),
      auth_runtime_(make_auth_runtime(config_)),
      logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}
```

In `client_loop`, track the key:

```cpp
        EntityId authenticated_client = EntityId::unknown;
        std::uint64_t kc_v = 0;
        if (require_auth_ && !authenticate_socket(socket, peer, authenticated_client, kc_v))
        {
            close_socket(socket);
            logger_.write("V", "PlainClient", "THREAD_EXIT", "client " + peer);
            return;
        }
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(socket, logger_, "V", "PlainClient");
            if (require_auth_ && packet.src != authenticated_client)
            {
                throw std::runtime_error("authenticated client id mismatch");
            }
            handle_packet(socket, packet, kc_v);
        }
```

Change `handle_packet` to decode before parsing:

```cpp
void PlainGameServer::handle_packet(SocketHandle socket, const Packet& packet,
                                    std::uint64_t kc_v)
{
    if (packet.msg_type != MsgType::app)
    {
        logger_.write("V", "PlainClient", "ERROR", "non-app packet ignored");
        return;
    }

    const Bytes plain_payload =
        decode_app_payload(packet.payload, kc_v, encrypt_app_payloads_);
    const GameMessage message = parse_game_message(plain_payload);
```

When storing the joined connection, preserve the key:

```cpp
        connections_[join.client_id] = {socket, join.client_id, kc_v};
```

Change `authenticate_socket` to return `kc_v`:

```cpp
bool PlainGameServer::authenticate_socket(SocketHandle socket, const std::string& peer,
                                          EntityId& client_id, std::uint64_t& kc_v)
{
    const Packet auth_packet = recv_packet_logged(socket, logger_, "V", "PlainGameAuth");
    if (auth_packet.msg_type != MsgType::v_auth_req || !is_client(auth_packet.src))
    {
        logger_.write("V", "PlainGameAuth", "ERROR",
                      "client " + peer + " sent app traffic before V_AUTH");
        return false;
    }
    const Packet response =
        process_v_auth_request(auth_packet, config_, auth_runtime_, logger_, "PlainGameAuth");
    send_packet_logged(socket, response, logger_, "V", "PlainGameAuth");
    const AuthSession session = auth_runtime_.v_sessions.get(auth_packet.src);
    client_id = auth_packet.src;
    kc_v = session.kc_v;
    return true;
}
```

Change `broadcast` to encrypt per recipient:

```cpp
    const Bytes plain_payload = build_game_message({GameMsgType::state, build_state(snapshot)});
    std::vector<EntityId> failed;
    for (const ClientConnection& target : targets)
    {
        try
        {
            const Bytes wire_payload =
                encode_app_payload(plain_payload, target.kc_v, encrypt_app_payloads_);
            send_packet_logged(target.socket,
                               make_packet(MsgType::app, EntityId::v, target.client_id,
                                           wire_payload),
                               logger_, "V", "PlainGameLoop");
        }
```

- [ ] **Step 5: Run the focused server test**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target plain_game_flow_selftest
.\build-mingw\plain_game_flow_selftest.exe
```

Expected output:

```text
plain_game_flow_selftest: ok
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add include/cyber/game/plain_game_server.hpp src/game/plain_game_server.cpp tests/plain_game_flow_selftest.cpp
git commit -m "feat: encrypt game payloads on server"
```

---

### Task 3: Wire Codec Into Authenticated Game Client

**Files:**

- Modify: `include/cyber/game/auth_plain_game_client.hpp`
- Modify: `src/game/auth_plain_game_client.cpp`

- [ ] **Step 1: Extend the client header**

Modify `include/cyber/game/auth_plain_game_client.hpp`.

Change the constructor:

```cpp
    AuthPlainGameClient(Config config, std::uint16_t ui_port,
                        bool encrypt_app_payloads = false);
```

Add members after `v_socket_`:

```cpp
    std::uint64_t kc_v_ = 0;
    bool encrypt_app_payloads_ = false;
```

- [ ] **Step 2: Extend the client implementation**

Modify `src/game/auth_plain_game_client.cpp`.

Add the codec include:

```cpp
#include "cyber/game/app_payload_codec.hpp"
```

Change the constructor:

```cpp
AuthPlainGameClient::AuthPlainGameClient(Config config, std::uint16_t ui_port,
                                         bool encrypt_app_payloads)
    : config_(std::move(config)),
      ui_port_(ui_port),
      encrypt_app_payloads_(encrypt_app_payloads),
      logger_(std::filesystem::path("logs") / "client_auth_plain_game.log")
{
}
```

In `handle_login`, store the V session key after authentication:

```cpp
            self_ = command.client_id;
            kc_v_ = auth.state.kc_v;
            v_socket_ = auth.socket;
            state_ = State::authenticated;
```

In the login failure path, clear the key:

```cpp
            kc_v_ = 0;
            state_ = State::waiting_for_login;
```

In `send_game_message`, encode the payload:

```cpp
    const Bytes message = build_game_message({type, payload});
    const Bytes wire_payload = encode_app_payload(message, kc_v_, encrypt_app_payloads_);
    const Packet packet = make_packet(MsgType::app, self_, EntityId::v, wire_payload);
```

In `receive_loop`, decode before parsing:

```cpp
            const Bytes plain_payload =
                decode_app_payload(packet.payload, kc_v_, encrypt_app_payloads_);
            const GameMessage message = parse_game_message(plain_payload);
```

In `close_v_socket`, clear only the socket. Keep `kc_v_` clearing inside login state transitions so a harmless close during shutdown does not race with state updates:

```cpp
void AuthPlainGameClient::close_v_socket()
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
}
```

- [ ] **Step 3: Build client and existing authenticated flow**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client v_server plain_game_flow_selftest
.\build-mingw\plain_game_flow_selftest.exe
```

Expected output:

```text
plain_game_flow_selftest: ok
```

- [ ] **Step 4: Commit**

Run:

```powershell
git add include/cyber/game/auth_plain_game_client.hpp src/game/auth_plain_game_client.cpp
git commit -m "feat: encrypt game payloads on client"
```

---

### Task 4: Add `--game-auth-encrypted` Command Mode

**Files:**

- Modify: `src/common/role_runtime.cpp`
- Modify: `README.md`

- [ ] **Step 1: Add command parsing and dispatch**

Modify `src/common/role_runtime.cpp`.

Update usage:

```cpp
                 " [--game-plain] [--game-auth-plain] [--game-auth-encrypted]"
                 " [--ui-port PORT]\n";
```

Add the flag near `game_auth_plain`:

```cpp
    bool game_auth_plain = false;
    bool game_auth_encrypted = false;
```

Parse the new argument:

```cpp
        else if (arg == "--game-auth-encrypted")
        {
            game_auth_encrypted = true;
        }
```

After argument parsing and before mode dispatch, reject conflicting game modes:

```cpp
    const int selected_game_modes = (game_plain ? 1 : 0) + (game_auth_plain ? 1 : 0) +
                                    (game_auth_encrypted ? 1 : 0);
    if (selected_game_modes > 1)
    {
        std::cerr << "select only one game mode\n";
        return 2;
    }
```

Replace the current `if (game_auth_plain)` block with:

```cpp
        if (game_auth_plain || game_auth_encrypted)
        {
            const bool encrypt_app_payloads = game_auth_encrypted;
            const char* mode_name =
                encrypt_app_payloads ? "--game-auth-encrypted" : "--game-auth-plain";
            if (role == RoleKind::v_server)
            {
                SocketRuntime runtime;
                cyber::game::PlainGameServer server(bind_endpoint(config, spec), config, true,
                                                     encrypt_app_payloads);
                server.run();
                return 0;
            }
            if (role == RoleKind::client)
            {
                if (ui_port == 0)
                {
                    throw std::runtime_error(std::string("client ") + mode_name +
                                             " requires --ui-port");
                }
                cyber::game::AuthPlainGameClient client(config, ui_port, encrypt_app_payloads);
                client.run();
                return 0;
            }
            throw std::runtime_error(std::string(mode_name) +
                                     " is only supported by v_server and client");
        }
```

- [ ] **Step 2: Build role binaries**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client v_server
.\build-mingw\client.exe --help
```

Expected `--help` output includes:

```text
--game-auth-encrypted
```

- [ ] **Step 3: Update README commands**

Modify `README.md` near the authenticated plaintext game instructions. Add:

````markdown
Encrypted authenticated game mode:

```powershell
.\build-mingw\as_server.exe --serve
.\build-mingw\tgs_server.exe --serve
.\build-mingw\v_server.exe --game-auth-encrypted
.\build-mingw\client.exe --game-auth-encrypted --ui-port 7001
```

Then open the same Web UI URL used by the authenticated plaintext mode. The browser-to-local-client WebSocket remains plaintext; only the C++ client-to-V `MsgType::app` game payload is encrypted with `Kc_v`.
````

- [ ] **Step 4: Commit**

Run:

```powershell
git add src/common/role_runtime.cpp README.md
git commit -m "feat: add encrypted authenticated game mode"
```

---

### Task 5: Add Encrypted Gameplay E2E Test

**Files:**

- Create: `tests/auth_encrypted_game_flow_selftest.ps1`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the encrypted PowerShell flow test**

Create `tests/auth_encrypted_game_flow_selftest.ps1` by copying `tests/auth_plain_game_flow_selftest.ps1`, then make these exact changes:

Change the temp directory prefix:

```powershell
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_auth_encrypted_game_" + [Guid]::NewGuid().ToString('N'))
```

Change AS and TGS max connection counts so the later rejection helper can reuse the same security services:

```powershell
$processes += Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '3') 'as_server' $tmp
$processes += Start-RoleProcess 'tgs_server.exe' @('--config', $config, '--serve', '--max-connections', '2') 'tgs_server' $tmp
```

Change V and client modes:

```powershell
$processes += Start-RoleProcess 'v_server.exe' @('--config', $config, '--game-auth-encrypted') 'v_server' $tmp
$processes += Start-RoleProcess 'client.exe' @('--config', $config, '--game-auth-encrypted', '--ui-port', "$uiPort") 'client' $tmp
```

Change the success line:

```powershell
Write-Host 'auth_encrypted_game_flow_selftest: ok'
```

Keep the WebSocket login, wrong password, correct password, pre-join no-state check, join, and state assertions unchanged.

- [ ] **Step 2: Register the encrypted flow test**

Modify `CMakeLists.txt` and add:

```cmake
add_test(
    NAME auth_encrypted_game_flow_selftest
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_encrypted_game_flow_selftest.ps1
            -BuildDir $<TARGET_FILE_DIR:client>
)
```

Place it after `auth_plain_game_flow_selftest`.

- [ ] **Step 3: Run the encrypted flow test**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client v_server as_server tgs_server
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\auth_encrypted_game_flow_selftest.ps1 -BuildDir .\build-mingw
```

Expected output:

```text
auth_encrypted_game_flow_selftest: ok
```

- [ ] **Step 4: Commit**

Run:

```powershell
git add CMakeLists.txt tests/auth_encrypted_game_flow_selftest.ps1
git commit -m "test: cover encrypted authenticated game flow"
```

---

### Task 6: Add Real V_AUTH Plaintext Rejection Test

**Files:**

- Create: `tests/encrypted_plaintext_rejection_client.cpp`
- Modify: `tests/auth_encrypted_game_flow_selftest.ps1`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the rejection helper**

Create `tests/encrypted_plaintext_rejection_client.cpp`:

```cpp
#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_protocol.hpp"

#include <filesystem>
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

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: encrypted_plaintext_rejection_client CONFIG\n";
        return 2;
    }

    try
    {
        cyber::SocketRuntime runtime;
        const cyber::Config config = cyber::Config::load(argv[1]);
        cyber::Logger logger(std::filesystem::temp_directory_path() /
                             "encrypted_plaintext_rejection_client.log");

        const cyber::EntityId client_id = cyber::EntityId::client1;
        const std::uint64_t kc = cyber::derive_client_key(client_id, "123456");
        cyber::VAuthenticatedSocket auth = cyber::authenticate_client_to_v_socket(
            config, client_id, kc, logger, "EncryptedPlainReject");

        const cyber::Bytes plaintext_join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({client_id, "PlaintextShouldFail"})});

        cyber::send_packet_logged(auth.socket,
                                  cyber::make_packet(cyber::MsgType::app, client_id,
                                                     cyber::EntityId::v, plaintext_join),
                                  logger, "Client", "EncryptedPlainReject");

        bool rejected = false;
        try
        {
            const cyber::Packet response =
                cyber::recv_packet_logged(auth.socket, logger, "Client",
                                          "EncryptedPlainReject");
            if (response.msg_type != cyber::MsgType::app)
            {
                rejected = true;
            }
            else
            {
                try
                {
                    const cyber::game::GameMessage message =
                        cyber::game::parse_game_message(response.payload);
                    rejected = message.type != cyber::game::GameMsgType::state;
                }
                catch (const std::exception&)
                {
                    rejected = true;
                }
            }
        }
        catch (const std::exception&)
        {
            rejected = true;
        }

        cyber::close_socket(auth.socket);
        require(rejected, "encrypted V accepted plaintext MsgType::app payload");
        std::cout << "encrypted_plaintext_rejection_client: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "encrypted_plaintext_rejection_client failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

- [ ] **Step 2: Register the helper executable**

Modify `CMakeLists.txt`:

```cmake
add_executable(encrypted_plaintext_rejection_client tests/encrypted_plaintext_rejection_client.cpp)
target_link_libraries(encrypted_plaintext_rejection_client PRIVATE cyber_common)
```

Do not add this executable as a direct CTest test. It needs AS/TGS/V running, and the PowerShell E2E test owns that setup.

- [ ] **Step 3: Call the helper from the encrypted flow test**

Modify `tests/auth_encrypted_game_flow_selftest.ps1`.

After the WebSocket state assertion and before `Write-Host`, add:

```powershell
    & (Join-Path $BuildDir 'encrypted_plaintext_rejection_client.exe') $config
    if ($LASTEXITCODE -ne 0) {
        throw "encrypted_plaintext_rejection_client failed"
    }
```

Because this helper consumes one more AS request and one more TGS request, keep the AS/TGS max connection counts from Task 5:

```powershell
--max-connections 3
--max-connections 2
```

- [ ] **Step 4: Run the encrypted flow with rejection helper**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target encrypted_plaintext_rejection_client client v_server as_server tgs_server
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\auth_encrypted_game_flow_selftest.ps1 -BuildDir .\build-mingw
```

Expected output includes both lines:

```text
encrypted_plaintext_rejection_client: ok
auth_encrypted_game_flow_selftest: ok
```

- [ ] **Step 5: Commit**

Run:

```powershell
git add CMakeLists.txt tests/encrypted_plaintext_rejection_client.cpp tests/auth_encrypted_game_flow_selftest.ps1
git commit -m "test: reject plaintext app payloads in encrypted mode"
```

---

### Task 7: Full Verification

**Files:**

- No planned source edits.

- [ ] **Step 1: Configure from a clean CMake state**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build-mingw -G Ninja -DCMAKE_MAKE_PROGRAM='E:\Qt\Tools\Ninja\ninja.exe' -DCMAKE_CXX_COMPILER='E:\Qt\Tools\mingw1120_64\bin\g++.exe'
```

Expected output includes:

```text
Build files have been written to: E:/zhuomian/cybersecurity/code/build-mingw
```

- [ ] **Step 2: Build all C++ targets**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
```

Expected result: exit code `0`.

- [ ] **Step 3: Run all CTest tests**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw --output-on-failure
```

Expected result after these tasks: all tests pass. The count should be at least `17/17` because this plan adds `app_payload_codec_selftest` and `auth_encrypted_game_flow_selftest` to the existing `15` tests.

- [ ] **Step 4: Run the Web UI production build**

Run:

```powershell
$env:PATH='E:\zhuomian\tools\node-v24.15.0-win-x64;' + $env:PATH
npm run build
```

Working directory:

```text
E:\zhuomian\cybersecurity\code\web-ui
```

Expected result: Vite build exits `0`. A chunk-size warning is acceptable because it already exists and is unrelated to this backend encryption change.

- [ ] **Step 5: Inspect final git state**

Run:

```powershell
git status --short --branch
git log --oneline --decorate --max-count=8
```

Expected result:

```text
## feature/encrypted-game-payloads
```

The log should show the task commits above on top of:

```text
docs: design encrypted game payloads
```

---

## Spec Coverage Self-Review

- Independent mode: Task 4 adds `--game-auth-encrypted`.
- Header unchanged: Tasks 2 and 3 only wrap `packet.payload`; no packet serialization files are touched.
- `Kc_v` from Kerberos: Task 2 reads it from `AuthSessionTable`; Task 3 stores it from `VAuthenticatedSocket`.
- Web UI remains plaintext local WebSocket: No `web-ui/*` files are in the plan.
- Decryption failures stay out of UI: Task 2 and Task 3 handle failures through exceptions/logging/connection shutdown only.
- Existing plaintext path remains: Task 2 keeps identity codec behavior and Task 7 runs all existing tests.
- Plaintext app payload rejection in encrypted mode: Task 6 authenticates to V and sends a plaintext join over the encrypted mode socket.
- Per-client encryption: Task 2 stores `kc_v` in `ClientConnection` and encodes each broadcast inside the per-target loop.
