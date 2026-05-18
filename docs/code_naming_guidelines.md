# 代码命名规则与审计

本文档用于统一当前 C++ 代码的命名风格，目标是让四个角色 AS、TGS、V、Client 在现场汇报和临时修改时都能快速定位职责。

## 1. 核心格式

普通函数、局部 helper、文件名使用：

```text
[domain]_[action]_[object]
```

含义：

- `domain`：函数属于哪条链路、哪一层或哪个角色。
- `action`：函数执行的动作，例如 build、parse、process、verify、log。
- `object`：函数处理的对象，例如 packet、payload、endpoint、state。

示例：

```cpp
as_process_packet()
tgs_process_packet()
v_auth_build_req_payload_view()
app_build_payload_view()
ack_log_verified_packet()
game_time_now_ms()
runtime_build_role_spec()
config_find_default_path()
```

## 2. 领域前缀

| 前缀 | 含义 |
| --- | --- |
| `as_` | Kerberos AS 阶段 |
| `tgs_` | Kerberos TGS 阶段 |
| `v_auth_` | Client -> V 服务认证阶段 |
| `cert_` | Client/V 证书交换 |
| `app_` | 游戏应用层安全封装，包含签名、加密、解密 |
| `ack_` | 双向不可否认证据 ACK |
| `game_` | 坦克大战应用层逻辑 |
| `packet_` | 通用固定首部 Packet |
| `protocol_` | Protocol Monitor 可视化结构 |
| `net_` | TCP/socket 收发 |
| `log_` | 普通日志和日志格式 |
| `runtime_` | 角色启动、运行模式、进程入口 |
| `ui_` | Client 本地 Web UI 桥接 |
| `monitor_` | Protocol Monitor |
| `crypto_` | DES、RSA、hash 等底层密码工具 |
| `config_` | 配置读取和配置派生值 |

不建议使用 `kerberos_1_`、`kerberos_2_` 这类数字阶段前缀。AS、TGS、V_AUTH 更贴近协议角色，也和报告中的流程一致。

## 3. 动作词

| 动作词 | 用法 |
| --- | --- |
| `build` | 根据结构体构造 bytes、packet、view 或配置对象 |
| `parse` | 从 bytes 或文本解析出结构体 |
| `process` | 处理一个完整请求并产生响应或状态变化 |
| `verify` | 验证签名、证书、ACK 或字段一致性 |
| `encrypt` / `decrypt` | 加密、解密 |
| `send` / `recv` | 网络发送、接收 |
| `log` | 写日志 |
| `format` | 转成人类可读字符串 |
| `find` | 查找配置或表项 |
| `run` | 启动主循环或角色服务 |
| `handle` | 事件驱动回调；只用于 UI、socket handler 等回调语义明显的地方 |

## 4. 保留规则

- 类型名保留 `PascalCase`：例如 `BattleRoom`、`TankGameClient`、`AuthRuntime`。
- enum class 类型名保留 `PascalCase`，枚举值保留 `snake_case`。
- public API 不在第一批重命名中调整，避免一次性影响全项目。
- 日志事件名继续使用大写，例如 `PACKET_SEND`、`AUTH_STATE`。
- TypeScript 前端暂时保留 `camelCase`，不和 C++ 命名强行统一。

## 5. 第一批审计

第一批只改 `.cpp` 内部 helper，不改 header 暴露函数。

| 文件 | 当前问题 | 第一批处理 |
| --- | --- | --- |
| `src/shared/auth/auth_flow.cpp` | AS/TGS/V_AUTH helper 混用 `handle_`、`ensure_`、`encrypted_` 等泛名 | 改为 `as_`、`tgs_`、`packet_`、`protocol_`、`auth_` 前缀 |
| `src/roles/v/tank_game_server.cpp` | V 内部 payload view helper 和游戏时间 helper 缺少领域前缀 | 改为 `app_`、`protocol_`、`v_auth_`、`game_` 前缀 |
| `src/roles/client/tank_game_client.cpp` | Client 内部 `app_payload_view` 与 V 同名但语义是应用层 view 构造 | 改为 `app_build_payload_view` |
| `src/shared/runtime/role_runtime.cpp` | 角色运行时 helper 命名偏泛，如 `spec_for`、`run_server` | 改为 `runtime_`、`config_`、`net_` 前缀 |

第二批已将 Kerberos、证书、应用层安全封装、ACK 和游戏协议的 public header API 按本文档规则统一。底层通用模块如 `packet`、`net_socket`、`crypto`、`logger` 暂不强制改名，后续如果继续整理，应按模块单独提交。
