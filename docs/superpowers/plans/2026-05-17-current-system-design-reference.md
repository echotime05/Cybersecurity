# Current System Design Reference Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a Markdown reference draft that accurately summarizes the current Cyber Tank Battle system design for the user to consult while writing the final course report.

**Architecture:** Treat the old Word document as a read-only source for chapter structure and reusable images, while treating the current repository code, README, tests, and recent commits as the source of truth. Write one standalone Markdown reference file under the course-design directory, plus an optional image-asset index that tells the user which old images are reusable, outdated, or should be redrawn.

**Tech Stack:** Markdown, PowerShell, Windows filesystem, current C++17 repository files, old `.docx` treated as a ZIP/OpenXML container.

---

### Task 1: Build the Source Inventory

**Files:**
- Read: `E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx`
- Read: `E:\zhuomian\cybersecurity\code\README.md`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\common\types.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\common\protocol_payloads.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\game\game_types.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\game\game_protocol.hpp`
- Read: `E:\zhuomian\cybersecurity\code\include\cyber\game\game_non_repudiation.hpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\auth_flow.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\common\crypto.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\tank_game_client.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\game\tank_game_server.cpp`
- Read: `E:\zhuomian\cybersecurity\code\src\monitor\protocol_monitor.cpp`
- Create: `E:\zhuomian\cybersecurity\课程设计\report_assets\source_inventory.md`

- [ ] **Step 1: Confirm the old document exists and record metadata**

Run:

```powershell
Get-Item 'E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx' |
  Select-Object FullName,Length,LastWriteTime
```

Expected: the file exists and reports a nonzero `Length`.

- [ ] **Step 2: Record current repository version**

Run:

```powershell
git -C E:\zhuomian\cybersecurity\code log -5 --oneline
git -C E:\zhuomian\cybersecurity\code status --short --branch
```

Expected: top commit is the latest pushed `main`; unrelated `image_processing_platform/` may remain untracked and must not be included in this documentation work.

- [ ] **Step 3: Create source inventory file**

Create `E:\zhuomian\cybersecurity\课程设计\report_assets\source_inventory.md` with this exact structure:

```markdown
# Source Inventory

## Old Reference Document

- Path: `E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx`
- Use: chapter structure and image reference only
- Do not use as source of truth for current behavior

## Current Source Of Truth

- `README.md`: runtime, deployment, logs, performance test entry
- `include/cyber/common/types.hpp`: entity IDs, message types, app codes, error codes
- `include/cyber/common/protocol_payloads.hpp`: Kerberos, certificate, and ACK payload structures
- `include/cyber/game/game_types.hpp`: game constants such as 20ms tick and 32 unit bullet range
- `include/cyber/game/game_protocol.hpp`: game app payload structures
- `include/cyber/game/game_non_repudiation.hpp`: signed/encrypted app packet and ACK design
- `src/common/auth_flow.cpp`: AS/TGS/V auth and certificate exchange flow
- `src/common/crypto.cpp`: DES-style payload encryption, padding, hash, RSA-style signatures
- `src/game/tank_game_client.cpp`: login, join, game command send, state receive, ACK send
- `src/game/tank_game_server.cpp`: V authentication, authoritative world state, broadcast, ACK handling
- `src/monitor/protocol_monitor.cpp`: protocol log tailing and WebSocket bridge

## Current Final Runtime Facts

- Public runtime mode: `--game-auth-encrypted`
- Browser connects only to local `client.exe`; browser does not connect directly to V
- Protocol panel connects to local `monitor.exe`
- Game server tick interval: 20ms
- Bullet range: 32 world units
- Game payloads between Client and V are signed and encrypted with `Kc_v`
- Every non-ACK game packet is ACKed with signed encrypted `APP_ACK`
```

### Task 2: Extract Old Document Structure And Image Assets

**Files:**
- Read: `E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx`
- Create directory: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_media`
- Create: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_outline.md`
- Create: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_image_index.md`

- [ ] **Step 1: Extract the first 150 non-empty paragraphs from the old docx**

Run this PowerShell command:

```powershell
$doc='E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx'
$out='E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_outline.md'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[System.IO.Compression.ZipFile]::OpenRead($doc)
try {
  $entry=$zip.GetEntry('word/document.xml')
  $sr=New-Object System.IO.StreamReader($entry.Open())
  $xmlText=$sr.ReadToEnd()
  $sr.Close()
  $xml=New-Object System.Xml.XmlDocument
  $xml.LoadXml($xmlText)
  $ns=New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
  $ns.AddNamespace('w','http://schemas.openxmlformats.org/wordprocessingml/2006/main')
  $lines=@('# Old DOCX Outline Sample','')
  foreach($p in $xml.SelectNodes('//w:p',$ns)){
    $text=(($p.SelectNodes('.//w:t',$ns) | ForEach-Object { $_.'#text' }) -join '').Trim()
    if($text.Length -gt 0){ $lines += "- $text" }
    if($lines.Count -ge 152){ break }
  }
  $lines | Set-Content -LiteralPath $out -Encoding UTF8
} finally {
  $zip.Dispose()
}
```

Expected: `old_docx_outline.md` contains the old table of contents and early chapter text for reference.

- [ ] **Step 2: Extract all embedded media files**

Run this PowerShell command:

```powershell
$doc='E:\zhuomian\cybersecurity\课程设计\网络安全设计报告_日志设计修订版.docx'
$mediaDir='E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_media'
New-Item -ItemType Directory -Force $mediaDir | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[System.IO.Compression.ZipFile]::OpenRead($doc)
try {
  foreach($entry in $zip.Entries | Where-Object { $_.FullName -like 'word/media/*' }) {
    $target=Join-Path $mediaDir ([System.IO.Path]::GetFileName($entry.FullName))
    [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
  }
} finally {
  $zip.Dispose()
}
```

Expected: `old_docx_media` contains the image files from the old report.

- [ ] **Step 3: Create image-use index**

Create `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_image_index.md` with this exact structure:

```markdown
# Old DOCX Image Index

This index is for report-writing reference. The old document is not the source of truth for current system behavior.

## Reusable With Little Or No Change

- Login / UI screenshots that still match the current Web UI.
- Packet-format explanatory images if they only illustrate generic fixed header + payload concepts.

## Reusable After Rechecking

- Kerberos flow diagrams, only if they show AS -> TGS -> V and do not claim that the browser talks directly to V.
- Deployment diagrams, only if updated to show local `monitor.exe` and local browser-to-client WebSocket.
- Log display screenshots, only if they match current Protocol panel behavior.

## Should Be Redrawn Or Rewritten

- Any diagram saying Client reads AS/TGS/V logs directly.
- Any diagram saying GAME_START waits for all four clients before state broadcast.
- Any diagram saying app-layer game traffic is plaintext in final runtime.
- Any figure describing old 33ms tick or 16 unit bullet range.
```

### Task 3: Write The Reference Markdown Skeleton

**Files:**
- Create: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`

- [ ] **Step 1: Create the top-level Markdown skeleton**

Create `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md` with these headings exactly:

```markdown
# 当前系统设计参考稿

> 本文档用于辅助撰写最终课程设计报告，不作为直接提交的排版版报告。旧 Word 文档只作为章节结构和图片素材参考；当前实现以代码、README、测试和最近提交为准。

## 1. 文档定位与当前版本

## 2. 系统总体架构

## 3. 角色职责

## 4. 运行模式与部署方式

## 5. 配置、实体 ID 与消息类型

## 6. 通用报文格式

## 7. Kerberos 认证流程

## 8. 证书交换流程

## 9. 加密、签名与消息认证

## 10. 双向不可否认与 APP_ACK

## 11. 坦克大战应用层设计

## 12. Client 与 Web UI 设计

## 13. Protocol Monitor 与报文可视化

## 14. 日志设计

## 15. 线程模型与并发边界

## 16. 性能测试与运行参数

## 17. 旧文档中已推翻或需修正的设计

## 18. 可复用图片建议

## 19. 写最终报告时建议展开的内容
```

Expected: the file is only a skeleton at this step.

### Task 4: Fill Current Architecture And Runtime Sections

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`
- Source: `README.md`
- Source: `scripts/run_local.ps1`
- Source: `scripts/run_web.ps1`
- Source: `scripts/run_perf_4clients.ps1`

- [ ] **Step 1: Fill sections 1 to 4**

Write concise but detailed prose under sections 1 to 4 covering these exact facts:

```text
当前项目是 Windows C++17 Kerberos-style tank battle demo with browser UI.
Browser UI <-> local client.exe WebSocket bridge <-> AS/TGS/V over TCP.
Protocol panel <-> local monitor.exe WebSocket bridge <-> local packet logs.
Browser never connects directly to V.
Final runtime is --game-auth-encrypted.
AS and TGS are request/response TCP servers.
V authenticates clients, owns authoritative BattleRoom, broadcasts GAME_STATE.
client.exe handles real password login, AS/TGS/V authentication, certificate exchange, game input forwarding, state receiving, and ACK sending.
monitor.exe tails logs/protocol_events/*.txt and sends protocolEvent JSON to the browser.
Single-machine mode uses run_local.ps1 and run_web.ps1.
Four-host deployment uses shared config and local browser/client/monitor on each player host.
```

- [ ] **Step 2: Mark old-design corrections inline**

Add a short paragraph in section 4:

```markdown
与旧文档不同，当前浏览器 UI 不直接连接 V，也不由 Client 直接读取 AS/TGS/V 的普通日志。浏览器只连接本地 `client.exe` 和本地 `monitor.exe`；`monitor.exe` 只展示本机进程写出的协议事件日志。
```

### Task 5: Fill Protocol, Kerberos, Crypto, And Non-Repudiation Sections

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`
- Source: `include/cyber/common/types.hpp`
- Source: `include/cyber/common/packet.hpp`
- Source: `include/cyber/common/protocol_payloads.hpp`
- Source: `include/cyber/game/game_non_repudiation.hpp`
- Source: `src/common/auth_flow.cpp`
- Source: `src/common/crypto.cpp`

- [ ] **Step 1: Fill sections 5 and 6**

Document these concrete protocol facts:

```text
Packet header is 11 bytes:
msg_type 1B
src 1B
dst 1B
payload_len 4B
reserved 4B
payload variable length

Entity IDs:
Client1 0x01
Client2 0x02
Client3 0x03
Client4 0x04
AS 0x11
TGS 0x12
V 0x13

Important MsgType values:
MSG_AS_REQ / MSG_AS_REP
MSG_TGS_REQ / MSG_TGS_REP
MSG_V_AUTH_REQ / MSG_V_AUTH_REP
MSG_CERT_C2V / MSG_CERT_V2C
MSG_APP
MSG_ERROR
```

- [ ] **Step 2: Fill sections 7 and 8**

Explain the current authentication chain:

```text
Client derives Kc from real user password.
AS validates by producing AS_REP encrypted for Kc; wrong password cannot decode a valid AS_REP.
AS_REP carries Kc_tgs and Ticket_tgs.
TGS_REQ carries Ticket_tgs and authenticator_tgs.
TGS_REP carries Kc_v and Ticket_v.
V_AUTH_REQ carries Ticket_v and authenticator_v.
After V_AUTH, CERT_C2V and CERT_V2C exchange runtime public keys/certificates.
The V socket remains open after authentication and becomes the application game socket.
```

- [ ] **Step 3: Fill sections 9 and 10**

Document these security facts:

```text
DES-style encryption applies to payload bytes, not fixed Packet headers.
Application MSG_APP game payloads are signed then encrypted with Kc_v.
RSA-style signature signs hash64 of the signed body, not the whole raw packet.
Packet header fields are not part of the RSA signature.
Every non-ACK game packet is acknowledged with APP_ACK.
APP_ACK is also a signed and encrypted MSG_APP.
ACK references the original packet by message metadata, payload length, and payload hash.
ACK packets are evidence records and are not acknowledged again.
Verified ACK packets are written to logs/v_ack.log or logs/client_ack.log as APP_NON_REPUDIATION_ACK packet_hex=...
```

### Task 6: Fill Game, UI, Monitor, Logs, Threads, And Performance Sections

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`
- Source: `include/cyber/game/game_types.hpp`
- Source: `include/cyber/game/game_protocol.hpp`
- Source: `src/game/battle_room.cpp`
- Source: `src/game/tank_game_client.cpp`
- Source: `src/game/tank_game_server.cpp`
- Source: `src/ui/ui_bridge.cpp`
- Source: `src/monitor/protocol_monitor.cpp`
- Source: latest `perf_runs/*/perf_report.txt` if present

- [ ] **Step 1: Fill section 11**

Document these current game facts:

```text
V is authoritative.
Client sends join, move, target, shoot, and name messages.
V applies valid inputs to BattleRoom.
V broadcasts GAME_STATE snapshots.
Game server tick interval is 20ms.
Tank speed is scaled from old 33ms pace by 20/33.
Bullet speed is scaled from old 33ms pace by 20/33.
Bullet range is 32 world units.
World size is 48 x 48.
Bullets disappear when reaching target range, leaving world bounds, colliding with blocks, or hitting enemy tanks.
```

- [ ] **Step 2: Fill sections 12 and 13**

Document these UI facts:

```text
Web UI has Game and Protocol views.
Login UI sends client ID and password to local client.exe over WebSocket.
Join Game appears after successful authentication.
Game rendering uses authoritative GAME_STATE from V.
Protocol panel consumes protocolEvent JSON from monitor.exe.
Protocol view shows fixed header fields, structured payload parse, plain hex, encrypted hex, direction, endpoint, timestamp, and message name.
Noisy high-frequency packet types can be hidden in the Protocol UI.
```

- [ ] **Step 3: Fill sections 14 and 15**

Document logs and threading:

```text
logs/as.log
logs/tgs.log
logs/v_game.log
logs/client_game.log
logs/v_ack.log
logs/client_ack.log
logs/protocol_events/*.txt
logs/runtime/*.out
logs/runtime/*.err

AS/TGS accept TCP connections and dispatch worker handling.
V has accept/client worker threads plus a game loop thread.
Client has a UI bridge thread and V receive loop.
send_mutex prevents concurrent writes to the V socket.
monitor.exe tails text protocol-event files and broadcasts WebSocket JSON to the browser.
```

- [ ] **Step 4: Fill section 16**

Record the current performance evidence:

```text
The four-client headless performance script is scripts/run_perf_4clients.ps1.
It starts isolated AS/TGS/V processes on random localhost ports.
It runs four simulated authenticated clients through encrypted game traffic.
Recent 20ms tick 60-second run observed avg_state_interval_ms=20.00, max_state_interval_ms around 24.46, errors=0.
Main observed pressure is protocol event log volume rather than V CPU.
```

### Task 7: Fill Old-Document Correction And Image Guidance Sections

**Files:**
- Modify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`
- Source: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_outline.md`
- Source: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_image_index.md`

- [ ] **Step 1: Fill section 17 with corrected design points**

Write a bullet list with these exact corrections:

```markdown
- 旧文档中“AS 阶段完成即可进入下一界面”的表述不符合当前实现。当前 UI 在完整 AS/TGS/V_AUTH/CERT 成功后才进入可加入游戏状态。
- 旧文档中“等待所有 Client 后由 V 发送 GAME_START”的设计不作为当前最终行为。当前实现中玩家 join 后即可由 V 的权威世界状态驱动游戏。
- 旧文档中“Client 直接读取本机 AS/TGS/V 普通日志并展示”的设计已被替换为 `monitor.exe` 读取 `logs/protocol_events/*.txt`。
- 旧文档中应用层明文游戏报文不是最终运行模式。最终运行模式是 `--game-auth-encrypted`，游戏 `MSG_APP` payload 签名后加密。
- 旧文档中若出现 33ms tick 或 16 单位子弹射程，需要改为当前 20ms tick 和 32 单位子弹射程。
- 旧文档中若有浏览器直接连接 V 的图，应改为浏览器连接本地 `client.exe`，由 `client.exe` 连接 V。
```

- [ ] **Step 2: Fill sections 18 and 19**

Write practical report-writing guidance:

```text
Reusable images: login/game UI screenshots if they match current UI; generic packet-format diagrams; generic Kerberos role diagrams after checking arrows.
Images to redraw: whole-system flow, deployment/log UI, encrypted payload visualization if old image does not include monitor.exe and protocol_events.
Final report should emphasize current implementation choices, not historical alternatives.
The Markdown reference intentionally omits pseudocode unless a structure is hard to explain in prose.
```

### Task 8: Review And Verify The Reference Draft

**Files:**
- Read/Verify: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`
- Read/Verify: `E:\zhuomian\cybersecurity\课程设计\report_assets\source_inventory.md`
- Read/Verify: `E:\zhuomian\cybersecurity\课程设计\report_assets\old_docx_image_index.md`

- [ ] **Step 1: Check the Markdown exists and has all sections**

Run:

```powershell
$path='E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md'
Get-Item $path | Select-Object FullName,Length,LastWriteTime
Select-String -Path $path -Pattern '^## ' | Select-Object Line
```

Expected: all sections 1 through 19 appear.

- [ ] **Step 2: Search for forbidden ambiguity markers**

Run:

```powershell
Select-String -Path 'E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md' -Pattern 'TODO|TBD|待补|不确定|可能需要'
```

Expected: no matches. If a design point is uncertain, remove it or rewrite it as a concrete recommendation for the final report.

- [ ] **Step 3: Verify the core final facts are present**

Run:

```powershell
$path='E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md'
Select-String -Path $path -Pattern '20ms','32','--game-auth-encrypted','APP_ACK','monitor.exe','logs/protocol_events','Kc_v','payload_len'
```

Expected: every pattern is present at least once.

- [ ] **Step 4: Do not commit generated course-design files unless explicitly requested**

Run:

```powershell
git -C E:\zhuomian\cybersecurity\code status --short --branch
```

Expected: repository status remains unchanged except for existing unrelated `image_processing_platform/` or this plan file if it has not yet been committed.

### Self-Review Checklist

- [ ] Scope is limited to a reference Markdown draft and asset index, not a formal report.
- [ ] Old docx is read-only and not treated as current truth.
- [ ] Current code/README/tests are the source of truth.
- [ ] No step asks for direct edits to the old Word document.
- [ ] The final draft path is outside the repo: `E:\zhuomian\cybersecurity\课程设计\当前系统设计参考稿.md`.
- [ ] The draft explicitly marks old-design corrections.
- [ ] No pseudocode-heavy report sections are required.
