# Cybersecurity Course Design

本工程用于实现课程设计报告中的多人联机坦克大战安全通信系统。

当前代码先固定工程结构和公共协议层，后续按 `docs/implementation-plan.md` 分阶段补齐 Kerberos、证书交换、不可否认和应用层同步。

## 目标角色

- `as_server`: 认证服务器，处理 `AS_REQ/AS_REP`。
- `tgs_server`: 票据授予服务器，处理 `TGS_REQ/TGS_REP`。
- `v_server`: 游戏服务器，处理 `V_AUTH`、证书交换和 `MSG_APP`。
- `client`: 客户端，完成登录认证、证书交换、输入上报和渲染。

## 编译环境要求

- 支持 C++17 的编译器（如 GCC, Clang, MSVC）
- CMake (版本 >= 3.16)

## 编译教程

本项目使用 CMake 进行构建，支持多平台和多种构建系统。以下提供通用的编译和测试步骤。

### 通用构建命令

在项目根目录下执行以下命令进行编译：

```bash
# 生成构建文件（默认使用系统的默认生成器）
cmake -S . -B build

# 编译项目
cmake --build build

# 运行所有测试验证编译结果
ctest --test-dir build --output-on-failure
```

### Windows 下使用 Visual Studio 2022 编译

如果你使用 Visual Studio 2022，推荐在“Developer PowerShell for VS 2022”或“Developer Command Prompt for VS 2022”中执行以下命令：

```powershell
# 使用 MSVC 生成构建文件，指定 64 位架构
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64

# 编译项目（以 Debug 模式为例）
cmake --build build-vs --config Debug

# 运行所有测试
ctest --test-dir build-vs -C Debug --output-on-failure
```

*注意：编译完成后，生成的可执行文件将位于 `build/` 或 `build-vs/Debug/` 目录下。接下来的运行示例以 `build/` 为例。*

## 当前可运行目标

编译完成后，每个角色都支持自检：

```bash
./build/as_server --self-test
./build/tgs_server --self-test
./build/v_server --self-test
./build/client --self-test
```

公共协议层自测：

```bash
./build/protocol_selftest
```

日志系统自测：

```bash
./build/log_selftest
```

本地 socket 收发日志自测：

```bash
./build/net_packet_selftest
```

四主机连接骨架自测：

```bash
./build/as_server --serve
./build/tgs_server --serve
./build/v_server --serve
./build/client --connect-test
```

当前验证结果：`protocol_selftest`、`log_selftest`、`net_packet_selftest`、`connect_probe_selftest` 通过，四个角色的 `--self-test` 均通过。

角色自检会写入本机日志文件：

```text
logs/as.log
logs/tgs.log
logs/v.log
logs/client_01.log
```

日志格式固定为：

```text
[实体][线程名][事件] 具体内容
```

- `log_selftest` 会同时验证日志写入和结构化解析，解析结果包含 `entity`、`thread_name`、`event`、`message` 四个字段。
- `net_packet_selftest` 会在本机回环地址上建立一条临时 TCP 连接，并通过 `send_packet_logged()` / `recv_packet_logged()` 交换 `KEY_DOWN` 和 `APP_ACK`，验证收发包日志自动输出 `PACKET_SEND` / `PACKET_RECV`。
- `connect_probe_selftest` 会自动启动 AS/TGS/V 的监听骨架，再让 Client 依次完成 `AS_REQ/AS_REP`、`TGS_REQ/TGS_REP`、`V_AUTH_REQ/V_AUTH_REP`、`CERT_C2V/CERT_V2C` 四段占位连接。四台物理机联调步骤见 `docs/four-host-connect-test.md`。

## 四主机配置验收

默认 `config/course_config.txt` 使用 `127.0.0.1`，便于本机自测。局域网部署时可参考 `config/course_config.lan.example.txt`，在四台物理机上分别设置 `LOCAL_CLIENT_ID` 以及 `AS_IP`、`TGS_IP`、`V_IP`。

查看当前角色和网络配置：

```bash
./build/client --print-config
./build/as_server --print-config
./build/tgs_server --print-config
./build/v_server --print-config
```
