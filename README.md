# Cyber Tank Battle 现场运行说明

这是一个 Windows C++17 坦克大战课程设计。最终链路固定为：

```text
Browser Web UI <-> local client.exe WebSocket bridge <-> AS/TGS/V TCP services
Protocol panel <-> local monitor.exe WebSocket bridge <-> logs/protocol_events
```

浏览器不直接连接 V。浏览器只连接本机 `client.exe`，`client.exe` 完成 Kerberos 认证、V 认证、证书交换、游戏报文签名加密和 ACK 不可否认证据记录。V 负责权威游戏状态计算并广播状态。

## 1. 环境要求

- Windows
- CMake 3.16+
- Ninja 和 C++17 编译器在 `PATH` 中
- Node.js 和 npm 在 `PATH` 中

如果本机使用 Qt 自带工具链，但命令行找不到 CMake/Ninja/MinGW，可以先临时加入 PATH：

```powershell
$env:Path = 'E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:Path
```

## 2. 构建

在仓库根目录执行：

```powershell
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
```

运行测试：

```powershell
ctest --test-dir build-mingw --output-on-failure
```

检查 Web UI：

```powershell
cd web-ui
npm run verify
cd ..
```

## 3. 单机一键启动

默认配置 `config/course_config.txt` 是本机演示配置，AS/TGS/V 都在 `127.0.0.1`，本机 Client 是 `Client1`。

启动 C++ 后端：

```powershell
.\scripts\run_local.ps1
```

启动 Web UI：

```powershell
.\scripts\run_web.ps1
```

浏览器打开：

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

停止 C++ 后端：

```powershell
.\scripts\stop_local.ps1
```

## 4. 分角色手动启动

如果 4 个人分别汇报，可以在不同终端手动启动各角色。以下命令都在仓库根目录执行。

### AS

```powershell
.\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

AS 负责 Client 到 AS 的第一阶段认证，验证 Client ID 和长期密钥，返回 `Kc_tgs` 与 `Ticket_tgs`。

主要源码：

```text
src/roles/as/main.cpp
src/common/role_runtime.cpp
src/common/auth_flow.cpp
src/common/protocol_payloads.cpp
src/common/auth_credentials.cpp
src/common/crypto.cpp
```

### TGS

```powershell
.\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

TGS 负责第二阶段认证，验证 `Ticket_tgs` 和 `Authenticator_tgs`，返回 `Kc_v` 与 `Ticket_v`。

主要源码：

```text
src/roles/tgs/main.cpp
src/common/role_runtime.cpp
src/common/auth_flow.cpp
src/common/protocol_payloads.cpp
src/common/crypto.cpp
```

### V

```powershell
.\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

V 负责 V_AUTH、Client/V 证书交换、加密签名游戏报文处理、权威世界状态计算、状态广播和 V 侧 ACK 证据日志。

主要源码：

```text
src/roles/v/main.cpp
src/common/role_runtime.cpp
src/common/auth_flow.cpp
src/game/tank_game_server.cpp
src/game/battle_room.cpp
src/game/game_world.cpp
src/game/game_protocol.cpp
src/game/app_payload_codec.cpp
src/game/game_non_repudiation.cpp
```

### Client

```powershell
.\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

Client 负责浏览器 WebSocket 桥接、真实密码登录、AS/TGS/V 完整认证、游戏输入上报、状态接收、本地 UI 状态转发和 Client 侧 ACK 证据日志。

主要源码：

```text
src/roles/client/main.cpp
src/common/role_runtime.cpp
src/common/auth_flow.cpp
src/game/tank_game_client.cpp
src/game/game_protocol.cpp
src/game/app_payload_codec.cpp
src/game/game_non_repudiation.cpp
src/ui/ui_bridge.cpp
src/ui/websocket.cpp
web-ui/src/Game.ts
web-ui/src/Network.ts
```

### Monitor 和 Web UI

```powershell
.\build-mingw\monitor.exe --ui-port 7010 --events-dir .\logs\protocol_events
.\scripts\run_web.ps1
```

Monitor 不参与认证和游戏计算。它只读取本机 `logs/protocol_events/*.txt`，把本机进程发包/收包事件推给浏览器 Protocol 面板。

主要源码：

```text
src/roles/monitor/main.cpp
src/monitor/protocol_monitor.cpp
src/common/protocol_event.cpp
web-ui/src/ProtocolMonitor.ts
web-ui/src/protocolPayload.ts
```

## 5. 登录密码

```text
Client1: 123456
Client2: admin123
Client3: hehe12345
Client4: &wxh@147
```

## 6. 四机部署

四台机器必须使用一致的 AS/TGS/V 地址和端口。可以从 `config/lan/` 复制模板到各机器的 `config/course_config.txt`，再按现场 IP 修改：

```text
Host 1: Client1
Host 2: AS + Client2
Host 3: TGS + Client3
Host 4: V + Client4
```

每台机器上：

- `*_IP` 是其他进程连接的地址。
- `*_BIND_IP` 是本机监听地址。局域网演示通常用 `0.0.0.0`。
- 浏览器仍然只连接本机 `127.0.0.1:7001` 和 `127.0.0.1:7010`。

AS 机器启动：

```powershell
.\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

TGS 机器启动：

```powershell
.\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

V 机器启动：

```powershell
.\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

每个玩家机器启动：

```powershell
.\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
.\build-mingw\monitor.exe --ui-port 7010 --events-dir .\logs\protocol_events
.\scripts\run_web.ps1
```

浏览器打开：

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

## 7. 脚本入口

当前保留的脚本只有这些：

```text
scripts/run_local.ps1          单机启动 AS/TGS/V/Client/Monitor
scripts/stop_local.ps1         停止本机 AS/TGS/V/Client/Monitor
scripts/run_web.ps1            启动 Web UI dev server
scripts/run_perf_4clients.ps1  四 Client 加密链路压测
```

四 Client 压测：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_perf_4clients.ps1 -DurationSeconds 60 -InputHz 10
```

压测报告写入：

```text
perf_runs/<timestamp>/perf_report.txt
```

## 8. 日志

运行日志位于 `logs/`：

```text
logs/
  as.log
  tgs.log
  v_game.log
  client_game.log
  v_ack.log
  client_ack.log

  protocol_events/
    as_xxx.txt
    tgs_xxx.txt
    v_xxx.txt
    client_xx_xxx.txt

  runtime/
    as_server.out
    as_server.err
    tgs_server.out
    tgs_server.err
    v_server.out
    v_server.err
    client.out
    client.err
    monitor.out
    monitor.err
```

`v_ack.log` 和 `client_ack.log` 是双向不可否认证据日志。每条已验证 ACK 都会记录完整 ACK packet 的十六进制。

`protocol_events/*.txt` 是 Protocol 面板输入源。Monitor 只读取这些文件，不读取普通运行日志。

## 9. 常见问题

如果浏览器提示 `127.0.0.1:5173` 拒绝连接，说明 Web UI 没启动，运行：

```powershell
.\scripts\run_web.ps1
```

如果登录卡在 `Authenticating`，检查 AS、TGS、V、Client 是否都在运行，并确认 V 使用的是：

```powershell
--game-auth-encrypted
```

如果 `client.exe` 无法监听 7001，先停掉旧进程：

```powershell
.\scripts\stop_local.ps1
```
