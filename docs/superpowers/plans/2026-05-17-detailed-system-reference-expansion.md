# Detailed System Reference Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md` from an architecture summary into a field-level report reference that lets the user write the final report without re-reading C++ source code.

**Architecture:** Keep the existing 19-section structure, but add detailed subsections, tables, and prose inside each section. Treat the current repository code, README, tests, and performance report as the source of truth; treat the old Word report only as image and chapter-structure reference.

**Tech Stack:** Markdown, PowerShell, current C++17 source tree, existing `report_assets` files.

---

### Task 1: Rebuild The Source Fact Inventory

**Files:**
- Read: `E:\zhuomian\cybersecurity\code\README.md`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\common\types.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\common\packet.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\common\protocol_payloads.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\game\game_protocol.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\game\game_non_repudiation.hpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\packet.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\protocol_payloads.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\game_protocol.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\game_non_repudiation.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\auth_flow.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\tank_game_client.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\tank_game_server.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\protocol_event.cpp`
- Read: `E:\zhuomian\cybersecurity\code\web-ui\src\ProtocolMonitor.ts`
- Read: `E:\zhuomian\cybersecurity\code\web-ui\src\protocolPayload.ts`

- [ ] **Step 1: Capture section headings from the current draft**

Run:

```powershell
Get-Content -Encoding UTF8 'E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md' |
  Select-String -Pattern '^#|^##|^###' |
  ForEach-Object { $_.Line }
```

Expected: the 19 existing top-level sections are visible.

- [ ] **Step 2: Capture protocol field sources**

Run:

```powershell
Get-Content -Raw -Encoding UTF8 include\cyber\common\protocol_payloads.hpp
Get-Content -Raw -Encoding UTF8 include\cyber\game\game_protocol.hpp
Get-Content -Encoding UTF8 src\common\protocol_payloads.cpp | Select-Object -First 460
Get-Content -Encoding UTF8 src\game\game_protocol.cpp | Select-Object -First 560
```

Expected: all Kerberos, certificate, ACK, signed app, and game message payload field orders are available for rewriting into Markdown tables.

### Task 2: Expand Common Encoding, Packet, IDs, And Message Types

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Add a common binary encoding subsection**

Under sections 5 and 6, add prose and tables covering:

```text
u8: 1 byte unsigned integer
i8: 1 byte signed value serialized as raw byte
u16/u32/u64: big-endian
f32: IEEE-754 float32 bits serialized as big-endian u32
string_u8: u8 length + raw string bytes
bytes_u16: u16 length + raw bytes
all fixed Packet header integers use big-endian
```

- [ ] **Step 2: Replace the brief message-type summary with a detailed table**

The table must include numeric values and usage for:

```text
MSG_AS_REQ=1
MSG_AS_REP=2
MSG_TGS_REQ=3
MSG_TGS_REP=4
MSG_V_AUTH_REQ=5
MSG_V_AUTH_REP=6
MSG_CERT_C2V=7
MSG_CERT_V2C=8
MSG_ERROR=101
MSG_APP=102
```

- [ ] **Step 3: Add detailed AppCode and ErrorCode tables**

Include all AppCode values from `types.hpp`, and explicitly mark old `KEY_DOWN`, `KEY_UP`, `AIM_EVENT`, `FIRE_EVENT`, `GAME_START` as retained enum values rather than the current final gameplay path.

### Task 3: Expand Kerberos And Certificate Payload Sections

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Add AS stage payload tables**

Add field-level tables for:

```text
AS_REQ: idc u8, idtgs u8, ts1 u64
Ticket_tgs plain body: kc_tgs u64, idc u8, adc u32, idtgs u8, ts2 u64, lifetime2 u64
AS_REP plain body: kc_tgs u64, idtgs u8, ts2 u64, lifetime2 u64, ticket_tgs bytes_u16
AS_REP wire payload: DES(Kc, AS_REP plain body)
ticket_tgs field: DES(KTGS, Ticket_tgs plain body)
```

- [ ] **Step 2: Add TGS stage payload tables**

Add field-level tables for:

```text
Authenticator: idc u8, adc u32, ts u64
TGS_REQ: idv u8, ticket_tgs bytes_u16, authenticator_tgs bytes_u16
Ticket_v plain body: kc_v u64, idc u8, adc u32, idv u8, ts4 u64, lifetime4 u64
TGS_REP plain body: kc_v u64, idv u8, ts4 u64, ticket_v bytes_u16
TGS_REP wire payload: DES(Kc_tgs, TGS_REP plain body)
```

- [ ] **Step 3: Add V_AUTH and certificate payload tables**

Add field-level tables for:

```text
V_AUTH_REQ: ticket_v bytes_u16, authenticator_v bytes_u16
V_AUTH_REP plain body: ts5_plus_1 u64
CERT_C2V plain body: client_id u8, cert bytes_u16
CERT_V2C plain body: v_id u8, cert bytes_u16
CERT_C2V and CERT_V2C wire payload: DES(Kc_v, plain body)
```

### Task 4: Expand Crypto, Signature, MSG_APP, And APP_ACK

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Add exact signed app envelope structure**

Document:

```text
SignedAppPayload: app_code u8, app_payload bytes_u16, signature bytes_u16
signature input: hash64(app_code + app_payload)
Packet header is excluded from signature
encrypted game wire payload: DES(Kc_v, SignedAppPayload)
```

- [ ] **Step 2: Add GameMessage wrapper structure**

Document:

```text
GameMessage: game_msg_type u8, game_payload variable
app_code_for_game_message_type mapping:
join -> GAME_JOIN_REQ
move -> GAME_MOVE
target -> GAME_TARGET
shoot -> GAME_SHOOT
name -> GAME_NAME
state -> GAME_STATE
```

- [ ] **Step 3: Add APP_ACK field table and correlation rules**

Document:

```text
AppAckPayload: acked_msg_type u8, acked_app_code u8, acked_src u8, acked_dst u8, acked_payload_len u32, acked_payload_hash u64
acked_payload_hash = hash64(received_packet.payload)
acked_payload_len = received_packet.payload.size()
ACK packets are not acknowledged again
Verified ACK packet hex is logged in v_ack.log or client_ack.log
```

### Task 5: Expand Tank Game Application-Layer Protocol

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Add a full app-code-to-payload table**

For each current game app code, document MsgType, AppCode, GameMsgType, direction, encryption, ACK behavior, and payload:

```text
GAME_JOIN_REQ / join / JoinMessage
GAME_MOVE / move / MoveMessage
GAME_TARGET / target / TargetMessage
GAME_SHOOT / shoot / ShootMessage
GAME_NAME / name / NameMessage
GAME_STATE / state / BattleStateSnapshot
APP_ACK / no GameMessage / AppAckPayload
```

- [ ] **Step 2: Add field-level game payload tables**

Document:

```text
JoinMessage: client_id u8, name string_u8
MoveMessage: x i8, y i8
TargetMessage: angle f32
ShootMessage: shooting u8 boolean
NameMessage: name string_u8
BattleStateSnapshot top fields: server_time_ms u64, total_score u16, winner_team i8, team_count u8, teams[], tank_count u8, tanks[], bullet_count u16, bullets[], pickable_count u16, pickables[]
TeamSnapshot: team_id u8, score u16, tanks u8
TankSnapshot: client_id u8, name string_u8, team u8, x f32, y f32, angle f32, hp i8, shield i8, dead u8 boolean, score u16
BulletSnapshot: id u16, owner_client_id u8, x f32, y f32, special u8 boolean
PickableSnapshot: id u16, type u8, x f32, y f32
```

- [ ] **Step 3: Add gameplay semantics**

Document V authoritative behavior, tick update, movement, target, shooting, reload, bullet range, bullet removal, death/respawn, scoring, and pickup types in prose based on `battle_room.cpp` and `game_types.hpp`.

### Task 6: Expand Client/UI, Monitor, Logs, Threads, And Performance

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Expand Client/UI behavior**

Document login state transitions, UI commands, local WebSocket boundary, V socket reuse after auth, receive loop behavior, and why rendering is local but authoritative state is server-driven.

- [ ] **Step 2: Expand protocol event/log visualization**

Document `protocolEvent` fields:

```text
role, timestamp, direction, endpoint, message, category,
header.msgType/src/dst/payloadLen/reserved,
payloadHex, payloadPlainHex, payloadEncryptedHex, payloadFields[]
```

Document why Protocol UI can show encrypted and decrypted views for Kerberos subfields and game payloads.

- [ ] **Step 3: Expand logs and thread model**

Document every log path and thread boundary, including AS/TGS worker threads, V accept/client/game loop threads, Client UI bridge/receive loop/send mutex, Monitor tail loop, and async logger flush model.

- [ ] **Step 4: Expand performance interpretation**

Document what `scripts/run_perf_4clients.ps1` measures, what it does not measure, the latest observed 20ms tick result, and why protocol log volume is the primary visible pressure.

### Task 7: Verify The Expanded Reference

**Files:**
- Verify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Check size and headings**

Run:

```powershell
$path='E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md'
Get-Item $path | Select-Object FullName,Length,LastWriteTime
Get-Content -Encoding UTF8 $path | Select-String -Pattern '^## |^### ' | ForEach-Object { $_.Line }
```

Expected: 19 top-level sections remain, with detailed `###` subsections added.

- [ ] **Step 2: Search for ambiguity placeholders**

Run:

```powershell
Select-String -Path 'E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md' -Pattern 'TODO|TBD|待补|不确定|可能需要'
```

Expected: no matches.

- [ ] **Step 3: Verify field-level coverage terms**

Run:

```powershell
$path='E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md'
Select-String -Path $path -Pattern 'AS_REQ','Ticket_tgs','TGS_REQ','Ticket_v','V_AUTH_REQ','CERT_C2V','SignedAppPayload','GameMessage','GAME_MOVE','GAME_STATE','AppAckPayload','payloadPlainHex','payloadEncryptedHex','send_mutex','avg_state_interval_ms'
```

Expected: every pattern appears at least once.

- [ ] **Step 4: Check repository status**

Run:

```powershell
git -C E:\zhuomian\cybersecurity\code status --short --branch
```

Expected: generated plan files may be untracked under `docs/superpowers/plans`; unrelated `image_processing_platform/` remains unmodified.
