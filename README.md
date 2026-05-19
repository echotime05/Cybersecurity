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

协议字段和报文展示规则集中见 `src/shared/protocol/`、`src/shared/game/` 和协议参考文档。
完整报文字段表见 [docs/protocol_reference.md](docs/protocol_reference.md)。

## 源码文件职责索引

本节只覆盖 `include/` 和 `src/`。`include/` 主要是接口、类型和协议结构定义；`src/` 主要是具体实现和各角色入口。

### include/cyber/common

| 文件 | 职责 |
| --- | --- |
| `include/cyber/common/auth_credentials.hpp` | 声明 Client 密码、长期密钥派生、Client secret 查询等认证凭据接口。 |
| `include/cyber/common/config.hpp` | 声明配置文件读取器 `Config` 和字符串、整数配置访问接口。 |
| `include/cyber/common/crypto.hpp` | 声明 DES-style 加解密、hash、RSA 签名验签、公钥、证书序列化和验证接口。 |
| `include/cyber/common/log_parser.hpp` | 声明旧文本日志解析工具，主要服务日志相关自测。 |
| `include/cyber/common/logger.hpp` | 声明异步行日志写入器，被协议事件日志底层使用。 |
| `include/cyber/common/net_packet.hpp` | 声明带协议事件记录的 TCP packet 收发函数。 |
| `include/cyber/common/net_socket.hpp` | 声明 TCP socket、监听、连接、读写和运行时初始化接口。 |
| `include/cyber/common/role_runtime.hpp` | 声明 AS/TGS/V/Client/Monitor 的命令行角色运行入口。 |
| `include/cyber/common/runtime_paths.hpp` | 声明运行时路径解析和日志根目录推导接口。 |
| `include/cyber/common/types.hpp` | 定义 `EntityId`、`MsgType`、`AppCode`、`ErrorCode` 等全局枚举和名称转换。 |

### include/cyber/protocol

| 文件 | 职责 |
| --- | --- |
| `include/cyber/protocol/app_envelope.hpp` | 定义 `SignedAppPayload`、`AppAckPayload`，声明应用层签名 payload 和 ACK codec。 |
| `include/cyber/protocol/certificate_messages.hpp` | 定义 Client/V 证书交换 payload，声明 `MSG_CERT_C2V`、`MSG_CERT_V2C` codec。 |
| `include/cyber/protocol/kerberos_messages.hpp` | 定义 AS/TGS/V_AUTH 的票据、认证器、请求响应结构，声明 Kerberos payload codec。 |
| `include/cyber/protocol/packet.hpp` | 定义固定 11B Packet header、`Packet`、错误 payload、简单 APP payload 和 hex 工具。 |
| `include/cyber/protocol/protocol_event.hpp` | 定义 Protocol Monitor 事件结构，声明协议事件格式化、解析、JSON 和写日志接口。 |

### include/cyber/game

| 文件 | 职责 |
| --- | --- |
| `include/cyber/game/app_payload_codec.hpp` | 声明游戏 `MSG_APP` payload 的明文/密文编码开关。 |
| `include/cyber/game/battle_room.hpp` | 声明 V 侧权威房间 `BattleRoom`，负责玩家、输入、状态快照和世界 tick。 |
| `include/cyber/game/game_non_repudiation.hpp` | 声明游戏报文签名、验签、构造 ACK、解析 ACK 的双向不可否认接口。 |
| `include/cyber/game/game_protocol.hpp` | 定义坦克大战应用层 `GameMessage`、输入消息、状态快照和序列化接口。 |
| `include/cyber/game/game_types.hpp` | 定义游戏世界内部实体类型，例如坦克、子弹、补给、障碍物和输入状态。 |
| `include/cyber/game/game_world.hpp` | 声明 V 侧世界模拟 `GameWorld`，负责移动、碰撞、子弹、补给和快照生成。 |

### include/cyber/roles

| 文件 | 职责 |
| --- | --- |
| `include/cyber/roles/as/as_service.hpp` | 声明 AS 处理单个 TCP 连接的角色入口。 |
| `include/cyber/roles/tgs/tgs_service.hpp` | 声明 TGS 处理单个 TCP 连接的角色入口。 |
| `include/cyber/roles/v/v_auth_service.hpp` | 定义 V 认证会话表和运行时，声明 V_AUTH 与证书交换处理接口。 |
| `include/cyber/roles/v/tank_game_server.hpp` | 声明 V 游戏服务器类，负责长连接、认证、游戏收包、广播和停止控制。 |
| `include/cyber/roles/client/client_auth_flow.hpp` | 定义 Client 认证状态和已认证 V socket，声明 Client 侧完整认证流程。 |
| `include/cyber/roles/client/tank_game_client.hpp` | 声明 Client 游戏客户端类，负责 UI bridge、登录、发包、收包和状态转发。 |

### include/cyber/ui and include/cyber/monitor

| 文件 | 职责 |
| --- | --- |
| `include/cyber/ui/ui_bridge.hpp` | 声明 Client 与浏览器 Web UI 之间的 JSON 命令和状态桥。 |
| `include/cyber/ui/websocket.hpp` | 声明轻量 WebSocket server、连接对象和消息回调。 |
| `include/cyber/monitor/protocol_monitor.hpp` | 声明 Protocol Monitor server，负责读取协议事件日志并推送给 Web UI。 |

### src/roles/as

| 文件 | 职责 |
| --- | --- |
| `src/roles/as/main.cpp` | AS 可执行程序入口，调用统一角色运行时。 |
| `src/roles/as/as_service.cpp` | AS 核心逻辑：接收 `MSG_AS_REQ`、验证 Client、生成 `Kc_tgs` 和 `Ticket_tgs`、返回 `MSG_AS_REP`。 |
| `src/roles/as/README.md` | AS 角色汇报和现场修改说明。 |

### src/roles/tgs

| 文件 | 职责 |
| --- | --- |
| `src/roles/tgs/main.cpp` | TGS 可执行程序入口，调用统一角色运行时。 |
| `src/roles/tgs/tgs_service.cpp` | TGS 核心逻辑：解析 `Ticket_tgs` 和 `Authenticator_tgs`，生成 `Kc_v`、`Ticket_v` 和 `MSG_TGS_REP`。 |
| `src/roles/tgs/README.md` | TGS 角色汇报和现场修改说明。 |

### src/roles/v

| 文件 | 职责 |
| --- | --- |
| `src/roles/v/main.cpp` | V 可执行程序入口，调用统一角色运行时。 |
| `src/roles/v/v_auth_service.cpp` | V 认证逻辑：处理 `MSG_V_AUTH_REQ`，维护认证会话，处理 Client/V 证书交换。 |
| `src/roles/v/tank_game_server.cpp` | V 长连接游戏服务器：接收 Client 连接、完成认证、处理 `MSG_APP`、发送 ACK、广播 `GAME_STATE`。 |
| `src/roles/v/battle_room.cpp` | V 权威房间逻辑：玩家加入、输入应用、tick 推进、计分和状态快照。 |
| `src/roles/v/game_world.cpp` | 游戏世界模拟：坦克移动、子弹飞行、碰撞、补给生成和世界规则。 |
| `src/roles/v/README.md` | V 角色汇报和现场修改说明。 |

### src/roles/client

| 文件 | 职责 |
| --- | --- |
| `src/roles/client/main.cpp` | Client 可执行程序入口，调用统一角色运行时。 |
| `src/roles/client/client_auth_flow.cpp` | Client 侧完整认证流程：AS_REQ、TGS_REQ、V_AUTH、证书交换，并复用 V socket。 |
| `src/roles/client/tank_game_client.cpp` | Client 游戏逻辑：处理浏览器登录和输入，向 V 发送签名加密游戏报文，接收状态并回 ACK。 |
| `src/roles/client/ui_bridge.cpp` | Client 与 Web UI 的 JSON bridge：解析浏览器命令，广播登录状态和游戏状态。 |
| `src/roles/client/websocket.cpp` | 轻量 WebSocket 实现：HTTP upgrade、frame 解析、消息发送和连接管理。 |
| `src/roles/client/README.md` | Client 角色汇报和现场修改说明。 |

### src/roles/monitor

| 文件 | 职责 |
| --- | --- |
| `src/roles/monitor/main.cpp` | Monitor 可执行程序入口，启动协议监控 WebSocket server。 |
| `src/roles/monitor/protocol_monitor.cpp` | Protocol Monitor 实现：tail `protocol_events/*.txt`，去重排序后推送给浏览器。 |
| `src/roles/monitor/README.md` | Monitor 角色汇报和现场修改说明。 |

### src/shared/auth

| 文件 | 职责 |
| --- | --- |
| `src/shared/auth/auth_credentials.cpp` | Client 密码、长期密钥派生、Client secret 查询和演示密钥表。 |

### src/shared/config

| 文件 | 职责 |
| --- | --- |
| `src/shared/config/config.cpp` | 配置文件解析和 `Config` 查询实现。 |

### src/shared/crypto

| 文件 | 职责 |
| --- | --- |
| `src/shared/crypto/crypto.cpp` | DES-style payload 加解密、hash64、演示 RSA、证书序列化和证书验证实现。 |

### src/shared/game

| 文件 | 职责 |
| --- | --- |
| `src/shared/game/app_payload_codec.cpp` | 根据开关对游戏应用层 payload 做明文传输或 `Kc_v` 加密/解密。 |
| `src/shared/game/game_non_repudiation.cpp` | 构造和解析签名游戏报文，验证签名，构造和验证 `APP_ACK`。 |
| `src/shared/game/game_protocol.cpp` | `GameMessage`、移动、瞄准、开火、加入、世界快照的二进制 codec 和 UI JSON 格式化。 |

### src/shared/logging

| 文件 | 职责 |
| --- | --- |
| `src/shared/logging/log_parser.cpp` | 旧 bracket-style 文本日志解析实现，主要用于日志自测。 |
| `src/shared/logging/logger.cpp` | 异步线程安全日志写入器，供协议事件日志底层复用。 |

### src/shared/net

| 文件 | 职责 |
| --- | --- |
| `src/shared/net/net_packet.cpp` | Packet 级 TCP 收发实现，并在收发时写入 Protocol Monitor 事件。 |
| `src/shared/net/net_socket.cpp` | Winsock 初始化、监听、连接、accept、send/recv 和 socket 关闭实现。 |

### src/shared/protocol

| 文件 | 职责 |
| --- | --- |
| `src/shared/protocol/packet.cpp` | 固定 11B Packet header、packet 序列化/解析、错误 payload、简单 APP payload 和 hex 工具实现。 |
| `src/shared/protocol/kerberos_messages.cpp` | AS/TGS/V_AUTH 的请求、响应、票据、认证器 codec 和相关加解密包装。 |
| `src/shared/protocol/certificate_messages.cpp` | Client/V 证书交换 payload codec。 |
| `src/shared/protocol/app_envelope.cpp` | `SignedAppPayload` 和 `AppAckPayload` codec，包含签名输入字节构造和验签。 |
| `src/shared/protocol/protocol_event.cpp` | 协议事件日志格式化、解析、JSON 输出和写入 `protocol_events/*.txt`。 |

### src/shared/runtime

| 文件 | 职责 |
| --- | --- |
| `src/shared/runtime/role_runtime.cpp` | 统一解析命令行并启动 AS/TGS/V/Client/Monitor 的运行时调度。 |
| `src/shared/runtime/runtime_paths.cpp` | 运行时路径、默认配置路径和日志根目录解析。 |

### src 目录说明文件

| 文件 | 职责 |
| --- | --- |
| `src/roles/README.md` | 总览角色源码布局和各角色 README 入口。 |
| `src/shared/README.md` | 总览共享源码布局和共享模块职责。 |

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
