# 现场修改指南

本文档用于老师现场要求改功能时快速定位代码。原则是：先判断改动属于哪个角色；如果只影响单个角色，从 `src/roles/<role>` 开始；如果影响报文格式、加密、签名、日志或 Web UI，再进入共享目录。

## 角色入口

| 角色 | 主要目录 | 汇报说明 |
| --- | --- | --- |
| AS | `src/roles/as/` | `src/roles/as/README.md` |
| TGS | `src/roles/tgs/` | `src/roles/tgs/README.md` |
| V | `src/roles/v/` | `src/roles/v/README.md` |
| Client | `src/roles/client/` | `src/roles/client/README.md` |
| Monitor | `src/roles/monitor/` | `src/roles/monitor/README.md` |

## AS

| 修改目标 | 入口文件 |
| --- | --- |
| 修改 AS 收包、校验、发包流程 | `src/roles/as/as_service.cpp` |
| 修改 AS 对外函数声明 | `include/cyber/roles/as/as_service.hpp` |
| 修改 Client 密码、长期密钥派生 | `src/shared/auth/auth_credentials.cpp` |
| 修改 AS_REP/Ticket_tgs 字段 | `src/shared/protocol/kerberos_messages.cpp` |

AS 只处理 `MSG_AS_REQ -> MSG_AS_REP`。它不维护游戏状态，也不直接连接 Web UI。

## TGS

| 修改目标 | 入口文件 |
| --- | --- |
| 修改 TGS 收包、票据校验、发包流程 | `src/roles/tgs/tgs_service.cpp` |
| 修改 TGS 对外函数声明 | `include/cyber/roles/tgs/tgs_service.hpp` |
| 修改 Ticket_tgs/Ticket_v/Authenticator 字段 | `src/shared/protocol/kerberos_messages.cpp` |
| 修改加密、解密、时间戳相关基础能力 | `src/shared/crypto/crypto.cpp` |

TGS 只处理 `MSG_TGS_REQ -> MSG_TGS_REP`。如果老师要求“票据多加字段”，优先看协议 payload 文件和 TGS service。

## V

| 修改目标 | 入口文件 |
| --- | --- |
| 修改 V_AUTH 和证书交换 | `src/roles/v/v_auth_service.cpp` |
| 修改 V server 连接、认证后收包、ACK、广播 | `src/roles/v/tank_game_server.cpp` |
| 修改游戏房间规则、计分、reload、补给 | `src/roles/v/battle_room.cpp` |
| 修改地图、子弹移动、碰撞、射程 | `src/roles/v/game_world.cpp` |
| 修改 V 角色头文件 | `include/cyber/roles/v/tank_game_server.hpp` |

V 是权威游戏服务器。最终位置、生命值、子弹、命中和分数都以 V 广播的 `GAME_STATE` 为准。

## Client

| 修改目标 | 入口文件 |
| --- | --- |
| 修改 AS/TGS/V_AUTH/CERT 客户端流程 | `src/roles/client/client_auth_flow.cpp` |
| 修改登录状态、加入游戏、发送游戏报文 | `src/roles/client/tank_game_client.cpp` |
| 修改浏览器 WebSocket JSON 桥接 | `src/roles/client/ui_bridge.cpp` |
| 修改 WebSocket 协议细节 | `src/roles/client/websocket.cpp` |
| 修改 Client 角色头文件 | `include/cyber/roles/client/tank_game_client.hpp` |

浏览器不直接连接 AS/TGS/V。所有浏览器命令先到本机 `client.exe`，再由 Client 生成 C++ TCP 协议报文。

## Web UI 与 Monitor

| 修改目标 | 入口文件 |
| --- | --- |
| 修改登录、游戏画面、输入事件 | `web-ui/src/Game.ts` |
| 修改 WebSocket 客户端 | `web-ui/src/Network.ts` |
| 修改 Protocol 面板布局 | `web-ui/src/ProtocolMonitor.ts` |
| 修改 payload 结构化解析 | `web-ui/src/protocolPayload.ts` |
| 修改 monitor 读取日志和推送事件 | `src/roles/monitor/monitor_server.cpp` |

Protocol 面板只展示 `monitor.exe` 从 `_generated/logs/protocol_events/*.txt` 读到的本机报文事件。

## 共享协议与安全代码

| 修改目标 | 入口文件 |
| --- | --- |
| 修改固定 11B Packet header | `src/shared/protocol/packet.cpp` |
| 修改 Kerberos 字段 | `src/shared/protocol/kerberos_messages.cpp` |
| 修改证书交换字段 | `src/shared/protocol/certificate_messages.cpp` |
| 修改 APP_ACK/SignedAppPayload 字段 | `src/shared/protocol/app_envelope.cpp` |
| 修改游戏 GameMessage 和快照字段 | `src/shared/game/game_protocol.cpp` |
| 修改应用层加密、明文/密文日志视图 | `src/shared/game/app_payload_codec.cpp` |
| 修改双向不可否认 ACK | `src/shared/game/game_non_repudiation.cpp` |
| 修改协议事件日志字段 | `src/shared/protocol/protocol_event.cpp` |

如果改了 wire 字段，通常要同步修改 C++ codec、Web UI 的 `protocolPayload.ts` 和文档中的字段表。

## 常用验证命令

```powershell
$env:Path = 'E:\Qt\Tools\CMake_64\bin;E:\Qt\Tools\Ninja;E:\Qt\Tools\mingw1120_64\bin;' + $env:Path
cmake --build _generated\build-mingw
ctest --test-dir _generated\build-mingw --output-on-failure
```

Web UI 修改后：

```powershell
cd web-ui
npm run verify
cd ..
```
