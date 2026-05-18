# Role-First Protocol Reference Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the code so AS, TGS, V, and Client owners can find and modify their role logic quickly, then centralize all wire protocol definitions for report writing and live changes.

**Architecture:** Phase A moves role-owned behavior out of `src/shared/auth/auth_flow.cpp` and role-adjacent headers into `src/roles/<role>/` plus `include/cyber/roles/<role>/`. Phase B creates a protocol definition center under `include/cyber/protocol/` and `src/shared/protocol/`, then updates docs and tests so the protocol reference is the single place to inspect packet fields, encryption boundaries, signatures, and UI visualization fields.

**Tech Stack:** C++17, CMake/Ninja, Winsock TCP, PowerShell scripts, Vite/TypeScript Web UI, existing CTest suite.

---

## File Structure Plan

### Phase A: Role-First Layout

Create or move role-owned C++ files:

```text
include/cyber/roles/as/as_service.hpp
src/roles/as/as_service.cpp

include/cyber/roles/tgs/tgs_service.hpp
src/roles/tgs/tgs_service.cpp

include/cyber/roles/v/v_auth_service.hpp
include/cyber/roles/v/tank_game_server.hpp
src/roles/v/v_auth_service.cpp
src/roles/v/tank_game_server.cpp

include/cyber/roles/client/client_auth_flow.hpp
include/cyber/roles/client/tank_game_client.hpp
src/roles/client/client_auth_flow.cpp
src/roles/client/tank_game_client.cpp
```

Keep shared code only for reusable mechanics:

```text
src/shared/auth/auth_credentials.cpp       password-derived keys and demo key lookup
src/shared/config/config.cpp               config parser
src/shared/crypto/crypto.cpp               DES/hash/RSA/cert primitives
src/shared/net/*.cpp                       TCP packet and socket helpers
src/shared/protocol/*.cpp                  bytes-level protocol codecs and monitor events
src/shared/runtime/role_runtime.cpp        CLI/accept-loop dispatch only
src/shared/game/*.cpp                      MSG_APP envelope and game payload codecs
```

Delete after migration:

```text
include/cyber/common/auth_flow.hpp
src/shared/auth/auth_flow.cpp
include/cyber/game/tank_game_client.hpp
include/cyber/game/tank_game_server.hpp
```

Do not delete these:

```text
include/cyber/common/auth_credentials.hpp
include/cyber/common/logger.hpp
src/shared/logging/logger.cpp
src/shared/logging/log_parser.cpp
```

`logger.cpp` remains the low-level async writer used by `protocol_events`.

### Phase B: Protocol Definition Center

Create the final protocol-facing headers:

```text
include/cyber/protocol/packet.hpp
include/cyber/protocol/kerberos_messages.hpp
include/cyber/protocol/certificate_messages.hpp
include/cyber/protocol/app_envelope.hpp
include/cyber/protocol/protocol_event.hpp
```

Split implementation files:

```text
src/shared/protocol/packet.cpp
src/shared/protocol/kerberos_messages.cpp
src/shared/protocol/certificate_messages.cpp
src/shared/protocol/app_envelope.cpp
src/shared/protocol/protocol_event.cpp
```

Delete after includes are updated:

```text
include/cyber/common/packet.hpp
include/cyber/common/protocol_payloads.hpp
include/cyber/common/protocol_event.hpp
src/shared/protocol/protocol_payloads.cpp
```

Create docs:

```text
docs/protocol_reference.md
docs/live_change_guide.md
```

---

## Task 1: Baseline Verification And Branch Safety

**Files:**
- Read: `CMakeLists.txt`
- Read: `README.md`
- Read: `docs/code_naming_guidelines.md`

- [ ] **Step 1: Confirm the workspace is clean**

Run:

```powershell
git status --short --branch
```

Expected:

```text
## main...origin/main
```

If any modified files appear, inspect them with `git diff -- <path>` and do not overwrite unrelated user changes.

- [ ] **Step 2: Run baseline build**

Run:

```powershell
$env:PATH='E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
cmake --build _generated\build-mingw
```

Expected: build completes with no failed target.

- [ ] **Step 3: Run baseline tests**

Run:

```powershell
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 17
```

- [ ] **Step 4: Run Web UI verification**

Run:

```powershell
npm run verify
```

Working directory: `web-ui`

Expected:

```text
protocolPayloadSelftest: ok
✓ built
```

- [ ] **Step 5: Commit only if baseline docs changed**

No commit is expected in this task. If a generated file changed, do not commit it.

---

## Task 2: Move AS Request Handling Into The AS Role

**Files:**
- Create: `include/cyber/roles/as/as_service.hpp`
- Create: `src/roles/as/as_service.cpp`
- Modify: `src/shared/auth/auth_flow.cpp`
- Modify: `src/shared/runtime/role_runtime.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/roles/as/README.md`
- Test: `tests/auth_encrypted_game_flow_selftest.ps1`
- Test: `tests/auth_payload_selftest.cpp`

- [ ] **Step 1: Write the AS role header**

Create `include/cyber/roles/as/as_service.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"

namespace cyber::roles::as
{
void as_process_connection(SocketHandle socket, const Config& config);
}
```

- [ ] **Step 2: Move AS-only helpers into `as_service.cpp`**

Create `src/roles/as/as_service.cpp` and move these helpers from `src/shared/auth/auth_flow.cpp`:

```cpp
namespace
{
constexpr std::uint32_t kDefaultAdc = 0x7F000001U;
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;

std::uint64_t auth_time_now_ms();
ClientSecret as_find_client_secret(const Config& config, EntityId id);
Packet as_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                 const Bytes& plain, std::uint64_t key);
ProtocolPayloadView protocol_build_encrypted_payload_view(const Bytes& plain,
                                                          const Bytes& encrypted);
void protocol_add_encrypted_field(ProtocolPayloadView& view, std::string name,
                                  const Bytes& encrypted, const Bytes& plain = {});
void packet_require_msg_type(const Packet& packet, MsgType expected);
}
```

Then implement this public function:

```cpp
namespace cyber::roles::as
{
void as_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
        const Packet request = recv_packet_logged(socket);
        packet_require_msg_type(request, MsgType::as_req);

        const AsReq as_req = as_parse_req(request.payload);
        const ClientSecret secret = as_find_client_secret(config, as_req.idc);
        const std::uint64_t kc_tgs = generate_des_key56();
        const std::uint64_t ts2 = auth_time_now_ms();
        const TicketTgsBody ticket_body{
            kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
        const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
        const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
        const Bytes rep_plain = as_build_rep_body(rep_body);
        const Packet response =
            as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                                      rep_plain, secret.kc);

        ProtocolPayloadView view =
            protocol_build_encrypted_payload_view(rep_plain, response.payload);
        protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                                     tgs_ticket_build_body(ticket_body));
        send_packet_logged(socket, response, view);
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
}
```

- [ ] **Step 3: Remove AS logic from shared auth flow**

In `src/shared/auth/auth_flow.cpp`, delete the old `as_process_packet()` function after `as_service.cpp` compiles. Keep any helper still used by Client, TGS, or V until later tasks move them.

- [ ] **Step 4: Dispatch AS connections from runtime**

In `src/shared/runtime/role_runtime.cpp`, include the new header:

```cpp
#include "cyber/roles/as/as_service.hpp"
```

Change the worker dispatch to:

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
else
{
    const Packet request = recv_packet_logged(socket);
    auth_handle_packet(role, socket, request, config);
}
```

This keeps TGS on the old shared path until Task 3.

- [ ] **Step 5: Update CMake**

Add the new source to `add_library(cyber_common ...)` in `CMakeLists.txt`:

```cmake
src/roles/as/as_service.cpp
```

- [ ] **Step 6: Update AS README**

In `src/roles/as/README.md`, change “Shared files usually modified for AS behavior” so AS packet handling points to:

```text
AS packet handling:     src/roles/as/as_service.cpp
AS public interface:    include/cyber/roles/as/as_service.hpp
```

Remove `src/shared/auth/auth_flow.cpp` from AS-owned behavior after this task.

- [ ] **Step 7: Build and test AS migration**

Run:

```powershell
$env:PATH='E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
cmake --build _generated\build-mingw --target as_server auth_payload_selftest
ctest --test-dir _generated\build-mingw -R "auth_payload_selftest|auth_encrypted_game_flow_selftest" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 8: Commit AS migration**

Run:

```powershell
git add CMakeLists.txt include/cyber/roles/as/as_service.hpp src/roles/as/as_service.cpp src/shared/auth/auth_flow.cpp src/shared/runtime/role_runtime.cpp src/roles/as/README.md
git commit -m "refactor: move AS service into role module"
```

---

## Task 3: Move TGS Request Handling Into The TGS Role

**Files:**
- Create: `include/cyber/roles/tgs/tgs_service.hpp`
- Create: `src/roles/tgs/tgs_service.cpp`
- Modify: `src/shared/auth/auth_flow.cpp`
- Modify: `src/shared/runtime/role_runtime.cpp`
- Modify: `CMakeLists.txt`
- Modify: `src/roles/tgs/README.md`
- Test: `tests/auth_encrypted_game_flow_selftest.ps1`

- [ ] **Step 1: Write the TGS role header**

Create `include/cyber/roles/tgs/tgs_service.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"

namespace cyber::roles::tgs
{
void tgs_process_connection(SocketHandle socket, const Config& config);
}
```

- [ ] **Step 2: Move TGS-only logic into `tgs_service.cpp`**

Create `src/roles/tgs/tgs_service.cpp` and move the current TGS logic from `src/shared/auth/auth_flow.cpp`. The public function must have this shape:

```cpp
namespace cyber::roles::tgs
{
void tgs_process_connection(SocketHandle socket, const Config& config)
{
    try
    {
        const Packet request = recv_packet_logged(socket);
        packet_require_msg_type(request, MsgType::tgs_req);

        const TgsReq tgs_req = tgs_parse_req(request.payload);
        const TicketTgsBody ticket =
            tgs_ticket_decrypt(tgs_req.ticket_tgs, config.get_u64("KTGS"));
        const AuthenticatorBody auth =
            authenticator_decrypt(tgs_req.authenticator_tgs, ticket.kc_tgs);
        if (ticket.idc != auth.idc || ticket.idtgs != EntityId::tgs ||
            tgs_req.idv != EntityId::v)
        {
            throw std::runtime_error("TGS identity check failed");
        }

        ProtocolPayloadView request_view;
        protocol_add_encrypted_field(request_view, "ticket_tgs", tgs_req.ticket_tgs,
                                     tgs_ticket_build_body(ticket));
        protocol_add_encrypted_field(request_view, "authenticator_tgs",
                                     tgs_req.authenticator_tgs,
                                     authenticator_build_body(auth));
        write_protocol_event(ProtocolDirection::recv, request, {}, request_view);

        const std::uint64_t kc_v = generate_des_key56();
        const std::uint64_t ts4 = auth_time_now_ms();
        const TicketVBody ticket_v_body{
            kc_v, ticket.idc, ticket.adc, EntityId::v, ts4, kDefaultLifetimeMs};
        const Bytes ticket_v = v_ticket_encrypt(ticket_v_body, config.get_u64("KV"));
        const TgsRepBody rep_body{kc_v, EntityId::v, ts4, ticket_v};
        const Bytes rep_plain = tgs_build_rep_body(rep_body);
        const Packet response =
            tgs_build_encrypted_packet(MsgType::tgs_rep, EntityId::tgs, ticket.idc,
                                       rep_plain, ticket.kc_tgs);

        ProtocolPayloadView view =
            protocol_build_encrypted_payload_view(rep_plain, response.payload);
        protocol_add_encrypted_field(view, "ticket_v", ticket_v,
                                     v_ticket_build_body(ticket_v_body));
        send_packet_logged(socket, response, view);
        close_socket(socket);
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
}
```

- [ ] **Step 3: Dispatch TGS from runtime**

In `src/shared/runtime/role_runtime.cpp`, include:

```cpp
#include "cyber/roles/tgs/tgs_service.hpp"
```

Change the worker dispatch to:

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
else if (role == RoleKind::tgs_server)
{
    cyber::roles::tgs::tgs_process_connection(socket, config);
}
else
{
    const Packet request = recv_packet_logged(socket);
    auth_handle_packet(role, socket, request, config);
}
```

- [ ] **Step 4: Remove TGS logic from shared auth flow**

Delete the old `tgs_process_packet()` function from `src/shared/auth/auth_flow.cpp`. Keep `auth_handle_packet()` until Task 4 removes V-related use or Task 5 deletes the file.

- [ ] **Step 5: Update CMake**

Add:

```cmake
src/roles/tgs/tgs_service.cpp
```

- [ ] **Step 6: Update TGS README**

In `src/roles/tgs/README.md`, make TGS packet handling point to:

```text
TGS packet handling:      src/roles/tgs/tgs_service.cpp
TGS public interface:     include/cyber/roles/tgs/tgs_service.hpp
```

- [ ] **Step 7: Build and test TGS migration**

Run:

```powershell
cmake --build _generated\build-mingw --target tgs_server
ctest --test-dir _generated\build-mingw -R "auth_encrypted_game_flow_selftest" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 8: Commit TGS migration**

Run:

```powershell
git add CMakeLists.txt include/cyber/roles/tgs/tgs_service.hpp src/roles/tgs/tgs_service.cpp src/shared/auth/auth_flow.cpp src/shared/runtime/role_runtime.cpp src/roles/tgs/README.md
git commit -m "refactor: move TGS service into role module"
```

---

## Task 4: Move V Authentication And Certificate Exchange Into The V Role

**Files:**
- Create: `include/cyber/roles/v/v_auth_service.hpp`
- Create: `src/roles/v/v_auth_service.cpp`
- Modify: `include/cyber/common/auth_flow.hpp`
- Modify: `src/shared/auth/auth_flow.cpp`
- Modify: `src/roles/v/tank_game_server.cpp`
- Modify: `include/cyber/game/tank_game_server.hpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/auth_payload_selftest.cpp`

- [ ] **Step 1: Write the V auth header**

Create `include/cyber/roles/v/v_auth_service.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/protocol_payloads.hpp"

#include <map>
#include <mutex>

namespace cyber::roles::v
{
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

AuthRuntime v_auth_make_runtime(const Config& config);
Packet v_auth_process_request(const Packet& request, const Config& config,
                              AuthRuntime& runtime);
Packet cert_process_c2v_request(const Packet& request, AuthRuntime& runtime);
}
```

- [ ] **Step 2: Move V auth implementation**

Create `src/roles/v/v_auth_service.cpp` by moving these from `src/shared/auth/auth_flow.cpp`:

```text
AuthSessionTable methods
auth_make_runtime() body, renamed to v_auth_make_runtime()
v_auth_process_request()
cert_process_c2v_request()
```

Keep function names `v_auth_process_request` and `cert_process_c2v_request` because they already follow the naming convention.

- [ ] **Step 3: Update V server includes and types**

In `include/cyber/game/tank_game_server.hpp`, replace:

```cpp
#include "cyber/common/auth_flow.hpp"
```

with:

```cpp
#include "cyber/roles/v/v_auth_service.hpp"
```

Change the member:

```cpp
AuthRuntime auth_runtime_;
```

to:

```cpp
cyber::roles::v::AuthRuntime auth_runtime_;
```

In `src/roles/v/tank_game_server.cpp`, replace:

```cpp
auth_runtime_(auth_make_runtime(config_))
```

with:

```cpp
auth_runtime_(cyber::roles::v::v_auth_make_runtime(config_))
```

and qualify calls:

```cpp
cyber::roles::v::v_auth_process_request(...)
cyber::roles::v::cert_process_c2v_request(...)
```

- [ ] **Step 4: Update tests**

In `tests/auth_payload_selftest.cpp`, replace:

```cpp
#include "cyber/common/auth_flow.hpp"
```

with:

```cpp
#include "cyber/roles/v/v_auth_service.hpp"
```

Then change:

```cpp
cyber::AuthRuntime runtime = cyber::auth_make_runtime(config);
cyber::v_auth_process_request(v_request, config, runtime);
```

to:

```cpp
cyber::roles::v::AuthRuntime runtime =
    cyber::roles::v::v_auth_make_runtime(config);
cyber::roles::v::v_auth_process_request(v_request, config, runtime);
```

- [ ] **Step 5: Update CMake**

Add:

```cmake
src/roles/v/v_auth_service.cpp
```

- [ ] **Step 6: Build and test V auth migration**

Run:

```powershell
cmake --build _generated\build-mingw --target v_server auth_payload_selftest tank_game_server_flow_selftest
ctest --test-dir _generated\build-mingw -R "auth_payload_selftest|tank_game_server_flow_selftest|auth_encrypted_game_flow_selftest" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 7: Commit V auth migration**

Run:

```powershell
git add CMakeLists.txt include/cyber/roles/v/v_auth_service.hpp src/roles/v/v_auth_service.cpp include/cyber/game/tank_game_server.hpp src/roles/v/tank_game_server.cpp include/cyber/common/auth_flow.hpp src/shared/auth/auth_flow.cpp tests/auth_payload_selftest.cpp
git commit -m "refactor: move V auth into role module"
```

---

## Task 5: Move Client Authentication Flow Into The Client Role

**Files:**
- Create: `include/cyber/roles/client/client_auth_flow.hpp`
- Create: `src/roles/client/client_auth_flow.cpp`
- Modify: `include/cyber/common/auth_flow.hpp`
- Modify: `src/shared/auth/auth_flow.cpp`
- Modify: `include/cyber/game/tank_game_client.hpp`
- Modify: `src/roles/client/tank_game_client.cpp`
- Modify: `tests/encrypted_plaintext_rejection_client.cpp`
- Modify: `tests/game_load_client.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the Client auth header**

Create `include/cyber/roles/client/client_auth_flow.hpp`:

```cpp
#pragma once

#include "cyber/common/config.hpp"
#include "cyber/common/crypto.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/common/protocol_payloads.hpp"

namespace cyber::roles::client
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

struct VAuthenticatedSocket
{
    AuthClientState state;
    SocketHandle socket = 0;
};

VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config,
                                                     EntityId client_id,
                                                     std::uint64_t kc);
}
```

- [ ] **Step 2: Move Client auth implementation**

Create `src/roles/client/client_auth_flow.cpp` by moving these from `src/shared/auth/auth_flow.cpp`:

```text
AuthClientState related helpers
auth_exchange_packet()
client_auth_connect_to_v_socket()
config_build_endpoint()
protocol_build_encrypted_payload_view()
protocol_add_encrypted_field()
packet_build_encrypted()
packet_require_msg_type()
auth_time_now_ms()
```

Keep `client_auth_connect_to_v_socket()` as the public function.

- [ ] **Step 3: Update Client role include**

In `include/cyber/game/tank_game_client.hpp`, replace:

```cpp
#include "cyber/common/auth_flow.hpp"
```

with:

```cpp
#include "cyber/roles/client/client_auth_flow.hpp"
```

Change fields that use auth types:

```cpp
RsaKeyPair client_key_pair_;
RsaPublicKey v_public_key_;
```

These types still come from `crypto.hpp` through the new client auth header.

In `src/roles/client/tank_game_client.cpp`, change:

```cpp
VAuthenticatedSocket auth =
    client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

to:

```cpp
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

- [ ] **Step 4: Update headless clients**

In `tests/encrypted_plaintext_rejection_client.cpp`, replace:

```cpp
#include "cyber/common/auth_flow.hpp"
```

with:

```cpp
#include "cyber/roles/client/client_auth_flow.hpp"
```

Change:

```cpp
cyber::VAuthenticatedSocket auth = cyber::client_auth_connect_to_v_socket(...);
```

to:

```cpp
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(...);
```

Apply the same change in `tests/game_load_client.cpp`.

- [ ] **Step 5: Delete shared auth flow after no users remain**

Run:

```powershell
rg -n "auth_flow|AuthClientState|VAuthenticatedSocket|auth_handle_packet|auth_make_runtime" include src tests
```

Expected: no references to `include/cyber/common/auth_flow.hpp`, `src/shared/auth/auth_flow.cpp`, `auth_handle_packet`, or `auth_make_runtime`.

Then remove:

```text
include/cyber/common/auth_flow.hpp
src/shared/auth/auth_flow.cpp
```

and remove `src/shared/auth/auth_flow.cpp` from `CMakeLists.txt`.

- [ ] **Step 6: Add Client auth file to CMake**

Add:

```cmake
src/roles/client/client_auth_flow.cpp
```

- [ ] **Step 7: Build and test Client auth migration**

Run:

```powershell
cmake --build _generated\build-mingw --target client encrypted_plaintext_rejection_client game_load_client
ctest --test-dir _generated\build-mingw -R "auth_encrypted_game_flow_selftest|tank_game_client_options_selftest" --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 8: Commit Client auth migration**

Run:

```powershell
git add CMakeLists.txt include/cyber/roles/client/client_auth_flow.hpp src/roles/client/client_auth_flow.cpp include/cyber/game/tank_game_client.hpp src/roles/client/tank_game_client.cpp tests/encrypted_plaintext_rejection_client.cpp tests/game_load_client.cpp
git add -u include/cyber/common/auth_flow.hpp src/shared/auth/auth_flow.cpp
git commit -m "refactor: move client auth into role module"
```

---

## Task 6: Move Role Public Headers Out Of `include/cyber/game`

**Files:**
- Move: `include/cyber/game/tank_game_client.hpp` to `include/cyber/roles/client/tank_game_client.hpp`
- Move: `include/cyber/game/tank_game_server.hpp` to `include/cyber/roles/v/tank_game_server.hpp`
- Modify: `src/roles/client/tank_game_client.cpp`
- Modify: `src/roles/v/tank_game_server.cpp`
- Modify: `src/shared/runtime/role_runtime.cpp`
- Modify: `tests/tank_game_server_flow_selftest.cpp`
- Modify: `tests/tank_game_client_options_selftest.cpp`
- Modify: `README.md`
- Modify: `src/roles/client/README.md`
- Modify: `src/roles/v/README.md`

- [ ] **Step 1: Move Client header**

Move:

```text
include/cyber/game/tank_game_client.hpp
```

to:

```text
include/cyber/roles/client/tank_game_client.hpp
```

Update the include guard by keeping `#pragma once`.

- [ ] **Step 2: Move V server header**

Move:

```text
include/cyber/game/tank_game_server.hpp
```

to:

```text
include/cyber/roles/v/tank_game_server.hpp
```

- [ ] **Step 3: Update includes**

Replace:

```cpp
#include "cyber/game/tank_game_client.hpp"
```

with:

```cpp
#include "cyber/roles/client/tank_game_client.hpp"
```

Replace:

```cpp
#include "cyber/game/tank_game_server.hpp"
```

with:

```cpp
#include "cyber/roles/v/tank_game_server.hpp"
```

Run:

```powershell
rg -n "cyber/game/tank_game_(client|server)\\.hpp" include src tests
```

Expected: no matches.

- [ ] **Step 4: Update role READMEs**

In `src/roles/client/README.md`, list:

```text
include/cyber/roles/client/tank_game_client.hpp
include/cyber/roles/client/client_auth_flow.hpp
```

In `src/roles/v/README.md`, list:

```text
include/cyber/roles/v/tank_game_server.hpp
include/cyber/roles/v/v_auth_service.hpp
```

- [ ] **Step 5: Build and test role header moves**

Run:

```powershell
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 17
```

- [ ] **Step 6: Commit role header moves**

Run:

```powershell
git add -A include/cyber/roles include/cyber/game src tests README.md src/roles/client/README.md src/roles/v/README.md
git commit -m "refactor: move role headers into role namespaces"
```

---

## Task 7: Update Role Ownership Documentation And Live Change Map

**Files:**
- Create: `docs/live_change_guide.md`
- Modify: `README.md`
- Modify: `src/roles/README.md`
- Modify: `src/roles/as/README.md`
- Modify: `src/roles/tgs/README.md`
- Modify: `src/roles/v/README.md`
- Modify: `src/roles/client/README.md`
- Modify: `当前系统设计参考稿.md`

- [ ] **Step 1: Create live change guide**

Create `docs/live_change_guide.md`:

```markdown
# Live Change Guide

This guide maps common teacher-requested changes to the exact files to edit.

## AS

| Change | Edit |
| --- | --- |
| Change accepted client passwords or long-term keys | `src/shared/auth/auth_credentials.cpp` |
| Add AS_REQ validation | `src/roles/as/as_service.cpp` |
| Change Ticket_tgs contents | `include/cyber/protocol/kerberos_messages.hpp`, `src/shared/protocol/kerberos_messages.cpp` |
| Change AS listen/connect config | `config/course_config.txt`, `src/shared/runtime/role_runtime.cpp` |

## TGS

| Change | Edit |
| --- | --- |
| Add TGS_REQ validation | `src/roles/tgs/tgs_service.cpp` |
| Change Ticket_v contents | `include/cyber/protocol/kerberos_messages.hpp`, `src/shared/protocol/kerberos_messages.cpp` |
| Change service ID checks | `src/roles/tgs/tgs_service.cpp` |

## V

| Change | Edit |
| --- | --- |
| Change V_AUTH checks | `src/roles/v/v_auth_service.cpp` |
| Change certificate verification | `src/roles/v/v_auth_service.cpp`, `src/shared/crypto/crypto.cpp` |
| Change tank speed, reload, scoring, pickups, bullet range | `src/roles/v/battle_room.cpp`, `src/roles/v/game_world.cpp`, `include/cyber/game/game_types.hpp` |
| Change how Client inputs are applied | `src/roles/v/tank_game_server.cpp` |
| Change APP_ACK behavior | `src/shared/game/game_non_repudiation.cpp` |

## Client

| Change | Edit |
| --- | --- |
| Change login flow | `src/roles/client/tank_game_client.cpp`, `web-ui/src/Game.ts` |
| Change AS/TGS/V auth sequence | `src/roles/client/client_auth_flow.cpp` |
| Change browser command mapping | `src/roles/client/ui_bridge.cpp`, `web-ui/src/Network.ts` |
| Change Protocol panel rendering | `web-ui/src/ProtocolMonitor.ts`, `web-ui/src/protocolPayload.ts` |
```

- [ ] **Step 2: Update top-level README**

In `README.md`, add a “现场修改入口” section that links to:

```text
docs/live_change_guide.md
src/roles/as/README.md
src/roles/tgs/README.md
src/roles/v/README.md
src/roles/client/README.md
docs/protocol_reference.md
```

- [ ] **Step 3: Update current system design reference**

In `当前系统设计参考稿.md`, update role file paths so:

```text
AS behavior lives in src/roles/as/as_service.cpp
TGS behavior lives in src/roles/tgs/tgs_service.cpp
V auth behavior lives in src/roles/v/v_auth_service.cpp
Client auth behavior lives in src/roles/client/client_auth_flow.cpp
```

- [ ] **Step 4: Verify docs contain no old auth flow path**

Run:

```powershell
rg -n "src/shared/auth/auth_flow|include/cyber/common/auth_flow|tank_game_client.hpp|tank_game_server.hpp" README.md docs src/roles 当前系统设计参考稿.md
```

Expected: no references to deleted paths. References to the new role header paths are expected.

- [ ] **Step 5: Commit docs**

Run:

```powershell
git add README.md docs/live_change_guide.md src/roles/README.md src/roles/as/README.md src/roles/tgs/README.md src/roles/v/README.md src/roles/client/README.md 当前系统设计参考稿.md
git commit -m "docs: document role-owned modification entry points"
```

---

## Task 8: Introduce Protocol Header Facades Before Moving Implementations

**Files:**
- Create: `include/cyber/protocol/packet.hpp`
- Create: `include/cyber/protocol/kerberos_messages.hpp`
- Create: `include/cyber/protocol/certificate_messages.hpp`
- Create: `include/cyber/protocol/app_envelope.hpp`
- Create: `include/cyber/protocol/protocol_event.hpp`

- [ ] **Step 1: Create packet facade**

Create `include/cyber/protocol/packet.hpp`:

```cpp
#pragma once

#include "cyber/common/packet.hpp"
```

- [ ] **Step 2: Create Kerberos message facade**

Create `include/cyber/protocol/kerberos_messages.hpp`:

```cpp
#pragma once

#include "cyber/common/protocol_payloads.hpp"
```

- [ ] **Step 3: Create certificate message facade**

Create `include/cyber/protocol/certificate_messages.hpp`:

```cpp
#pragma once

#include "cyber/common/protocol_payloads.hpp"
```

- [ ] **Step 4: Create app envelope facade**

Create `include/cyber/protocol/app_envelope.hpp`:

```cpp
#pragma once

#include "cyber/common/protocol_payloads.hpp"
#include "cyber/game/app_payload_codec.hpp"
#include "cyber/game/game_non_repudiation.hpp"
```

- [ ] **Step 5: Create protocol event facade**

Create `include/cyber/protocol/protocol_event.hpp`:

```cpp
#pragma once

#include "cyber/common/protocol_event.hpp"
```

- [ ] **Step 6: Build facades**

Run:

```powershell
cmake --build _generated\build-mingw --target cyber_common
```

Expected: build succeeds.

- [ ] **Step 7: Commit protocol facades**

Run:

```powershell
git add include/cyber/protocol
git commit -m "refactor: add protocol facade headers"
```

---

## Task 9: Update Includes To Use The Protocol Center

**Files:**
- Modify: all C++ files under `include/`, `src/`, and `tests/` that include `cyber/common/packet.hpp`
- Modify: all C++ files that include `cyber/common/protocol_payloads.hpp`
- Modify: all C++ files that include `cyber/common/protocol_event.hpp`

- [ ] **Step 1: Replace packet includes**

Run:

```powershell
rg -l 'cyber/common/packet.hpp' include src tests | ForEach-Object {
    (Get-Content -LiteralPath $_ -Raw).Replace('cyber/common/packet.hpp','cyber/protocol/packet.hpp') |
        Set-Content -LiteralPath $_ -Encoding ASCII
}
```

Then inspect:

```powershell
rg -n 'cyber/common/packet.hpp' include src tests
```

Expected: no matches.

- [ ] **Step 2: Replace protocol payload includes with the narrowest protocol facade**

Use these replacements:

```text
AS/TGS/V_AUTH ticket and authenticator users -> cyber/protocol/kerberos_messages.hpp
CERT_C2V/CERT_V2C users -> cyber/protocol/certificate_messages.hpp
SignedAppPayload/AppAckPayload users -> cyber/protocol/app_envelope.hpp
```

Run inspection after manual edits:

```powershell
rg -n 'cyber/common/protocol_payloads.hpp' include src tests
```

Expected: no matches.

- [ ] **Step 3: Replace protocol event includes**

Run:

```powershell
rg -l 'cyber/common/protocol_event.hpp' include src tests | ForEach-Object {
    (Get-Content -LiteralPath $_ -Raw).Replace('cyber/common/protocol_event.hpp','cyber/protocol/protocol_event.hpp') |
        Set-Content -LiteralPath $_ -Encoding ASCII
}
```

Then inspect:

```powershell
rg -n 'cyber/common/protocol_event.hpp' include src tests
```

Expected: no matches.

- [ ] **Step 4: Build after include migration**

Run:

```powershell
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 17
```

- [ ] **Step 5: Commit include migration**

Run:

```powershell
git add include src tests
git commit -m "refactor: use protocol-centered includes"
```

---

## Task 10: Split `protocol_payloads` Into Kerberos, Certificate, And App Envelope Codecs

**Files:**
- Create: `src/shared/protocol/kerberos_messages.cpp`
- Create: `src/shared/protocol/certificate_messages.cpp`
- Create: `src/shared/protocol/app_envelope.cpp`
- Modify: `include/cyber/protocol/kerberos_messages.hpp`
- Modify: `include/cyber/protocol/certificate_messages.hpp`
- Modify: `include/cyber/protocol/app_envelope.hpp`
- Modify: `src/shared/protocol/protocol_payloads.cpp`
- Modify: `CMakeLists.txt`
- Delete after migration: `include/cyber/common/protocol_payloads.hpp`
- Delete after migration: `src/shared/protocol/protocol_payloads.cpp`

- [ ] **Step 1: Move Kerberos declarations**

Replace `include/cyber/protocol/kerberos_messages.hpp` with declarations for:

```cpp
struct AsReq;
struct TicketTgsBody;
struct AsRepBody;
struct AuthenticatorBody;
struct TgsReq;
struct TicketVBody;
struct TgsRepBody;
struct VAuthReq;
struct VAuthRepBody;

Bytes as_build_req(const AsReq& value);
AsReq as_parse_req(const Bytes& payload);
Bytes tgs_ticket_build_body(const TicketTgsBody& value);
TicketTgsBody tgs_ticket_parse_body(const Bytes& payload);
Bytes tgs_ticket_encrypt(const TicketTgsBody& value, std::uint64_t ktgs);
TicketTgsBody tgs_ticket_decrypt(const Bytes& cipher, std::uint64_t ktgs);
Bytes as_build_rep_body(const AsRepBody& value);
AsRepBody as_parse_rep_body(const Bytes& payload);
Bytes authenticator_build_body(const AuthenticatorBody& value);
AuthenticatorBody authenticator_parse_body(const Bytes& payload);
Bytes authenticator_encrypt(const AuthenticatorBody& value, std::uint64_t key56);
AuthenticatorBody authenticator_decrypt(const Bytes& cipher, std::uint64_t key56);
Bytes tgs_build_req(const TgsReq& value);
TgsReq tgs_parse_req(const Bytes& payload);
Bytes v_ticket_build_body(const TicketVBody& value);
TicketVBody v_ticket_parse_body(const Bytes& payload);
Bytes v_ticket_encrypt(const TicketVBody& value, std::uint64_t kv);
TicketVBody v_ticket_decrypt(const Bytes& cipher, std::uint64_t kv);
Bytes tgs_build_rep_body(const TgsRepBody& value);
TgsRepBody tgs_parse_rep_body(const Bytes& payload);
Bytes v_auth_build_req(const VAuthReq& value);
VAuthReq v_auth_parse_req(const Bytes& payload);
Bytes v_auth_build_rep_body(const VAuthRepBody& value);
VAuthRepBody v_auth_parse_rep_body(const Bytes& payload);
```

Copy the exact struct field definitions from `include/cyber/common/protocol_payloads.hpp`; do not rename fields in this task.

- [ ] **Step 2: Move certificate declarations**

Replace `include/cyber/protocol/certificate_messages.hpp` with:

```cpp
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
}
```

- [ ] **Step 3: Move app envelope declarations**

Replace `include/cyber/protocol/app_envelope.hpp` with:

```cpp
#pragma once

#include "cyber/common/crypto.hpp"
#include "cyber/protocol/packet.hpp"

namespace cyber
{
struct AppAckPayload
{
    MsgType acked_msg_type = MsgType::app;
    AppCode acked_app_code = AppCode::app_ack;
    EntityId acked_src = EntityId::unknown;
    EntityId acked_dst = EntityId::unknown;
    std::uint32_t acked_payload_len = 0;
    std::uint64_t acked_payload_hash = 0;
};

struct SignedAppPayload
{
    AppCode app_code = AppCode::app_ack;
    Bytes app_payload;
    Bytes signature;
};

Bytes ack_build_payload(const AppAckPayload& value);
AppAckPayload ack_parse_payload(const Bytes& payload);
Bytes app_build_signed_payload(AppCode code, const Bytes& app_payload,
                               const RsaPrivateKey& private_key);
SignedAppPayload app_parse_signed_payload(const Bytes& decrypted_payload);
bool app_verify_signed_payload(const SignedAppPayload& signed_payload,
                               const RsaPublicKey& public_key);
Bytes app_build_signed_logical_bytes(AppCode code, const Bytes& app_payload);
}
```

- [ ] **Step 4: Split implementation**

Move function bodies from `src/shared/protocol/protocol_payloads.cpp`:

```text
AS/TGS/V_AUTH/ticket/authenticator functions -> src/shared/protocol/kerberos_messages.cpp
CERT_C2V/CERT_V2C functions -> src/shared/protocol/certificate_messages.cpp
APP_ACK/SignedAppPayload functions -> src/shared/protocol/app_envelope.cpp
```

Each new `.cpp` must include its corresponding header.

- [ ] **Step 5: Remove old protocol payload file**

After all functions are moved, delete:

```text
include/cyber/common/protocol_payloads.hpp
src/shared/protocol/protocol_payloads.cpp
```

Update `CMakeLists.txt` by removing:

```cmake
src/shared/protocol/protocol_payloads.cpp
```

and adding:

```cmake
src/shared/protocol/kerberos_messages.cpp
src/shared/protocol/certificate_messages.cpp
src/shared/protocol/app_envelope.cpp
```

- [ ] **Step 6: Build and test protocol split**

Run:

```powershell
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 17
```

- [ ] **Step 7: Commit protocol payload split**

Run:

```powershell
git add -A include/cyber/protocol include/cyber/common src/shared/protocol CMakeLists.txt include src tests
git commit -m "refactor: split protocol payload codecs by domain"
```

---

## Task 11: Move Packet And Protocol Event Headers Into Protocol Center

**Files:**
- Move: `include/cyber/common/packet.hpp` to `include/cyber/protocol/packet.hpp`
- Move: `include/cyber/common/protocol_event.hpp` to `include/cyber/protocol/protocol_event.hpp`
- Modify: all includes under `include/`, `src/`, and `tests/`
- Delete old facade headers if they still include common paths

- [ ] **Step 1: Replace packet facade with real header**

Copy the full contents of `include/cyber/common/packet.hpp` into `include/cyber/protocol/packet.hpp`.

Then delete:

```text
include/cyber/common/packet.hpp
```

- [ ] **Step 2: Replace protocol event facade with real header**

Copy the full contents of `include/cyber/common/protocol_event.hpp` into `include/cyber/protocol/protocol_event.hpp`.

Then delete:

```text
include/cyber/common/protocol_event.hpp
```

- [ ] **Step 3: Verify no old protocol includes remain**

Run:

```powershell
rg -n "cyber/common/(packet|protocol_event|protocol_payloads)\\.hpp" include src tests
```

Expected: no matches.

- [ ] **Step 4: Build and test final protocol include move**

Run:

```powershell
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
npm run verify
```

Working directory for `npm run verify`: `web-ui`.

Expected:

```text
100% tests passed, 0 tests failed out of 17
protocolPayloadSelftest: ok
✓ built
```

- [ ] **Step 5: Commit protocol header move**

Run:

```powershell
git add -A include/cyber/protocol include/cyber/common include src tests
git commit -m "refactor: make protocol headers the public packet API"
```

---

## Task 12: Write The Protocol Reference Document

**Files:**
- Create: `docs/protocol_reference.md`
- Modify: `README.md`
- Modify: `当前系统设计参考稿.md`

- [ ] **Step 1: Create protocol reference**

Create `docs/protocol_reference.md` with these sections:

```markdown
# Protocol Reference

## Fixed Packet Header

| Offset | Size | Field | Type | Description |
| --- | --- | --- | --- | --- |
| 0 | 1 | `msg_type` | `u8` | `MSG_AS_REQ`, `MSG_AS_REP`, `MSG_TGS_REQ`, `MSG_TGS_REP`, `MSG_V_AUTH_REQ`, `MSG_V_AUTH_REP`, `MSG_CERT_C2V`, `MSG_CERT_V2C`, `MSG_APP`, or `MSG_ERROR` |
| 1 | 1 | `src` | `u8` | Logical sender ID |
| 2 | 1 | `dst` | `u8` | Logical receiver ID |
| 3 | 4 | `payload_len` | `u32_be` | Payload byte length |
| 7 | 4 | `reserved` | `u32_be` | Always 0 in final runtime |

## Kerberos Messages

| Field | Type | Encryption |
| --- | --- | --- |
| `MSG_AS_REQ.idc` | `u8` | plain |
| `MSG_AS_REQ.idtgs` | `u8` | plain |
| `MSG_AS_REQ.ts1` | `u64` | plain |
| `MSG_AS_REP.kc_tgs` | `u64` | outer payload encrypted with `Kc` |
| `MSG_AS_REP.ticket_tgs` | bytes | nested field encrypted with `KTGS` |
| `MSG_TGS_REQ.ticket_tgs` | bytes | encrypted with `KTGS` |
| `MSG_TGS_REQ.authenticator_tgs` | bytes | encrypted with `Kc_tgs` |
| `MSG_TGS_REP.kc_v` | `u64` | outer payload encrypted with `Kc_tgs` |
| `MSG_TGS_REP.ticket_v` | bytes | nested field encrypted with `KV` |
| `MSG_V_AUTH_REQ.ticket_v` | bytes | encrypted with `KV` |
| `MSG_V_AUTH_REQ.authenticator_v` | bytes | encrypted with `Kc_v` |
| `MSG_V_AUTH_REP.ts5_plus_1` | `u64` | outer payload encrypted with `Kc_v` |

## Certificate Messages

| Message | Field | Type | Encryption |
| --- | --- | --- | --- |
| `MSG_CERT_C2V` | `client_id` | `u8` | outer payload encrypted with `Kc_v` |
| `MSG_CERT_C2V` | `cert` | bytes | outer payload encrypted with `Kc_v` |
| `MSG_CERT_V2C` | `v_id` | `u8` | outer payload encrypted with `Kc_v` |
| `MSG_CERT_V2C` | `cert` | bytes | outer payload encrypted with `Kc_v` |

## Application Envelope

Document:

```text
Packet(MSG_APP).payload = DES_Kc_v(SignedAppPayload)
SignedAppPayload = app_code + app_payload + signature
signature = RSA_private(hash(app_code + app_payload))
```

## Game AppCode Table

| AppCode | Direction | Payload |
| --- | --- | --- |
| `GAME_JOIN_REQ` | Client -> V | `GameMessage(join, JoinMessage)` |
| `GAME_MOVE` | Client -> V | `GameMessage(move, MoveMessage)` |
| `GAME_TARGET` | Client -> V | `GameMessage(target, TargetMessage)` |
| `GAME_SHOOT` | Client -> V | `GameMessage(shoot, empty)` |
| `GAME_STATE` | V -> Client | `GameMessage(state, BattleStateSnapshot)` |
| `APP_ACK` | Bidirectional | `AppAckPayload` |

## Protocol Events For UI

| Field | Meaning |
| --- | --- |
| `packet_hex` | complete packet bytes: 11-byte header plus wire payload |
| `payload_hex` | payload exactly as transmitted on the TCP connection |
| `payload_plain_hex` | decrypted payload when the process can parse it |
| `payload_encrypted_hex` | encrypted payload when a plaintext view is also available |
| `field*_plain_hex` | decrypted nested Kerberos field, such as ticket or authenticator |
| `field*_encrypted_hex` | encrypted nested Kerberos field, such as ticket or authenticator |
```

- [ ] **Step 2: Link the reference**

In `README.md`, add:

```markdown
完整报文字段表见 [docs/protocol_reference.md](docs/protocol_reference.md)。
```

In `当前系统设计参考稿.md`, replace duplicate protocol field tables with a short reference if a table would otherwise diverge from `docs/protocol_reference.md`.

- [ ] **Step 3: Check for placeholders**

Run:

Open `docs/protocol_reference.md` and confirm every section contains concrete field names, concrete types, and concrete encryption/signature boundaries. The file must not contain placeholder prose or instructions to fill content later.

- [ ] **Step 4: Commit protocol reference**

Run:

```powershell
git add docs/protocol_reference.md README.md 当前系统设计参考稿.md
git commit -m "docs: add centralized protocol reference"
```

---

## Task 13: Final End-To-End Verification

**Files:**
- Read: all changed files
- Modify only if verification reveals a real issue

- [ ] **Step 1: Run full C++ verification**

Run:

```powershell
$env:PATH='E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:PATH
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected:

```text
100% tests passed, 0 tests failed out of 17
```

- [ ] **Step 2: Run Web UI verification**

Run:

```powershell
npm run verify
```

Working directory: `web-ui`

Expected:

```text
protocolPayloadSelftest: ok
✓ built
```

- [ ] **Step 3: Run local smoke test**

Run:

```powershell
.\scripts\run_local.ps1
```

Expected:

```text
Started local encrypted tank backend:
Client bridge: ws://127.0.0.1:7001
Protocol monitor: ws://127.0.0.1:7010
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

Login as `Client1` with password `123456`, join the game, and confirm Protocol shows AS, TGS, V, Client packet events.

- [ ] **Step 4: Stop local processes**

Run:

```powershell
.\scripts\stop_local.ps1
```

Expected: AS/TGS/V/Client/Monitor processes are stopped.

- [ ] **Step 5: Check old paths are gone**

Run:

```powershell
rg -n "auth_flow|cyber/common/(packet|protocol_payloads|protocol_event)\\.hpp|cyber/game/tank_game_(client|server)\\.hpp|src/shared/auth/auth_flow" include src tests README.md docs 当前系统设计参考稿.md
```

Expected: no matches.

- [ ] **Step 6: Check formatting and status**

Run:

```powershell
git diff --check
git status --short --branch
```

Expected: `git diff --check` exits 0. `git status` shows only intentional committed or staged files; after final commit it should show:

```text
## main...origin/main
```

- [ ] **Step 7: Final commit if verification fixes were needed**

If any verification fixes were made:

```powershell
git add -A
git commit -m "chore: finalize role protocol refactor"
```

- [ ] **Step 8: Push**

Run:

```powershell
git push origin main
```

Expected:

```text
main -> main
```

---

## Self-Review

- Spec coverage: Phase A is covered by Tasks 2-7. Phase B is covered by Tasks 8-12. Final verification is Task 13.
- Placeholder scan: This plan contains concrete tasks, file paths, commands, and code shapes. The protocol reference task includes starter tables and requires checking that the final docs contain no placeholder prose.
- Type consistency: Role namespaces are fixed as `cyber::roles::as`, `cyber::roles::tgs`, `cyber::roles::v`, and `cyber::roles::client`. Protocol headers are fixed under `include/cyber/protocol/`.
- Risk control: Each role split and protocol split has its own build/test command and commit. The old `auth_flow` and old protocol headers are deleted only after `rg` verifies no references remain.
