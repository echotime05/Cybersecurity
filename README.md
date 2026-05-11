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

```powershell
.\build-mingw\as_server.exe --self-test
.\build-mingw\tgs_server.exe --self-test
.\build-mingw\v_server.exe --self-test
.\build-mingw\client.exe --self-test
```

完整 CTest 当前应通过：

```text
protocol_selftest
log_selftest
net_packet_selftest
connect_probe_selftest
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
.\build-mingw\v_server.exe --serve
```

四台主机都运行 Client 探测：

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

## 日志格式

所有日志行固定为：

```text
[实体][线程名][事件] 具体内容
```

AS/TGS/V 不做独立 UI，只写本机日志。后续 Qt 或 Web 可视化只读取同一套日志。
