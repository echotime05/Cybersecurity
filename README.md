# Cybersecurity Course Design

## Phase A Plaintext Tank Battle

Phase A runs the tank battle application layer without Kerberos, DES, RSA
signatures, ACK non-repudiation, or replay protection. It keeps the existing
packet header and sends plaintext `MsgType::app` game messages between client
and V.

Start V on the V host:

```powershell
.\build-mingw\v_server.exe --game-plain
```

Start one client per player host:

```powershell
.\build-mingw\client.exe --game-plain --ui-port 7001
```

Start the Web UI:

```powershell
cd web-ui
npm install
npm run dev -- --host 127.0.0.1
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001
```

The browser connects only to the local C++ client bridge. The C++ client keeps
the TCP game connection to V and forwards world-state snapshots to the browser.

本工程用于实现课程设计报告中的多人联机坦克大战安全通信系统。

当前重点是终端验收：已跑通四主机配置、日志、报文、监听、真实 Kerberos 正常链路、证书交换，以及 `GAME_JOIN_REQ / APP_ACK` 双向不可否认闭环；坦克大战应用层同步和可视化后置。

## 当前四主机部署

| 主机 | 运行进程 | `LOCAL_CLIENT_ID` | 关键日志 |
| --- | --- | --- | --- |
| 主机 1 | Client1 only | `0x01` | `logs/client_01.log` |
| 主机 2 | AS + Client2 | `0x02` | `logs/as.log`、`logs/client_02.log` |
| 主机 3 | TGS + Client3 | `0x03` | `logs/tgs.log`、`logs/client_03.log` |
| 主机 4 | V + Client4 | `0x04` | `logs/v.log`、`logs/client_04.log` |

当前 LAN 地址：

```text
AS  = 172.27.197.122:9001
TGS = 172.27.123.205:9002
V   = 172.27.39.248:9003
```

## Host4 一键启动 V

在主机 4 的仓库根目录运行：

```powershell
.\run_v.bat
```

这个命令会自动完成：

- 复制 `config\lan\host4_v_client4.txt` 到 `config\course_config.txt`
- 停掉旧的 `v_server.exe`，避免 9003 端口被占用
- 如果能找到 `cmake.exe`，自动构建 `v_server`
- 前台启动 V 并输出监听状态

成功时终端会停在监听状态，并至少看到：

```text
V listening on 0.0.0.0:9003
log file: logs\v.log
```

停止 V 使用 `Ctrl+C`。

只检查配置和构建、不进入监听：

```powershell
.\run_v.bat -PrintOnly
```

## 构建

从仓库根目录运行。确保 `cmake`、`ninja` 和 C++ 编译器已经在 `PATH` 中。

```powershell
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

如果使用 Visual Studio：

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Debug
ctest --test-dir build-vs -C Debug --output-on-failure
```

## 代码目录

当前仓库按“4 个角色 + 公共模块”组织：

```text
src/roles/client/   Client 入口
src/roles/as/       AS 入口
src/roles/tgs/      TGS 入口
src/roles/v/        V 入口
src/common/         四个角色共享的协议、日志、网络、加密和认证流程
include/cyber/      公共头文件
config/             本机配置和四主机 LAN 配置模板
tests/              自测和本机集成测试
docs/               设计、计划和联调说明
scripts/            运行辅助脚本
useless/            本地废弃/临时产物说明，不参与构建
```

每个角色负责人优先看自己的 `src/roles/<role>/main.cpp`，共用逻辑再进入 `src/common`。

## 配置

默认 `config/course_config.txt` 用于当前机器运行。四主机联调时，在每台机器上复制对应模板：

```powershell
# 主机 1：Client1 only
Copy-Item .\config\lan\host1_client1.txt .\config\course_config.txt -Force

# 主机 2：AS + Client2
Copy-Item .\config\lan\host2_as_client2.txt .\config\course_config.txt -Force

# 主机 3：TGS + Client3
Copy-Item .\config\lan\host3_tgs_client3.txt .\config\course_config.txt -Force

# 主机 4：V + Client4
Copy-Item .\config\lan\host4_v_client4.txt .\config\course_config.txt -Force
```

查看当前配置：

```powershell
.\build-mingw\client.exe --print-config
```

## 本机自测

```powershell
.\build-mingw\protocol_selftest.exe
.\build-mingw\log_selftest.exe
.\build-mingw\net_packet_selftest.exe
.\build-mingw\crypto_selftest.exe
.\build-mingw\auth_payload_selftest.exe
ctest --test-dir build-mingw --output-on-failure
```

## 四主机连接骨架联调

主机 2 启动 AS：

```powershell
.\build-mingw\as_server.exe --serve
```

主机 3 启动 TGS：

```powershell
.\build-mingw\tgs_server.exe --serve
```

主机 4 启动 V：

```powershell
.\run_v.bat
```

四台主机都可以运行 Client 探测：

```powershell
.\build-mingw\client.exe --connect-test
```

成功时 Client 终端输出：

```text
connect-test: ok
```

Client 日志应出现：

```text
[Client][CAuthWorker][AUTH_STATE] AS_OK
[Client][CAuthWorker][AUTH_STATE] TGS_OK
[Client][CAuthWorker][AUTH_STATE] V_AUTH_OK
[Client][CAuthWorker][AUTH_STATE] AUTH_DONE
```

更多四主机联调细节见 `docs/four-host-connect-test.md`。

## 真实认证与双向不可否认验收

启动 AS/TGS/V 后，任一 Client 主机运行：

```powershell
.\build-mingw\client.exe --auth-test
```

成功输出：

```text
AUTH_STATE AS_OK
AUTH_STATE TGS_OK
AUTH_STATE V_AUTH_OK
AUTH_STATE AUTH_DONE
APP_NON_REPUDIATION GAME_JOIN_REQ_SIGNED
APP_NON_REPUDIATION APP_ACK_VERIFIED
auth-test: ok
```

`--auth-test` 执行真实正常流程：

```text
AS_REQ / AS_REP
TGS_REQ / TGS_REP
V_AUTH_REQ / V_AUTH_REP
CERT_C2V / CERT_V2C
GAME_JOIN_REQ + signed APP_ACK
```

## 日志格式

所有日志行固定为：

```text
[实体][线程名][事件] 具体内容
```

当前自测覆盖：

- `log_selftest` 验证日志写入和结构化解析。
- `net_packet_selftest` 验证本机 TCP 收发和 `PACKET_SEND` / `PACKET_RECV` 日志。
- `crypto_selftest` 验证 DES 风格分组加密、hash、RSA 签名验签和证书验签。
- `auth_payload_selftest` 验证 Kerberos、证书和 ACK payload 的 build/parse。
- `connect_probe_selftest` 自动启动 AS/TGS/V 监听骨架，并让 Client 依次完成 `AS_REQ/AS_REP`、`TGS_REQ/TGS_REP`、`V_AUTH_REQ/V_AUTH_REP`、`CERT_C2V/CERT_V2C`。
- `auth_flow_selftest` 自动启动 AS/TGS/V，并让 Client 完成 `--auth-test`。

## 当前阶段边界

已经完成：

- 四主机 LAN 配置模板
- AS/TGS/V 监听骨架
- Client 连接探测骨架
- 固定格式日志和日志解析
- 报文序列化、反序列化和基础校验
- 加密解密基础
- 完整 Kerberos 正常票据流程
- C/V 证书交换
- `GAME_JOIN_REQ / APP_ACK` 双向不可否认正常闭环

尚未完成：

- Kerberos 错误处理和回退状态机
- 篡改报文/ACK 的错误演示
- 坦克大战应用层状态同步
- Qt/Web 可视化

## 低延迟修改记录

为了给后续坦克大战实时同步降低抖动，当前已完成两项低延迟基础修改：

1. 异步批量日志写入

   原来的 `Logger::write()` 每写一条日志都会立即 `flush()` 到磁盘。网络收发线程在记录 `PACKET_SEND`、`PACKET_RECV`、应用层事件和 ACK 时，会被磁盘 I/O 阻塞。现在 `Logger::write()` 只负责格式化日志并放入内存队列，后台日志线程每 20ms 批量写入文件；当队列达到 64 条时会提前唤醒写线程。`Logger::flush()` 和 `Logger` 析构会等待队列落盘，保证认证验收、测试结束和程序退出时日志完整。

2. 小包连接启用 `TCP_NODELAY`

   坦克大战阶段会频繁发送 `KEY_DOWN`、`KEY_UP`、`AIM_EVENT`、`FIRE_EVENT`、`APP_ACK` 等小包。为了减少 Nagle 算法合并小包造成的额外等待，`connect_tcp()` 和 `accept_tcp()` 现在会在连接建立后自动调用 `set_tcp_nodelay()`，对连接两端开启 `TCP_NODELAY`。这样后续 Client 与 V 的游戏长连接天然使用低延迟小包发送策略。

相关自测：

- `log_selftest` 验证异步日志在 `flush()` 和析构后能完整落盘。
- `net_packet_selftest` 验证 `connect_tcp()` 和 `accept_tcp()` 返回的 socket 已启用 `TCP_NODELAY`，并继续验证 `PACKET_SEND` / `PACKET_RECV` 日志。
