# Cybersecurity Course Design

本工程用于实现课程设计报告中的多人联机坦克大战安全通信系统。

当前代码先固定工程结构和公共协议层，后续按 `docs/implementation-plan.md` 分阶段补齐 Kerberos、证书交换、不可否认和应用层同步。

## 目标角色

- `as_server`: 认证服务器，处理 `AS_REQ/AS_REP`。
- `tgs_server`: 票据授予服务器，处理 `TGS_REQ/TGS_REP`。
- `v_server`: 游戏服务器，处理 `V_AUTH`、证书交换和 `MSG_APP`。
- `client`: 客户端，完成登录认证、证书交换、输入上报和渲染。

## 本机已验证的构建方式

你的机器上已检测到 Qt 自带的 CMake、Ninja 和 MinGW：

- `E:\Qt\Tools\CMake_64\bin\cmake.exe`
- `E:\Qt\Tools\Ninja\ninja.exe`
- `E:\Qt\Tools\mingw1120_64\bin\g++.exe`

在 `E:\zhuomian\cybersecurity\code` 下执行：

```powershell
$env:PATH = 'E:\Qt\Tools\mingw1120_64\bin;E:\Qt\Tools\Ninja;' + $env:PATH
& 'E:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-mingw -G Ninja -DCMAKE_CXX_COMPILER='E:\Qt\Tools\mingw1120_64\bin\g++.exe' -DCMAKE_MAKE_PROGRAM='E:\Qt\Tools\Ninja\ninja.exe'
& 'E:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-mingw
& 'E:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-mingw --output-on-failure
```

## 通用构建方式

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

普通 PowerShell 的 `PATH` 中不一定有 `cmake`、`g++` 或 `cl`。如果使用 Visual Studio 编译，建议从 “Developer PowerShell for VS 2022” 打开本目录，或显式调用 VS/Qt 工具路径。

## Visual Studio 2022

已生成并验证 VS 解决方案：

```text
E:\zhuomian\cybersecurity\code\build-vs\cyber_tank_design.sln
```

也可以重新生成：

```powershell
& 'E:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-vs -G 'Visual Studio 17 2022' -A x64
& 'E:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-vs --config Debug
& 'E:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-vs -C Debug --output-on-failure
```

## 当前可运行目标

编译完成后，每个角色都支持自检：

```powershell
.\build-mingw\as_server.exe --self-test
.\build-mingw\tgs_server.exe --self-test
.\build-mingw\v_server.exe --self-test
.\build-mingw\client.exe --self-test
```

公共协议层自测：

```powershell
.\build-mingw\protocol_selftest.exe
```

日志系统自测：

```powershell
.\build-mingw\log_selftest.exe
```

本地 socket 收发日志自测：

```powershell
.\build-mingw\net_packet_selftest.exe
```

四主机连接骨架自测：

```powershell
.\build-mingw\as_server.exe --serve
.\build-mingw\tgs_server.exe --serve
.\build-mingw\v_server.exe --serve
.\build-mingw\client.exe --connect-test
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

`log_selftest` 会同时验证日志写入和结构化解析，解析结果包含 `entity`、`thread_name`、`event`、`message` 四个字段。

`net_packet_selftest` 会在本机回环地址上建立一条临时 TCP 连接，并通过
`send_packet_logged()` / `recv_packet_logged()` 交换 `KEY_DOWN` 和 `APP_ACK`，
验证收发包日志自动输出 `PACKET_SEND` / `PACKET_RECV`。

`connect_probe_selftest` 会自动启动 AS/TGS/V 的监听骨架，再让 Client 依次完成
`AS_REQ/AS_REP`、`TGS_REQ/TGS_REP`、`V_AUTH_REQ/V_AUTH_REP`、`CERT_C2V/CERT_V2C`
四段占位连接。四台物理机联调步骤见 `docs/four-host-connect-test.md`。

## 四主机配置验收

默认 `config/course_config.txt` 使用 `127.0.0.1`，便于本机自测。局域网部署时可参考
`config/course_config.lan.example.txt`，在四台物理机上分别设置 `LOCAL_CLIENT_ID`
以及 `AS_IP`、`TGS_IP`、`V_IP`。

查看当前角色和网络配置：

```powershell
.\build-mingw\client.exe --print-config
.\build-mingw\as_server.exe --print-config
.\build-mingw\tgs_server.exe --print-config
.\build-mingw\v_server.exe --print-config
```
