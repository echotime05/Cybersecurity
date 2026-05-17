# Kerberos-Gated Tank UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a real-password Kerberos login gate and a simple join screen before the existing plaintext tank battle Web UI.

**Architecture:** Keep the browser connected only to the local C++ client bridge. The local C++ client derives `Kc` from the UI password, runs AS/TGS/V authentication, keeps the authenticated V socket open, and sends plaintext tank game messages only after the user clicks `Join Game`. The V tank server gains an auth-gated mode that accepts `V_AUTH_REQ` before plaintext `MsgType::app` game traffic.

**Tech Stack:** C++17, Winsock TCP, existing packet/auth/game protocol helpers, native WebSocket bridge, Vite + TypeScript + Three.js.

---

## File Structure

Create:

- `include/cyber/common/auth_credentials.hpp`: password-to-key derivation and fixed client display names.
- `src/common/auth_credentials.cpp`: implementation of deterministic course-demo key derivation.
- `tests/auth_credentials_selftest.cpp`: deterministic key and client-name tests.
- `include/cyber/game/auth_plain_game_client.hpp`: C++ client for Web UI driven Kerberos login and plaintext gameplay.
- `src/game/auth_plain_game_client.cpp`: authenticated client state machine, UI command handling, V receive loop.
- `tests/auth_plain_game_flow_selftest.ps1`: end-to-end AS/TGS/auth-gated V/client/UI WebSocket smoke test.

Modify:

- `CMakeLists.txt`: add new sources and tests.
- `config/course_config.txt` and `config/lan/*.txt`: update `C1_KC` through `C4_KC` to password-derived values.
- `include/cyber/common/auth_flow.hpp` and `src/common/auth_flow.cpp`: expose reusable V-auth helpers that do not close the socket.
- `include/cyber/ui/ui_bridge.hpp` and `src/ui/ui_bridge.cpp`: parse `login` and join gate commands, broadcast generic JSON status messages.
- `include/cyber/game/plain_game_server.hpp` and `src/game/plain_game_server.cpp`: add optional V-auth gate before plaintext game packets.
- `src/common/role_runtime.cpp`: add `--game-auth-plain` for client and V.
- `web-ui/src/Network.ts`: add login/join status types and commands.
- `web-ui/src/Game.ts`: add UI state gate, disable controls before join, wire login and join events.
- `web-ui/index.html`: add login overlay and join overlay.
- `README.md`: document the authenticated plaintext run mode.

---

### Task 1: Password-Derived Client Keys

**Files:**
- Create: `include/cyber/common/auth_credentials.hpp`
- Create: `src/common/auth_credentials.cpp`
- Create: `tests/auth_credentials_selftest.cpp`
- Modify: `CMakeLists.txt`
- Modify: `config/course_config.txt`
- Modify: `config/lan/host1_client1.txt`
- Modify: `config/lan/host2_as_client2.txt`
- Modify: `config/lan/host3_tgs_client3.txt`
- Modify: `config/lan/host4_v_client4.txt`
- Modify: `config/course_config.lan.example.txt`
- Modify: `tests/auth_flow_selftest.ps1`
- Modify: `tests/connect_probe_selftest.ps1`

- [ ] **Step 1: Write the failing credentials test**

Create `tests/auth_credentials_selftest.cpp`:

```cpp
#include "cyber/common/auth_credentials.hpp"
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
        require(cyber::derive_client_key(cyber::EntityId::client1, "123456") ==
                    0x0059EF3DB7CB8C8DULL,
                "Client1 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client2, "admin123") ==
                    0x006A73A4EBE9C564ULL,
                "Client2 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client3, "hehe12345") ==
                    0x0057EF9D5F45AB7BULL,
                "Client3 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client4, "&wxh@147") ==
                    0x00EC3EB766D59086ULL,
                "Client4 derived key mismatch");
        require(cyber::derive_client_key(cyber::EntityId::client1, "wrong") !=
                    0x0059EF3DB7CB8C8DULL,
                "wrong password should produce a different key");
        require(cyber::default_client_name(cyber::EntityId::client4) == "Client4",
                "default client name mismatch");

        std::cout << "auth_credentials_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "auth_credentials_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
```

Add `src/common/auth_credentials.cpp` as the first source in the existing `add_library(cyber_common)` source list:

```cmake
add_library(cyber_common
    src/common/auth_credentials.cpp
    src/common/auth_flow.cpp
    src/common/config.cpp
)
```

Keep the remaining existing `cyber_common` sources after `src/common/config.cpp`.

Add the test target after `crypto_selftest`:

```cmake
add_executable(auth_credentials_selftest tests/auth_credentials_selftest.cpp)
target_link_libraries(auth_credentials_selftest PRIVATE cyber_common)
```

Register the test after `crypto_selftest`:

```cmake
add_test(NAME auth_credentials_selftest COMMAND auth_credentials_selftest)
```

- [ ] **Step 2: Run the focused build and verify it fails**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target auth_credentials_selftest
```

Expected: build fails because `cyber/common/auth_credentials.hpp` and `derive_client_key` do not exist yet.

- [ ] **Step 3: Add the credentials implementation**

Create `include/cyber/common/auth_credentials.hpp`:

```cpp
#pragma once

#include "cyber/common/types.hpp"

#include <cstdint>
#include <string>

namespace cyber
{
std::uint64_t derive_client_key(EntityId client_id, const std::string& password);
std::string default_client_name(EntityId client_id);
} // namespace cyber
```

Create `src/common/auth_credentials.cpp`:

```cpp
#include "cyber/common/auth_credentials.hpp"

#include "cyber/common/crypto.hpp"

#include <stdexcept>

namespace cyber
{
std::uint64_t derive_client_key(EntityId client_id, const std::string& password)
{
    if (!is_client(client_id))
    {
        throw std::runtime_error("derive_client_key requires a client id");
    }
    const std::string material = "client-kc-v1:" +
                                 std::to_string(static_cast<int>(client_id)) + ":" + password;
    const Bytes bytes(material.begin(), material.end());
    std::uint64_t key = hash64(bytes) & 0x00FFFFFFFFFFFFFFULL;
    if (key == 0)
    {
        key = 0x0001010101010101ULL;
    }
    return key;
}

std::string default_client_name(EntityId client_id)
{
    if (!is_client(client_id))
    {
        return "Unknown";
    }
    return std::string(to_string(client_id));
}
} // namespace cyber
```

Update each config and test-generated config file with these values:

```text
C1_KC=0x59ef3db7cb8c8d
C2_KC=0x6a73a4ebe9c564
C3_KC=0x57ef9d5f45ab7b
C4_KC=0xec3eb766d59086
```

- [ ] **Step 4: Run the credentials test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target auth_credentials_selftest
& .\build-mingw\auth_credentials_selftest.exe
```

Expected:

```text
auth_credentials_selftest: ok
```

- [ ] **Step 5: Commit**

```powershell
git add include/cyber/common/auth_credentials.hpp src/common/auth_credentials.cpp tests/auth_credentials_selftest.cpp CMakeLists.txt config/course_config.txt config/lan/host1_client1.txt config/lan/host2_as_client2.txt config/lan/host3_tgs_client3.txt config/lan/host4_v_client4.txt config/course_config.lan.example.txt tests/auth_flow_selftest.ps1 tests/connect_probe_selftest.ps1
git commit -m "feat: derive kerberos client keys from passwords"
```

---

### Task 2: Reusable V Authentication Helpers

**Files:**
- Modify: `include/cyber/common/auth_flow.hpp`
- Modify: `src/common/auth_flow.cpp`
- Modify: `tests/auth_payload_selftest.cpp`

- [ ] **Step 1: Write the failing helper test**

Append this block inside `tests/auth_payload_selftest.cpp` before its success print:

```cpp
        cyber::AuthRuntime runtime = cyber::make_auth_runtime(config);
        cyber::Packet v_request =
            cyber::make_packet(cyber::MsgType::v_auth_req, cyber::EntityId::client1,
                               cyber::EntityId::v, cyber::build_v_auth_req(v_auth_req));
        cyber::Logger logger(std::filesystem::temp_directory_path() / "auth_payload_v_auth.log");
        cyber::Packet v_response =
            cyber::process_v_auth_request(v_request, config, runtime, logger, "AuthPayloadTest");
        require(v_response.msg_type == cyber::MsgType::v_auth_rep, "V_AUTH response type mismatch");
        require(v_response.src == cyber::EntityId::v, "V_AUTH response source mismatch");
        require(v_response.dst == cyber::EntityId::client1, "V_AUTH response destination mismatch");
        const cyber::VAuthRepBody parsed_v_response =
            cyber::parse_v_auth_rep_body(cyber::des_decrypt_payload(v_response.payload, kc_v));
        require(parsed_v_response.ts5_plus_1 == auth_v.ts + 1U,
                "V_AUTH response timestamp mismatch");
        require(runtime.v_sessions.get(cyber::EntityId::client1).v_auth_done,
                "V_AUTH session was not stored");
```

Add these includes at the top of `tests/auth_payload_selftest.cpp`:

```cpp
#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"

#include <filesystem>
#include <fstream>
```

Construct the test config in memory by writing a temporary config file before the block:

```cpp
        const std::filesystem::path config_path =
            std::filesystem::temp_directory_path() / "auth_payload_config.txt";
        {
            std::ofstream out(config_path);
            out << "C1_ID=0x01\nC2_ID=0x02\nC3_ID=0x03\nC4_ID=0x04\n"
                << "AS_ID=0x11\nTGS_ID=0x12\nV_ID=0x13\nLOCAL_CLIENT_ID=0x01\n"
                << "AS_BIND_IP=127.0.0.1\nAS_IP=127.0.0.1\nAS_HOST=127.0.0.1\nAS_PORT=1\n"
                << "TGS_BIND_IP=127.0.0.1\nTGS_IP=127.0.0.1\nTGS_HOST=127.0.0.1\nTGS_PORT=2\n"
                << "V_BIND_IP=127.0.0.1\nV_IP=127.0.0.1\nV_HOST=127.0.0.1\nV_PORT=3\n"
                << "C1_PASSWORD=123456\nC1_KC=0x59ef3db7cb8c8d\n"
                << "C2_PASSWORD=admin123\nC2_KC=0x6a73a4ebe9c564\n"
                << "C3_PASSWORD=hehe12345\nC3_KC=0x57ef9d5f45ab7b\n"
                << "C4_PASSWORD=&wxh@147\nC4_KC=0xec3eb766d59086\n"
                << "KTGS=0x1c24deeecc136e\nKV=0x3398481d2a89f6\n"
                << "PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3\n"
                << "PK_CA_E=0x10001\n"
                << "SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69\n";
        }
        const cyber::Config config = cyber::Config::load(config_path);
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target auth_payload_selftest
```

Expected: build fails because `process_v_auth_request` is not declared.

- [ ] **Step 3: Expose socket-preserving auth helpers**

Add to `include/cyber/common/auth_flow.hpp`:

```cpp
struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

Packet process_v_auth_request(const Packet& request, const Config& config, AuthRuntime& runtime,
                              Logger& logger, const std::string& thread_name);

VAuthenticatedSocket authenticate_client_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc, Logger& logger,
                                                     const std::string& thread_name);
```

In `src/common/auth_flow.cpp`, replace the private `handle_v_auth_packet` response construction with this public helper:

```cpp
Packet process_v_auth_request(const Packet& request, const Config& config, AuthRuntime& runtime,
                              Logger& logger, const std::string& thread_name)
{
    ensure_msg(request, MsgType::v_auth_req);
    const VAuthReq v_req = parse_v_auth_req(request.payload);
    const TicketVBody ticket = decrypt_ticket_v(v_req.ticket_v, config.get_u64("KV"));
    const AuthenticatorBody auth = decrypt_authenticator(v_req.authenticator_v, ticket.kc_v);
    if (ticket.idc != auth.idc || ticket.idv != EntityId::v || request.src != ticket.idc)
    {
        throw std::runtime_error("V identity check failed");
    }
    runtime.v_sessions.put_v_auth(ticket.idc, ticket.adc, ticket.kc_v);
    logger.write("V", thread_name, "AUTH_STATE",
                 "V_AUTH_REP_SENT client=" + entity_log_name(ticket.idc));
    return encrypted_packet(MsgType::v_auth_rep, EntityId::v, ticket.idc,
                            build_v_auth_rep_body({auth.ts + 1U}), ticket.kc_v);
}
```

Then make `handle_v_auth_packet` call the helper and send the returned packet:

```cpp
void handle_v_auth_packet(SocketHandle socket, const Packet& request, const Config& config,
                          AuthRuntime& runtime, Logger& logger, const std::string& thread_name)
{
    const Packet response = process_v_auth_request(request, config, runtime, logger, thread_name);
    send_packet_logged(socket, response, logger, "V", thread_name);
}
```

Add `authenticate_client_to_v_socket` near `run_client_auth_test` by reusing the same AS/TGS/V code path and leaving the V socket open:

```cpp
VAuthenticatedSocket authenticate_client_to_v_socket(const Config& config, EntityId client_id,
                                                     std::uint64_t kc, Logger& logger,
                                                     const std::string& thread_name)
{
    AuthClientState state;
    state.client_id = client_id;
    state.adc = kDefaultAdc;
    state.kc = kc;
    state.client_key_pair = demo_rsa_key_pair_for(client_id);

    const std::uint64_t ts1 = now_ms();
    const Packet as_req =
        make_packet(MsgType::as_req, state.client_id, EntityId::as,
                    build_as_req({state.client_id, EntityId::tgs, ts1}));
    const Packet as_rep = request_response(connect_endpoint(config, "AS_IP", "AS_PORT"), as_req,
                                           logger, thread_name);
    ensure_msg(as_rep, MsgType::as_rep);
    const AsRepBody as_body = parse_as_rep_body(des_decrypt_payload(as_rep.payload, state.kc));
    state.kc_tgs = as_body.kc_tgs;
    state.ticket_tgs = as_body.ticket_tgs;
    logger.write("Client", thread_name, "AUTH_STATE", "AS_OK");

    const AuthenticatorBody auth_tgs{state.client_id, state.adc, now_ms()};
    const TgsReq tgs_req_body{EntityId::v, state.ticket_tgs,
                              encrypt_authenticator(auth_tgs, state.kc_tgs)};
    const Packet tgs_req =
        make_packet(MsgType::tgs_req, state.client_id, EntityId::tgs,
                    build_tgs_req(tgs_req_body));
    const Packet tgs_rep = request_response(connect_endpoint(config, "TGS_IP", "TGS_PORT"),
                                            tgs_req, logger, thread_name);
    ensure_msg(tgs_rep, MsgType::tgs_rep);
    const TgsRepBody tgs_body =
        parse_tgs_rep_body(des_decrypt_payload(tgs_rep.payload, state.kc_tgs));
    state.kc_v = tgs_body.kc_v;
    state.ticket_v = tgs_body.ticket_v;
    logger.write("Client", thread_name, "AUTH_STATE", "TGS_OK");

    const TcpEndpoint v = connect_endpoint(config, "V_IP", "V_PORT");
    logger.write("Client", thread_name, "CONNECT", "connect to V " + endpoint_text(v));
    SocketHandle socket = connect_tcp(v);
    try
    {
        const std::uint64_t ts5 = now_ms();
        const AuthenticatorBody auth_v{state.client_id, state.adc, ts5};
        const VAuthReq v_req_body{state.ticket_v, encrypt_authenticator(auth_v, state.kc_v)};
        const Packet v_req = make_packet(MsgType::v_auth_req, state.client_id, EntityId::v,
                                         build_v_auth_req(v_req_body));
        send_packet_logged(socket, v_req, logger, "Client", thread_name);
        const Packet v_rep = recv_packet_logged(socket, logger, "Client", thread_name);
        ensure_msg(v_rep, MsgType::v_auth_rep);
        const VAuthRepBody v_body =
            parse_v_auth_rep_body(des_decrypt_payload(v_rep.payload, state.kc_v));
        if (v_body.ts5_plus_1 != ts5 + 1U)
        {
            throw std::runtime_error("V_AUTH TS5+1 check failed");
        }
        logger.write("Client", thread_name, "AUTH_STATE", "V_AUTH_OK");
        return {state, socket};
    }
    catch (const std::exception&)
    {
        close_socket(socket);
        throw;
    }
}
```

- [ ] **Step 4: Run auth payload and existing auth flow tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target auth_payload_selftest
& .\build-mingw\auth_payload_selftest.exe
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw -R auth_flow_selftest --output-on-failure
```

Expected:

```text
auth_payload_selftest: ok
100% tests passed
```

- [ ] **Step 5: Commit**

```powershell
git add include/cyber/common/auth_flow.hpp src/common/auth_flow.cpp tests/auth_payload_selftest.cpp
git commit -m "feat: expose reusable v authentication helpers"
```

---

### Task 3: UI Bridge Login and Status Messages

**Files:**
- Modify: `include/cyber/ui/ui_bridge.hpp`
- Modify: `src/ui/ui_bridge.cpp`
- Modify: `tests/ui_bridge_selftest.cpp`

- [ ] **Step 1: Write failing UI bridge tests**

Extend `tests/ui_bridge_selftest.cpp`:

```cpp
        const cyber::ui::UiCommand login =
            bridge.parse_json_command("{\"type\":\"login\",\"clientId\":4,\"password\":\"&wxh@147\"}");
        require(login.kind == cyber::ui::UiCommandKind::login, "login command kind mismatch");
        require(login.client_id == cyber::EntityId::client4, "login client mismatch");
        require(login.password == "&wxh@147", "login password mismatch");

        const cyber::ui::UiCommand join_gate =
            bridge.parse_json_command("{\"type\":\"join\"}");
        require(join_gate.kind == cyber::ui::UiCommandKind::join_game,
                "join gate command kind mismatch");

        const std::string authenticated =
            cyber::ui::login_state_json("authenticated", cyber::EntityId::client4,
                                        "172.27.39.248:9003", "");
        require(authenticated.find("\"type\":\"loginState\"") != std::string::npos,
                "loginState type missing");
        require(authenticated.find("\"status\":\"authenticated\"") != std::string::npos,
                "loginState status missing");
        require(authenticated.find("\"clientId\":4") != std::string::npos,
                "loginState client id missing");
        require(authenticated.find("\"vServer\":\"172.27.39.248:9003\"") != std::string::npos,
                "loginState V server missing");

        const std::string failed =
            cyber::ui::login_state_json("failed", cyber::EntityId::unknown, "",
                                        "Invalid client id or password");
        require(failed.find("\"message\":\"Invalid client id or password\"") != std::string::npos,
                "loginState failure message missing");
        require(cyber::ui::join_state_json("joined").find("\"status\":\"joined\"") !=
                    std::string::npos,
                "joinState JSON mismatch");
```

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target ui_bridge_selftest
```

Expected: build fails because `UiCommandKind`, `login_state_json`, and `join_state_json` do not exist.

- [ ] **Step 3: Extend bridge command and JSON types**

Update `include/cyber/ui/ui_bridge.hpp`:

```cpp
enum class UiCommandKind
{
    game,
    login,
    join_game,
    error
};

struct UiCommand
{
    UiCommandKind kind = UiCommandKind::error;
    cyber::game::GameMsgType type = cyber::game::GameMsgType::error;
    cyber::Bytes payload;
    cyber::EntityId client_id = cyber::EntityId::unknown;
    std::string password;
};

std::string login_state_json(const std::string& status, cyber::EntityId client_id,
                             const std::string& v_server, const std::string& message);
std::string join_state_json(const std::string& status);
```

Add a public broadcast method:

```cpp
    void broadcast_text(const std::string& json);
    void set_self(cyber::EntityId self);
```

Update `src/ui/ui_bridge.cpp`:

```cpp
std::string json_escape(const std::string& value)
{
    std::string out;
    for (char ch : value)
    {
        if (ch == '"' || ch == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

std::string login_state_json(const std::string& status, cyber::EntityId client_id,
                             const std::string& v_server, const std::string& message)
{
    std::string out = "{\"type\":\"loginState\",\"status\":\"" + json_escape(status) + "\"";
    if (cyber::is_client(client_id))
    {
        out += ",\"clientId\":" + std::to_string(static_cast<int>(client_id));
    }
    if (!v_server.empty())
    {
        out += ",\"vServer\":\"" + json_escape(v_server) + "\"";
    }
    if (!message.empty())
    {
        out += ",\"message\":\"" + json_escape(message) + "\"";
    }
    out += "}";
    return out;
}

std::string join_state_json(const std::string& status)
{
    return "{\"type\":\"joinState\",\"status\":\"" + json_escape(status) + "\"}";
}
```

Change `parse_json_command` branches:

```cpp
    if (type == "login")
    {
        const int id = extract_int(text, "clientId", -1);
        cyber::EntityId client_id = cyber::EntityId::unknown;
        if (id >= 1 && id <= 4)
        {
            client_id = static_cast<cyber::EntityId>(id);
        }
        return {UiCommandKind::login, cyber::game::GameMsgType::error, {}, client_id,
                extract_string(text, "password")};
    }
    if (type == "join")
    {
        return {UiCommandKind::join_game, cyber::game::GameMsgType::error, {}, self_, ""};
    }
```

For game commands, return `UiCommandKind::game`:

```cpp
        return {UiCommandKind::game, cyber::game::GameMsgType::move,
                cyber::game::build_move({static_cast<std::int8_t>(x),
                                         static_cast<std::int8_t>(y)}),
                self_, ""};
```

Update `handle_client` to dispatch every non-error command:

```cpp
            if (command.kind != UiCommandKind::error && handler_)
            {
                handler_(command);
            }
```

Move the body of `broadcast_state` socket sending into `broadcast_text`:

```cpp
void UiBridge::set_self(cyber::EntityId self)
{
    self_ = self;
}

void UiBridge::broadcast_state(const cyber::game::BattleStateSnapshot& snapshot)
{
    broadcast_text(cyber::game::to_json(snapshot, self_));
}
```

- [ ] **Step 4: Run UI bridge tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target ui_bridge_selftest
& .\build-mingw\ui_bridge_selftest.exe
```

Expected:

```text
ui_bridge_selftest: ok
```

- [ ] **Step 5: Commit**

```powershell
git add include/cyber/ui/ui_bridge.hpp src/ui/ui_bridge.cpp tests/ui_bridge_selftest.cpp
git commit -m "feat: add ui bridge login commands"
```

---

### Task 4: Auth-Gated Plain Game Server

**Files:**
- Modify: `include/cyber/game/plain_game_server.hpp`
- Modify: `src/game/plain_game_server.cpp`
- Modify: `tests/plain_game_flow_selftest.cpp`

- [ ] **Step 1: Write failing server behavior test**

In `tests/plain_game_flow_selftest.cpp`, add a second scenario after the existing plaintext scenario. It should start an auth-gated server and verify a raw game join before V auth is rejected:

```cpp
        cyber::game::PlainGameServer auth_server({"127.0.0.1", 0}, config, true);
        const std::uint16_t auth_port = auth_server.start_for_test();
        std::thread auth_thread([&]() { auth_server.run_until_stopped(); });

        cyber::SocketHandle unauthenticated = cyber::connect_tcp({"127.0.0.1", auth_port});
        const auto bad_join = cyber::game::build_game_message(
            {cyber::game::GameMsgType::join,
             cyber::game::build_join({cyber::EntityId::client1, "Client1"})});
        cyber::send_packet_logged(unauthenticated,
                                  cyber::make_packet(cyber::MsgType::app,
                                                     cyber::EntityId::client1, cyber::EntityId::v,
                                                     bad_join),
                                  client_logger, "Client", "AuthGateTest");
        bool closed_or_failed = false;
        try
        {
            (void)cyber::recv_packet_logged(unauthenticated, client_logger, "Client",
                                            "AuthGateTest");
        }
        catch (const std::exception&)
        {
            closed_or_failed = true;
        }
        require(closed_or_failed, "auth-gated server accepted app traffic before V_AUTH");
        cyber::close_socket(unauthenticated);
        auth_server.stop();
        auth_thread.join();
```

Add the config construction used in Task 2 to this test, or factor a local helper that writes the same temporary config and calls `Config::load`.

- [ ] **Step 2: Run focused test and verify it fails**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target plain_game_flow_selftest
```

Expected: build fails because `PlainGameServer({"127.0.0.1", 0}, config, true)` does not exist.

- [ ] **Step 3: Add auth gate constructor and first-packet V_AUTH check**

Update `include/cyber/game/plain_game_server.hpp` by adding the two includes near the existing includes:

```cpp
#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
```

Add the auth-gated constructor in the public section:

```cpp
    explicit PlainGameServer(TcpEndpoint endpoint);
    PlainGameServer(TcpEndpoint endpoint, Config config, bool require_auth);
```

Add the auth helper and fields in the private section:

```cpp
    bool authenticate_socket(SocketHandle socket, const std::string& peer, EntityId& client_id);
    Config config_;
    bool require_auth_ = false;
    AuthRuntime auth_runtime_;
```

Update `src/game/plain_game_server.cpp` constructors:

```cpp
PlainGameServer::PlainGameServer(TcpEndpoint endpoint)
    : endpoint_(std::move(endpoint)),
      logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}

PlainGameServer::PlainGameServer(TcpEndpoint endpoint, Config config, bool require_auth)
    : endpoint_(std::move(endpoint)),
      config_(std::move(config)),
      require_auth_(require_auth),
      auth_runtime_(require_auth_ ? cyber::make_auth_runtime(config_) : AuthRuntime{}),
      logger_(std::filesystem::path("logs") / "v_plain_game.log")
{
}
```

Add `authenticate_socket`:

```cpp
bool PlainGameServer::authenticate_socket(SocketHandle socket, const std::string& peer,
                                          EntityId& client_id)
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
    client_id = auth_packet.src;
    return true;
}
```

At the start of `client_loop`:

```cpp
    EntityId authenticated_client = EntityId::unknown;
    if (require_auth_ && !authenticate_socket(socket, peer, authenticated_client))
    {
        close_socket(socket);
        return;
    }
```

Before `handle_packet(socket, packet)`:

```cpp
            if (require_auth_ && packet.src != authenticated_client)
            {
                throw std::runtime_error("authenticated client id mismatch");
            }
            handle_packet(socket, packet);
```

In the join branch of `handle_packet`, reject mismatched join payloads:

```cpp
        if (join.client_id != packet.src)
        {
            throw std::runtime_error("join client id must match packet source");
        }
```

- [ ] **Step 4: Run server flow tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target plain_game_flow_selftest
& .\build-mingw\plain_game_flow_selftest.exe
```

Expected:

```text
plain_game_flow_selftest: ok
```

- [ ] **Step 5: Commit**

```powershell
git add include/cyber/game/plain_game_server.hpp src/game/plain_game_server.cpp tests/plain_game_flow_selftest.cpp
git commit -m "feat: gate plaintext game server with v authentication"
```

---

### Task 5: Authenticated Plain Game Client

**Files:**
- Create: `include/cyber/game/auth_plain_game_client.hpp`
- Create: `src/game/auth_plain_game_client.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add source file entries and verify the class is missing**

Add `src/game/auth_plain_game_client.cpp` to `cyber_common` in `CMakeLists.txt`.

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
```

Expected: build fails because the new source file and header do not exist.

- [ ] **Step 2: Create the authenticated client header**

Create `include/cyber/game/auth_plain_game_client.hpp`:

```cpp
#pragma once

#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/logger.hpp"
#include "cyber/game/game_protocol.hpp"
#include "cyber/ui/ui_bridge.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace cyber::game
{
class AuthPlainGameClient
{
public:
    AuthPlainGameClient(Config config, std::uint16_t ui_port);
    ~AuthPlainGameClient();

    void run();

private:
    enum class State
    {
        waiting_for_login,
        authenticating,
        authenticated,
        joined
    };

    void handle_ui_command(const cyber::ui::UiCommand& command);
    void handle_login(const cyber::ui::UiCommand& command);
    void handle_join();
    void handle_game_command(const cyber::ui::UiCommand& command);
    void receive_loop();
    void send_game_message(GameMsgType type, const Bytes& payload);
    void close_v_socket();
    std::string v_server_text() const;

    Config config_;
    std::uint16_t ui_port_ = 0;
    EntityId self_ = EntityId::unknown;
    SocketHandle v_socket_ = 0;
    State state_ = State::waiting_for_login;
    std::mutex state_mutex_;
    std::mutex send_mutex_;
    std::atomic<bool> stopping_{false};
    Logger logger_;
    std::unique_ptr<cyber::ui::UiBridge> bridge_;
    std::thread rx_thread_;
};
} // namespace cyber::game
```

- [ ] **Step 3: Create the authenticated client implementation**

Create `src/game/auth_plain_game_client.cpp`:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"

#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/net_packet.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace cyber::game
{
AuthPlainGameClient::AuthPlainGameClient(Config config, std::uint16_t ui_port)
    : config_(std::move(config)),
      ui_port_(ui_port),
      logger_(std::filesystem::path("logs") / "client_auth_plain_game.log")
{
}

AuthPlainGameClient::~AuthPlainGameClient()
{
    stopping_ = true;
    close_v_socket();
    if (bridge_)
    {
        bridge_->stop();
    }
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

void AuthPlainGameClient::run()
{
    SocketRuntime runtime;
    bridge_ = std::make_unique<cyber::ui::UiBridge>(
        ui_port_, EntityId::unknown,
        [this](const cyber::ui::UiCommand& command) { handle_ui_command(command); });

    std::cout << "Client auth plaintext game UI ws://127.0.0.1:" << ui_port_ << '\n';
    bridge_->broadcast_text(cyber::ui::login_state_json("idle", EntityId::unknown, "", ""));
    bridge_->run();
    stopping_ = true;
    close_v_socket();
    if (rx_thread_.joinable())
    {
        rx_thread_.join();
    }
}

void AuthPlainGameClient::handle_ui_command(const cyber::ui::UiCommand& command)
{
    if (command.kind == cyber::ui::UiCommandKind::login)
    {
        handle_login(command);
    }
    else if (command.kind == cyber::ui::UiCommandKind::join_game)
    {
        handle_join();
    }
    else if (command.kind == cyber::ui::UiCommandKind::game)
    {
        handle_game_command(command);
    }
}

void AuthPlainGameClient::handle_login(const cyber::ui::UiCommand& command)
{
    if (!is_client(command.client_id) || command.password.empty())
    {
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == State::authenticating)
        {
            return;
        }
        state_ = State::authenticating;
    }
    bridge_->broadcast_text(cyber::ui::login_state_json("authenticating", command.client_id, "", ""));

    try
    {
        close_v_socket();
        if (rx_thread_.joinable())
        {
            rx_thread_.join();
        }
        const std::uint64_t kc = derive_client_key(command.client_id, command.password);
        VAuthenticatedSocket auth =
            authenticate_client_to_v_socket(config_, command.client_id, kc, logger_,
                                            "AuthPlainGameClient");
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            self_ = command.client_id;
            v_socket_ = auth.socket;
            state_ = State::authenticated;
        }
        bridge_->set_self(self_);
        rx_thread_ = std::thread([this]() { receive_loop(); });
        bridge_->broadcast_text(cyber::ui::login_state_json("authenticated", self_,
                                                            v_server_text(), ""));
    }
    catch (const std::exception& ex)
    {
        logger_.write("Client", "AuthPlainGameClient", "ERROR", ex.what());
        close_v_socket();
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = State::waiting_for_login;
        bridge_->broadcast_text(cyber::ui::login_state_json(
            "failed", EntityId::unknown, "", "Invalid client id or password"));
    }
}

void AuthPlainGameClient::handle_join()
{
    EntityId client = EntityId::unknown;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::authenticated)
        {
            return;
        }
        client = self_;
        state_ = State::joined;
    }
    send_game_message(GameMsgType::join, build_join({client, default_client_name(client)}));
    bridge_->broadcast_text(cyber::ui::join_state_json("joined"));
}

void AuthPlainGameClient::handle_game_command(const cyber::ui::UiCommand& command)
{
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::joined)
        {
            return;
        }
    }
    send_game_message(command.type, command.payload);
}

void AuthPlainGameClient::send_game_message(GameMsgType type, const Bytes& payload)
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ == 0)
    {
        return;
    }
    const Bytes message = build_game_message({type, payload});
    const Packet packet = make_packet(MsgType::app, self_, EntityId::v, message);
    send_packet_logged(v_socket_, packet, logger_, "Client", "AuthPlainGameTx");
}

void AuthPlainGameClient::receive_loop()
{
    try
    {
        while (!stopping_)
        {
            const Packet packet = recv_packet_logged(v_socket_, logger_, "Client",
                                                     "AuthPlainGameRx");
            if (packet.msg_type != MsgType::app)
            {
                continue;
            }
            const GameMessage message = parse_game_message(packet.payload);
            if (message.type == GameMsgType::state && bridge_)
            {
                bridge_->broadcast_state(parse_state(message.payload));
            }
        }
    }
    catch (const std::exception& ex)
    {
        if (!stopping_)
        {
            logger_.write("Client", "AuthPlainGameRx", "ERROR", ex.what());
            bridge_->broadcast_text(cyber::ui::login_state_json(
                "failed", EntityId::unknown, "", "V connection closed"));
        }
    }
}

void AuthPlainGameClient::close_v_socket()
{
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (v_socket_ != 0)
    {
        close_socket(v_socket_);
        v_socket_ = 0;
    }
}

std::string AuthPlainGameClient::v_server_text() const
{
    return config_.get_string("V_IP") + ":" + std::to_string(config_.get_u16("V_PORT"));
}
} // namespace cyber::game
```

- [ ] **Step 4: Build the new client**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client
```

Expected: `client.exe` builds.

- [ ] **Step 5: Commit**

```powershell
git add include/cyber/game/auth_plain_game_client.hpp src/game/auth_plain_game_client.cpp CMakeLists.txt
git commit -m "feat: add authenticated plaintext game client"
```

---

### Task 6: Command Surface

**Files:**
- Modify: `src/common/role_runtime.cpp`
- Modify: `README.md`

- [ ] **Step 1: Add failing CLI expectation**

Run before implementation:

```powershell
.\build-mingw\client.exe --help
```

Expected: output does not contain `--game-auth-plain`.

- [ ] **Step 2: Add `--game-auth-plain`**

Update includes in `src/common/role_runtime.cpp`:

```cpp
#include "cyber/game/auth_plain_game_client.hpp"
```

Update usage:

```cpp
" [--game-plain] [--game-auth-plain] [--ui-port PORT]\n";
```

Add flag storage:

```cpp
    bool game_auth_plain = false;
```

Add argument parsing:

```cpp
        else if (arg == "--game-auth-plain")
        {
            game_auth_plain = true;
        }
```

Add runtime branch before `if (game_plain)`:

```cpp
        if (game_auth_plain)
        {
            if (role == RoleKind::v_server)
            {
                SocketRuntime runtime;
                cyber::game::PlainGameServer server(bind_endpoint(config, spec), config, true);
                server.run();
                return 0;
            }
            if (role == RoleKind::client)
            {
                if (ui_port == 0)
                {
                    throw std::runtime_error("client --game-auth-plain requires --ui-port");
                }
                cyber::game::AuthPlainGameClient client(config, ui_port);
                client.run();
                return 0;
            }
            throw std::runtime_error("--game-auth-plain is only supported by v_server and client");
        }
```

Add README run commands:

```markdown
Authenticated plaintext game mode:

```powershell
.\build-mingw\as_server.exe --serve
.\build-mingw\tgs_server.exe --serve
.\build-mingw\v_server.exe --game-auth-plain
.\build-mingw\client.exe --game-auth-plain --ui-port 7001
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001
```

The browser password is used only by the local C++ client to derive `Kc`. Tank game messages after join remain plaintext `MsgType::app` payloads.
```

- [ ] **Step 3: Build and verify help text**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw --target client v_server
.\build-mingw\client.exe --help
```

Expected: help text contains `--game-auth-plain`.

- [ ] **Step 4: Commit**

```powershell
git add src/common/role_runtime.cpp README.md
git commit -m "feat: add authenticated plaintext game mode"
```

---

### Task 7: Frontend Login and Join UI

**Files:**
- Modify: `web-ui/src/Network.ts`
- Modify: `web-ui/src/Game.ts`
- Modify: `web-ui/index.html`

- [ ] **Step 1: Add TypeScript network API**

Update `web-ui/src/Network.ts` with status types:

```ts
export type LoginStatus = "idle" | "authenticating" | "authenticated" | "failed";
export type JoinStatus = "joined";

export type LoginState = {
  type: "loginState";
  status: LoginStatus;
  clientId?: number;
  vServer?: string;
  message?: string;
};

export type JoinState = {
  type: "joinState";
  status: JoinStatus;
};
```

Add callbacks and message handling:

```ts
  onLoginState?: (state: LoginState) => void;
  onJoinState?: (state: JoinState) => void;

  async connect(): Promise<Network> {
    this.socket = new WebSocket(this.serverUrl);
    this.socket.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.type === "state") {
        this.self = message.self;
        this.state = message.state;
        this.onState?.(message.state);
      } else if (message.type === "loginState") {
        this.onLoginState?.(message);
      } else if (message.type === "joinState") {
        this.onJoinState?.(message);
      }
    };
    await new Promise<void>((resolve, reject) => {
      this.socket.onopen = () => resolve();
      this.socket.onerror = () => reject(new Error("WebSocket connection failed"));
    });
    return this;
  }
```

Add login and join commands:

```ts
  sendLogin(clientId: number, password: string) {
    this.send({ type: "login", clientId, password });
  }

  sendJoin() {
    this.send({ type: "join" });
  }
```

- [ ] **Step 2: Add overlay markup and CSS**

Add to `web-ui/index.html` inside `#hud`, before health bars:

```html
    <div id="auth-overlay">
      <form id="login-panel" class="auth-panel">
        <div class="panel-title">Tank Client Login</div>
        <label>
          Client ID
          <select id="login-client">
            <option value="1">Client1</option>
            <option value="2">Client2</option>
            <option value="3">Client3</option>
            <option value="4">Client4</option>
          </select>
        </label>
        <label>
          Password
          <input id="login-password" type="password" autocomplete="current-password" />
        </label>
        <button id="login-submit" type="submit">Login</button>
        <div id="login-message" class="auth-message"></div>
      </form>

      <div id="join-panel" class="auth-panel hidden">
        <div class="panel-title">Join Game</div>
        <div class="join-row"><span>Client ID</span><strong id="join-client-id">Client</strong></div>
        <div class="join-row"><span>V Server</span><strong id="join-v-server">0.0.0.0:0</strong></div>
        <button id="join-submit" type="button">Join Game</button>
      </div>
    </div>
```

Add CSS:

```css
    #auth-overlay {
      position: absolute; inset: 0; display: flex; align-items: center; justify-content: center;
      pointer-events: auto; z-index: 20; background: rgba(4, 8, 13, 0.58);
    }
    #auth-overlay.hidden { display: none; }
    .auth-panel {
      width: min(360px, calc(100vw - 32px));
      background: rgba(10, 16, 24, 0.92);
      border: 1px solid rgba(255, 255, 255, 0.16);
      border-radius: 6px;
      padding: 18px;
      color: #fff;
      font-family: Arial, sans-serif;
      box-shadow: 0 12px 40px rgba(0, 0, 0, 0.42);
    }
    .auth-panel.hidden { display: none; }
    .panel-title { font-size: 18px; font-weight: 700; margin-bottom: 16px; }
    .auth-panel label { display: block; font-size: 12px; color: #aeb8c5; margin-bottom: 12px; }
    .auth-panel select, .auth-panel input {
      width: 100%; height: 34px; margin-top: 6px; padding: 0 10px;
      color: #fff; background: #111b28; border: 1px solid #2e4056; border-radius: 4px;
    }
    .auth-panel button {
      width: 100%; height: 36px; margin-top: 6px; cursor: pointer;
      color: #0b1119; background: #72d6ff; border: 0; border-radius: 4px; font-weight: 700;
    }
    .auth-panel button:disabled { opacity: 0.55; cursor: default; }
    .auth-message { min-height: 18px; margin-top: 10px; font-size: 12px; color: #ffb4a8; }
    .join-row {
      display: flex; align-items: center; justify-content: space-between;
      padding: 8px 0; border-bottom: 1px solid rgba(255, 255, 255, 0.08);
      font-size: 13px;
    }
    .join-row span { color: #aeb8c5; }
```

- [ ] **Step 3: Wire UI state in `Game.ts`**

Add fields:

```ts
  joined = false;
  authOverlay!: HTMLElement;
  loginPanel!: HTMLElement;
  joinPanel!: HTMLElement;
  loginClient!: HTMLSelectElement;
  loginPassword!: HTMLInputElement;
  loginSubmit!: HTMLButtonElement;
  loginMessage!: HTMLElement;
  joinClientId!: HTMLElement;
  joinVServer!: HTMLElement;
  joinSubmit!: HTMLButtonElement;
```

Initialize DOM fields in the constructor:

```ts
    this.authOverlay = document.getElementById("auth-overlay")!;
    this.loginPanel = document.getElementById("login-panel")!;
    this.joinPanel = document.getElementById("join-panel")!;
    this.loginClient = document.getElementById("login-client") as HTMLSelectElement;
    this.loginPassword = document.getElementById("login-password") as HTMLInputElement;
    this.loginSubmit = document.getElementById("login-submit") as HTMLButtonElement;
    this.loginMessage = document.getElementById("login-message")!;
    this.joinClientId = document.getElementById("join-client-id")!;
    this.joinVServer = document.getElementById("join-v-server")!;
    this.joinSubmit = document.getElementById("join-submit") as HTMLButtonElement;
```

Add UI event listeners:

```ts
    this.loginPanel.addEventListener("submit", (event) => {
      event.preventDefault();
      const clientId = Number(this.loginClient.value);
      this.loginSubmit.disabled = true;
      this.loginMessage.textContent = "Authenticating";
      this.network.sendLogin(clientId, this.loginPassword.value);
    });
    this.joinSubmit.addEventListener("click", () => {
      this.joinSubmit.disabled = true;
      this.network.sendJoin();
    });
```

Register network status callbacks in `start()`:

```ts
    this.network.onLoginState = (state) => {
      if (state.status === "authenticating") {
        this.loginSubmit.disabled = true;
        this.loginMessage.textContent = "Authenticating";
      } else if (state.status === "authenticated") {
        this.loginPanel.classList.add("hidden");
        this.joinPanel.classList.remove("hidden");
        this.joinClientId.textContent = `Client${state.clientId ?? ""}`;
        this.joinVServer.textContent = state.vServer ?? "";
        this.loginPassword.value = "";
      } else if (state.status === "failed") {
        this.loginSubmit.disabled = false;
        this.loginMessage.textContent = state.message ?? "Login failed";
      }
    };
    this.network.onJoinState = (state) => {
      if (state.status === "joined") {
        this.joined = true;
        this.authOverlay.classList.add("hidden");
      }
    };
```

Only hide the connection status after the WebSocket opens:

```ts
      await this.network.connect();
      this.connectStatus.style.display = "none";
```

Gate controls before join:

```ts
    window.addEventListener("mousedown", (e) => {
      if (e.button === 0 && this.joined) {
        this.mouseDown = true;
        this.network.sendShoot(true);
      }
    });
```

And in `sendInput()`:

```ts
    if (!this.connected || !this.joined) return;
```

- [ ] **Step 4: Build the Web UI**

Run:

```powershell
cd web-ui
npm run build
cd ..
```

Expected: Vite build completes successfully.

- [ ] **Step 5: Commit**

```powershell
git add web-ui/src/Network.ts web-ui/src/Game.ts web-ui/index.html
git commit -m "feat: add kerberos login gate to web ui"
```

---

### Task 8: End-to-End Authenticated Game Flow Test

**Files:**
- Create: `tests/auth_plain_game_flow_selftest.ps1`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add the failing PowerShell test**

Create `tests/auth_plain_game_flow_selftest.ps1`:

```powershell
param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

function Start-RoleProcess {
    param(
        [string]$ExeName,
        [string[]]$Arguments,
        [string]$Name,
        [string]$WorkDir
    )

    Start-Process `
        -FilePath (Join-Path $BuildDir $ExeName) `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $WorkDir "$Name.out") `
        -RedirectStandardError (Join-Path $WorkDir "$Name.err") `
        -PassThru
}

function Receive-WebSocketJson {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [int]$TimeoutMs
    )
    $buffer = New-Object byte[] 8192
    $segment = [ArraySegment[byte]]::new($buffer)
    $task = $Socket.ReceiveAsync($segment, [Threading.CancellationToken]::None)
    if (-not $task.Wait($TimeoutMs)) {
        throw "Timed out waiting for websocket message"
    }
    $count = $task.Result.Count
    return [Text.Encoding]::UTF8.GetString($buffer, 0, $count) | ConvertFrom-Json
}

function Send-WebSocketText {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [string]$Text
    )
    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $segment = [ArraySegment[byte]]::new($bytes)
    $Socket.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text,
        $true, [Threading.CancellationToken]::None).Wait(5000)
}

function Assert-NoWebSocketMessage {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [int]$TimeoutMs
    )
    $buffer = New-Object byte[] 1024
    $segment = [ArraySegment[byte]]::new($buffer)
    $task = $Socket.ReceiveAsync($segment, [Threading.CancellationToken]::None)
    if ($task.Wait($TimeoutMs)) {
        throw "Received websocket message before join"
    }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_auth_plain_game_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null

$basePort = Get-Random -Minimum 21000 -Maximum 43000
$asPort = $basePort
$tgsPort = $basePort + 1
$vPort = $basePort + 2
$uiPort = $basePort + 3
$config = Join-Path $tmp 'course_config.txt'

@"
C1_ID=0x01
C2_ID=0x02
C3_ID=0x03
C4_ID=0x04
AS_ID=0x11
TGS_ID=0x12
V_ID=0x13
LOCAL_CLIENT_ID=0x01
AS_BIND_IP=127.0.0.1
AS_IP=127.0.0.1
AS_HOST=127.0.0.1
AS_PORT=$asPort
TGS_BIND_IP=127.0.0.1
TGS_IP=127.0.0.1
TGS_HOST=127.0.0.1
TGS_PORT=$tgsPort
V_BIND_IP=127.0.0.1
V_IP=127.0.0.1
V_HOST=127.0.0.1
V_PORT=$vPort
C1_PASSWORD=123456
C1_KC=0x59ef3db7cb8c8d
C2_PASSWORD=admin123
C2_KC=0x6a73a4ebe9c564
C3_PASSWORD=hehe12345
C3_KC=0x57ef9d5f45ab7b
C4_PASSWORD=&wxh@147
C4_KC=0xec3eb766d59086
KTGS=0x1c24deeecc136e
KV=0x3398481d2a89f6
PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3
PK_CA_E=0x10001
SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69
"@ | Set-Content -LiteralPath $config -Encoding ASCII

$processes = @()
$socket = $null
try {
    $processes += Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '2') 'as_server' $tmp
    $processes += Start-RoleProcess 'tgs_server.exe' @('--config', $config, '--serve', '--max-connections', '1') 'tgs_server' $tmp
    $processes += Start-RoleProcess 'v_server.exe' @('--config', $config, '--game-auth-plain') 'v_server' $tmp
    $processes += Start-RoleProcess 'client.exe' @('--config', $config, '--game-auth-plain', '--ui-port', "$uiPort") 'client' $tmp

    Start-Sleep -Milliseconds 900

    $socket = [System.Net.WebSockets.ClientWebSocket]::new()
    $socket.ConnectAsync([Uri]"ws://127.0.0.1:$uiPort", [Threading.CancellationToken]::None).Wait(5000)

    Send-WebSocketText $socket '{"type":"login","clientId":1,"password":"wrong"}'
    $failed = Receive-WebSocketJson $socket 5000
    if ($failed.type -ne 'loginState' -or $failed.status -ne 'failed') {
        throw "Expected failed loginState for wrong password"
    }

    Send-WebSocketText $socket '{"type":"login","clientId":1,"password":"123456"}'
    $authenticated = $null
    for ($i = 0; $i -lt 8 -and $null -eq $authenticated; $i++) {
        $message = Receive-WebSocketJson $socket 5000
        if ($message.type -eq 'loginState' -and $message.status -eq 'authenticated') {
            $authenticated = $message
        }
    }
    if ($null -eq $authenticated -or $authenticated.clientId -ne 1) {
        throw "Expected authenticated loginState for Client1"
    }

    Send-WebSocketText $socket '{"type":"move","x":1,"y":0}'
    Assert-NoWebSocketMessage $socket 300
    Send-WebSocketText $socket '{"type":"join"}'
    $joined = Receive-WebSocketJson $socket 5000
    if ($joined.type -ne 'joinState' -or $joined.status -ne 'joined') {
        throw "Expected joined state"
    }

    $sawState = $false
    for ($i = 0; $i -lt 10 -and -not $sawState; $i++) {
        $message = Receive-WebSocketJson $socket 5000
        if ($message.type -eq 'state' -and $message.state.tanks.Count -gt 0) {
            $sawState = $true
        }
    }
    if (-not $sawState) {
        throw "Expected game state after join"
    }

    Write-Host 'auth_plain_game_flow_selftest: ok'
}
finally {
    if ($socket) {
        $socket.Dispose()
    }
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
```

Add to `CMakeLists.txt`:

```cmake
add_test(
    NAME auth_plain_game_flow_selftest
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_plain_game_flow_selftest.ps1
            -BuildDir $<TARGET_FILE_DIR:client>
)
```

- [ ] **Step 2: Run the new test and verify it fails before final fixes**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw -R auth_plain_game_flow_selftest --output-on-failure
```

Expected before all previous tasks are complete: test fails because the new command path or bridge behavior is incomplete. Expected after previous tasks: test passes and prints `auth_plain_game_flow_selftest: ok`.

- [ ] **Step 3: Commit**

```powershell
git add tests/auth_plain_game_flow_selftest.ps1 CMakeLists.txt
git commit -m "test: cover authenticated plaintext game flow"
```

---

### Task 9: Full Verification

**Files:**
- No new files.

- [ ] **Step 1: Reconfigure and build**

Run:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -S . -B build-mingw -G Ninja -DCMAKE_MAKE_PROGRAM='E:\Qt\Tools\Ninja\ninja.exe' -DCMAKE_CXX_COMPILER='E:\Qt\Tools\mingw1120_64\bin\g++.exe'
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build-mingw
```

Expected: build completes without errors.

- [ ] **Step 2: Run all CTest tests**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --test-dir build-mingw --output-on-failure
```

Expected: every registered test passes, including:

```text
auth_credentials_selftest
auth_payload_selftest
plain_game_flow_selftest
auth_flow_selftest
auth_plain_game_flow_selftest
```

- [ ] **Step 3: Build Web UI**

Run:

```powershell
cd web-ui
npm run build
cd ..
```

Expected: Vite build completes successfully.

- [ ] **Step 4: Manual run check**

Stop any old game processes:

```powershell
Get-Process as_server,tgs_server,v_server,client,node -ErrorAction SilentlyContinue | Stop-Process -Force
```

Start authenticated plaintext mode:

```powershell
$env:PATH='E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;E:\zhuomian\tools\node-v24.15.0-win-x64;' + $env:PATH
Start-Process -FilePath .\build-mingw\as_server.exe -ArgumentList @('--serve') -WorkingDirectory $PWD -WindowStyle Hidden
Start-Process -FilePath .\build-mingw\tgs_server.exe -ArgumentList @('--serve') -WorkingDirectory $PWD -WindowStyle Hidden
Start-Process -FilePath .\build-mingw\v_server.exe -ArgumentList @('--game-auth-plain') -WorkingDirectory $PWD -WindowStyle Hidden
Start-Process -FilePath .\build-mingw\client.exe -ArgumentList @('--game-auth-plain','--ui-port','7001') -WorkingDirectory $PWD -WindowStyle Hidden
Start-Process -FilePath npm -ArgumentList @('run','dev','--','--host','127.0.0.1') -WorkingDirectory (Join-Path $PWD 'web-ui') -WindowStyle Hidden
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001
```

Expected manual behavior:

- Page starts on login overlay.
- Wrong password shows a coarse login failure.
- Correct password for selected client reaches Join Game.
- Join Game shows the existing tank UI.
- Direction, aiming, and shooting do nothing before join and work after join.

- [ ] **Step 5: Final commit if verification required doc or README fixes**

If verification caused README or small test fixes, commit them:

```powershell
git add README.md tests CMakeLists.txt web-ui src include config
git commit -m "chore: verify kerberos gated tank ui"
```

If no files changed after Task 8, skip this commit.
