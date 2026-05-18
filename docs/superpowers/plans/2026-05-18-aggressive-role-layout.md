# Aggressive Role Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Reorganize the final tank security project so four presenters can quickly find AS, TGS, V, and Client code while generated files are isolated from editable source.

**Architecture:** Keep public headers stable under `include/cyber` to avoid churn, move C++ implementation files into `src/roles/*` for role-owned behavior and `src/shared/*` for shared protocol, crypto, network, logging, runtime, and game helpers. Move generated build/runtime outputs under `_generated/` and update scripts, README, and CMake paths to match.

**Tech Stack:** C++17, CMake/Ninja, PowerShell scripts, Node/Vite web UI, TypeScript protocol monitor UI.

---

### Task 1: Isolate Generated Outputs

**Files:**
- Modify: `.gitignore`
- Modify: `scripts/run_local.ps1`
- Modify: `scripts/run_perf_4clients.ps1`
- Modify: `web-ui/package.json`
- Modify: `web-ui/vite.config.ts`
- Move generated directories under `_generated/`
- Move unrelated untracked `image_processing_platform/` outside `code/`

- [x] **Step 1: Update ignore rules**

Add `_generated/` as the canonical output root while keeping compatibility ignores for old local build directories and `web-ui/node_modules/`.

- [x] **Step 2: Move generated directories safely**

Verify resolved absolute paths stay under `E:\zhuomian\cybersecurity\code` for `_generated` moves and under `E:\zhuomian\cybersecurity\_quarantine` for unrelated quarantine moves, then move:

```powershell
build-mingw       -> _generated\build-mingw
build-web-tests   -> _generated\build-web-tests
logs              -> _generated\logs
perf_runs         -> _generated\perf_runs
web-ui\dist       -> _generated\web-dist
image_processing_platform -> E:\zhuomian\cybersecurity\_quarantine\image_processing_platform
```

- [x] **Step 3: Update runtime scripts**

`scripts/run_local.ps1` must use `_generated\build-mingw`, `_generated\logs\runtime`, and `_generated\logs\protocol_events`.

`scripts/run_perf_4clients.ps1` must default to `_generated\build-mingw` and `_generated\perf_runs`.

- [x] **Step 4: Update web generated outputs**

`web-ui/package.json` protocol self-test must compile into `../_generated/build-web-tests`.

`web-ui/vite.config.ts` must build into `../_generated/web-dist`.

### Task 2: Reorganize C++ Implementation Files

**Files:**
- Modify: `CMakeLists.txt`
- Move: `src/common/*.cpp` -> `src/shared/*/*.cpp`
- Move: `src/game/app_payload_codec.cpp`, `src/game/game_protocol.cpp`, `src/game/game_non_repudiation.cpp` -> `src/shared/game/`
- Move: `src/game/tank_game_server.cpp`, `src/game/battle_room.cpp`, `src/game/game_world.cpp` -> `src/roles/v/`
- Move: `src/game/tank_game_client.cpp`, `src/ui/ui_bridge.cpp`, `src/ui/websocket.cpp` -> `src/roles/client/`
- Move: `src/monitor/protocol_monitor.cpp` -> `src/roles/monitor/`
- Move: `src/common/README.md` -> `src/shared/README.md`

- [x] **Step 1: Create role/shared directories**

Create `src/shared/auth`, `src/shared/config`, `src/shared/crypto`, `src/shared/logging`, `src/shared/net`, `src/shared/protocol`, `src/shared/runtime`, and `src/shared/game`.

- [x] **Step 2: Move tracked files with git**

Use `git mv` for tracked C++ and README files so existing modifications are preserved and Git history stays readable.

- [x] **Step 3: Update CMake source list**

Replace old `src/common`, `src/game`, `src/ui`, and `src/monitor` implementation paths with the new `src/shared/*` and `src/roles/*` paths.

### Task 3: Add Role Guides and Update Main Docs

**Files:**
- Modify: `README.md`
- Modify: `当前系统设计参考稿.md`
- Modify: `src/roles/README.md`
- Modify: `src/shared/README.md`
- Create: `src/roles/as/README.md`
- Create: `src/roles/tgs/README.md`
- Create: `src/roles/v/README.md`
- Create: `src/roles/client/README.md`
- Create: `src/roles/monitor/README.md`

- [x] **Step 1: Add per-role README files**

Each role README must include:

```text
role startup executable
owned files
shared files likely to inspect during live modification
packet/message responsibilities
common modification points
```

- [x] **Step 2: Update top-level README**

The README must show:

```powershell
cmake -S . -B _generated\build-mingw -G Ninja
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
.\scripts\run_local.ps1
.\scripts\run_web.ps1
```

It must map AS, TGS, V, Client, and Monitor to their role README files and source locations.

- [x] **Step 3: Update system reference paths**

Update implementation path references and log paths from `logs/` to `_generated/logs/`.

### Task 4: Verify Final Chain

**Files:**
- No source edits expected after this task unless verification finds a path bug.

- [x] **Step 1: Reconfigure CMake**

Run:

```powershell
cmake -S . -B _generated\build-mingw -G Ninja
```

Expected: configuration succeeds and writes build files under `_generated/build-mingw`.

- [x] **Step 2: Build**

Run:

```powershell
cmake --build _generated\build-mingw
```

Expected: all C++ targets build.

- [x] **Step 3: Run C++ tests**

Run:

```powershell
ctest --test-dir _generated\build-mingw --output-on-failure
```

Expected: all registered tests pass.

- [x] **Step 4: Run web protocol verification**

Run:

```powershell
npm run verify
```

from `web-ui/`.

Expected: TypeScript compile/self-test passes.

- [x] **Step 5: Check root clarity**

Run:

```powershell
Get-ChildItem -Force | Select-Object Name
git status --short
rg -n "build-mingw|build-web-tests|perf_runs|logs\\\\|logs/|src/common|src/game|src/ui|src/monitor" README.md scripts CMakeLists.txt src 当前系统设计参考稿.md web-ui
```

Expected: root no longer contains generated output directories; remaining path references either point to `_generated/...` or are intentionally in archive docs.
