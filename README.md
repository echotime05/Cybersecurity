# Cybersecurity Course Design

本工程用于实现课程设计报告中的多人联机坦克大战安全通信系统。

当前重点是终端验收：先跑通四主机配置、日志、报文、监听和连接骨架；加密、真实 Kerberos payload、应用层同步和可视化后置。

## 当前四主机部署

| 主机 | 运行进程 | `LOCAL_CLIENT_ID` | 关键日志 |
| --- | --- | --- | --- |
| 主机 1 | Client1 only | `0x01` | `logs/client_01.log` |
| 主机 2 | AS + Client2 | `0x02` | `logs/as.log`、`logs/client_02.log` |
| 主机 3 | TGS + Client3 | `0x03` | `logs/tgs.log`、`logs/client_03.log` |
| 主机 4 | V + Client4 | `0x04` | `logs/v.log`、`logs/client_04.log` |

<<<<<<< HEAD
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
=======
当前 LAN 地址：

```text
AS  = 172.27.197.122:9001
TGS = 172.27.123.205:9002
V   = 172.27.39.248:9003
```

## 构建

从仓库根目录运行。确保 `cmake`、`ninja` 和 C++ 编译器已经在 `PATH` 中。

```powershell
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

如果使用 Visual Studio：
>>>>>>> 25a936be254debbc3798bdaf109bf9a72b958635

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Debug
ctest --test-dir build-vs -C Debug --output-on-failure
```

## 配置

默认 `config/course_config.txt` 使用 `127.0.0.1`，用于本机自测。

四主机联调时，在每台机器上复制对应模板：

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
```

角色自检：

```bash
./build/as_server --self-test
./build/tgs_server --self-test
./build/v_server --self-test
./build/client --self-test
```

完整 CTest 当前应通过：

<<<<<<< HEAD
```bash
./build/protocol_selftest
=======
```text
protocol_selftest
log_selftest
net_packet_selftest
connect_probe_selftest
>>>>>>> 25a936be254debbc3798bdaf109bf9a72b958635
```

## 四主机连接骨架联调

<<<<<<< HEAD
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
=======
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
.\build-mingw\v_server.exe --serve
```

四台主机都运行 Client 探测：

```powershell
.\build-mingw\client.exe --connect-test
>>>>>>> 25a936be254debbc3798bdaf109bf9a72b958635
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

## 日志格式

所有日志行固定为：

```text
[实体][线程名][事件] 具体内容
```

<<<<<<< HEAD
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
=======
AS/TGS/V 不做独立 UI，只写本机日志。后续 Qt 或 Web 可视化只读取同一套日志。
>>>>>>> 25a936be254debbc3798bdaf109bf9a72b958635
