# Four Client Performance Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Add a repeatable local performance test that runs the encrypted AS/TGS/V game chain with four simulated clients and reports server load, packet rates, log growth, and `GAME_STATE` interval jitter.

**Architecture:** Add a headless C++ load client that authenticates four client identities, joins V, sends deterministic movement/target/shoot traffic, receives encrypted signed `GAME_STATE`, and returns signed encrypted `APP_ACK`. Add a PowerShell wrapper that starts isolated AS/TGS/V instances on random localhost ports, runs the load client, samples process resources and log sizes, and writes a text report.

**Tech Stack:** C++17, existing `cyber_common` auth/game APIs, PowerShell 5+, CMake/Ninja.

---

### Task 1: Add Headless Load Client

**Files:**
- Create: `tests/game_load_client.cpp`
- Modify: `CMakeLists.txt`

- [x] **Step 1: Implement a C++ executable that drives four encrypted clients**

Create `tests/game_load_client.cpp` with:

```cpp
#include "cyber/common/auth_credentials.hpp"
#include "cyber/common/auth_flow.hpp"
#include "cyber/common/config.hpp"
#include "cyber/common/net_packet.hpp"
#include "cyber/common/net_socket.hpp"
#include "cyber/game/game_non_repudiation.hpp"
#include "cyber/game/game_protocol.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Each worker authenticates one ClientN, joins V, sends deterministic inputs,
// receives GAME_STATE, signs ACKs, and records state interval statistics.
```

The executable accepts:

```text
game_load_client CONFIG DURATION_SECONDS INPUT_HZ
```

It prints one machine-readable summary line:

```text
PERF_SUMMARY clients=4 duration_s=10 input_sent=400 state_recv=1200 ack_sent=1200 avg_state_interval_ms=33.1 max_state_interval_ms=41
```

- [x] **Step 2: Add the executable to CMake**

Add:

```cmake
add_executable(game_load_client tests/game_load_client.cpp)
target_link_libraries(game_load_client PRIVATE cyber_common)
```

- [x] **Step 3: Build the executable**

Run:

```powershell
cmake --build build-mingw --target game_load_client
```

Expected: build succeeds and produces `build-mingw/game_load_client.exe`.

### Task 2: Add Local Four-Client Perf Script

**Files:**
- Create: `scripts/run_perf_4clients.ps1`

- [x] **Step 1: Write the wrapper script**

The script should:

```powershell
param(
    [string]$BuildDir = 'build-mingw',
    [int]$DurationSeconds = 30,
    [int]$InputHz = 10,
    [switch]$SkipBuild,
    [switch]$KeepWorkDir
)
```

It should create a temporary config with random localhost AS/TGS/V ports, start:

```powershell
as_server.exe --config <tmp-config> --serve
tgs_server.exe --config <tmp-config> --serve
v_server.exe --config <tmp-config> --game-auth-encrypted
game_load_client.exe <tmp-config> <duration> <input-hz>
```

It should sample process CPU/memory and log file sizes before/after the load run, then write a report to:

```text
<tmp-workdir>/perf_report.txt
```

- [x] **Step 2: Run a short smoke test**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_perf_4clients.ps1 -DurationSeconds 5 -InputHz 5
```

Expected: the output includes `PERF_SUMMARY`, V CPU delta, and log growth rows.

### Task 3: Document Usage

**Files:**
- Modify: `README.md`

- [x] **Step 1: Add a performance test section**

Document:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_perf_4clients.ps1 -DurationSeconds 60 -InputHz 10
```

Explain that this is a headless encrypted game-link load test, not a browser rendering benchmark.

- [x] **Step 2: Run verification**

Run:

```powershell
cmake --build build-mingw --target game_load_client
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_perf_4clients.ps1 -DurationSeconds 5 -InputHz 5
```

Expected: both commands succeed.
