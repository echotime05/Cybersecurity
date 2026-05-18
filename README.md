# Cyber Tank Battle 现场运行说明

这是一个 Windows C++17 坦克大战课程设计项目。最终链路固定为：

```text
Browser Web UI <-> local client.exe WebSocket bridge <-> AS/TGS/V TCP services
Protocol panel <-> local monitor.exe WebSocket bridge <-> _generated/logs/protocol_events
```

浏览器不直接连接 V。浏览器只连接本机 `client.exe` 和本机 `monitor.exe`。`client.exe` 负责 Kerberos 认证、V 认证、证书交换、游戏报文签名加密、ACK 不可否认证据记录和 Web UI 状态转发。V 负责权威游戏状态计算和广播。

## 1. 目录结构

```text
config/                     最终运行配置
include/cyber/              公共头文件
src/roles/as/               AS 入口和 AS 汇报说明
src/roles/tgs/              TGS 入口和 TGS 汇报说明
src/roles/v/                V 入口、游戏服务器、世界状态
src/roles/client/           Client 入口、游戏客户端、WebSocket bridge
src/roles/monitor/          Protocol monitor
src/shared/                 四个角色共享的认证、加密、网络、协议、日志代码
scripts/                    现场运行脚本
tests/                      C++ 自测和链路测试
web-ui/                     浏览器 UI
_generated/                 构建产物、运行日志、压测输出、Web dist
```

`_generated/` 是生成物目录，正常修改功能时不用进入。现场改功能主要看 `src/roles/*`、`src/shared/*`、`include/cyber/*` 和 `web-ui/src/*`。

## 现场修改入口

现场被要求改功能时，先看 [docs/live_change_guide.md](docs/live_change_guide.md)。四个主要角色的代码入口分别是：

```text
AS:      src/roles/as/README.md
TGS:     src/roles/tgs/README.md
V:       src/roles/v/README.md
Client:  src/roles/client/README.md
Monitor: src/roles/monitor/README.md
```

协议字段和报文展示规则集中见 `src/shared/protocol/`、`src/shared/game/` 和后续协议参考文档。

## 2. 环境要求

- Windows
- CMake 3.16+
- Ninja 和 C++17 编译器在 `PATH` 中
- Node.js 和 npm 在 `PATH` 中

如果本机使用 Qt 自带工具链，但命令行找不到 CMake/Ninja/MinGW，可以临时执行：

```powershell
$env:Path = 'E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:Path
```

## 3. 构建和测试

在仓库根目录执行：

```powershell
cmake -S . -B _generated\build-mingw -G Ninja
cmake --build _generated\build-mingw
```

运行 C++ 测试：

```powershell
ctest --test-dir _generated\build-mingw --output-on-failure
```

检查 Web UI：

```powershell
cd web-ui
npm run verify
cd ..
```

## 4. 单机一键启动

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

停止本机 AS/TGS/V/Client/Monitor：

```powershell
.\scripts\stop_local.ps1
```

## 5. 分角色手动启动

以下命令都在仓库根目录执行。

### AS

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

AS 负责 Client 到 AS 的第一阶段认证，验证 Client ID 和长期密钥，返回 `Kc_tgs` 与 `Ticket_tgs`。

代码入口：`src/roles/as/README.md`

### TGS

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

TGS 负责第二阶段认证，验证 `Ticket_tgs` 和 `Authenticator_tgs`，返回 `Kc_v` 与 `Ticket_v`。

代码入口：`src/roles/tgs/README.md`

### V

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

V 负责 V_AUTH、Client/V 证书交换、游戏输入处理、权威世界状态计算、状态广播和 V 侧 `MSG_APP.APP_ACK` 协议事件。

代码入口：`src/roles/v/README.md`

### Client

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

Client 负责浏览器 WebSocket bridge、真实密码登录、AS/TGS/V 完整认证、游戏输入上报、状态接收、本地 UI 状态转发和 Client 侧 `MSG_APP.APP_ACK` 协议事件。

代码入口：`src/roles/client/README.md`

### Monitor 和 Web UI

```powershell
.\_generated\build-mingw\monitor.exe --ui-port 7010 --events-dir .\_generated\logs\protocol_events
.\scripts\run_web.ps1
```

Monitor 不参与认证和游戏计算。它只读取本机 `_generated/logs/protocol_events/*.txt`，把本机进程发包/收包事件推给浏览器 Protocol 面板。

代码入口：`src/roles/monitor/README.md`

## 6. 登录密码

```text
Client1: 123456
Client2: admin123
Client3: hehe12345
Client4: &wxh@147
```

## 7. 四机部署

四台机器必须使用一致的 AS/TGS/V 地址和端口。`config/course_config.txt` 中：

- `*_IP` / `*_HOST` 是其他进程连接的地址。
- `*_BIND_IP` 是本机监听地址，局域网演示通常用 `0.0.0.0`。
- 浏览器仍然只连接本机 `127.0.0.1:7001` 和 `127.0.0.1:7010`。

典型分工：

```text
Host 1: Client1
Host 2: AS + Client2
Host 3: TGS + Client3
Host 4: V + Client4
```

每个玩家机器启动：

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
.\_generated\build-mingw\monitor.exe --ui-port 7010 --events-dir .\_generated\logs\protocol_events
.\scripts\run_web.ps1
```

## 8. 脚本入口

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
_generated/perf_runs/<timestamp>/perf_report.txt
```

## 9. 日志

运行日志位于 `_generated/logs/`：

该目录由配置项 `LOG_ROOT=_generated/logs` 控制。最终演示配置都显式设置该值；如果临时测试配置没有写 `LOG_ROOT`，程序会优先使用当前目录下的 `_generated/logs`，否则回退到 `logs`。

```text
_generated/logs/
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

`protocol_events/*.txt` 是唯一的业务/协议日志，也是 Protocol 面板输入源。AS、TGS、V、Client 的发送和接收报文都写入这里；每行包含固定首部、`payload_hex`、`packet_hex`，以及可视化需要的 `payload_plain_hex`、`payload_encrypted_hex` 和加密字段拆分。双向不可否认的 `APP_ACK` 不再写单独 ACK 日志，而是作为 `MSG_APP.APP_ACK` 报文事件写入 `protocol_events`，其完整 ACK packet 由 `packet_hex` 保存。

`runtime/*.out` 和 `runtime/*.err` 只是脚本启动进程时重定向的 stdout/stderr，用来排查进程是否崩溃；Monitor 不读取它们。

## 10. 常见问题

如果浏览器提示 `127.0.0.1:5173` 拒绝连接，说明 Web UI 没启动，运行：

```powershell
.\scripts\run_web.ps1
```

如果登录卡在 `Authenticating`，检查 AS、TGS、V、Client 是否都在运行，并确认 V 使用：

```powershell
--game-auth-encrypted
```

如果 `client.exe` 无法监听 7001，先停止旧进程：

```powershell
.\scripts\stop_local.ps1
```
