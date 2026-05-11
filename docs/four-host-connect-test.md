# 四主机连接骨架联调说明

本阶段只验证网络骨架和日志链路，不做真实加密和 Kerberos payload 校验。

## 前置条件

四台主机都使用同一份代码，并复制对应的 LAN 配置到 `config/course_config.txt`：

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

四台主机的 `AS_IP`、`TGS_IP`、`V_IP` 必须一致：

```text
AS_IP=172.27.197.122
TGS_IP=172.27.123.205
V_IP=172.27.39.248
```

## 启动服务端

主机 2：

```powershell
.\build-mingw\as_server.exe --serve
```

主机 3：

```powershell
.\build-mingw\tgs_server.exe --serve
```

主机 4：

```powershell
.\build-mingw\v_server.exe --serve
```

预期终端输出：

```text
AS listening on 0.0.0.0:9001
TGS listening on 0.0.0.0:9002
V listening on 0.0.0.0:9003
```

## 启动客户端探测

四台主机都运行：

```powershell
.\build-mingw\client.exe --connect-test
```

预期终端输出：

```text
connect-test: ok
log file: logs\client_0x.log
```

`--connect-test` 会依次执行四段占位连接：

1. Client -> AS：发送 `MSG_AS_REQ`，接收 `MSG_AS_REP`，写 `AUTH_STATE AS_OK`。
2. Client -> TGS：发送 `MSG_TGS_REQ`，接收 `MSG_TGS_REP`，写 `AUTH_STATE TGS_OK`。
3. Client -> V：发送 `MSG_V_AUTH_REQ`，接收 `MSG_V_AUTH_REP`，写 `AUTH_STATE V_AUTH_OK`。
4. Client -> V：发送 `MSG_CERT_C2V`，接收 `MSG_CERT_V2C`，写 `AUTH_STATE AUTH_DONE`。

## 日志验收

主机 2 的 `logs/as.log` 应出现：

```text
[AS][ASMainThread][ACCEPT] ...
[AS][ASWorker-Probe][PACKET_RECV] ... msg_type=MSG_AS_REQ ...
[AS][ASWorker-Probe][PACKET_SEND] ... msg_type=MSG_AS_REP ...
```

主机 3 的 `logs/tgs.log` 应出现：

```text
[TGS][TGSMainThread][ACCEPT] ...
[TGS][TGSWorker-Probe][PACKET_RECV] ... msg_type=MSG_TGS_REQ ...
[TGS][TGSWorker-Probe][PACKET_SEND] ... msg_type=MSG_TGS_REP ...
```

主机 4 的 `logs/v.log` 应出现：

```text
[V][VMainThread][ACCEPT] ...
[V][VWorker-Probe][PACKET_RECV] ... msg_type=MSG_V_AUTH_REQ ...
[V][VWorker-Probe][PACKET_SEND] ... msg_type=MSG_V_AUTH_REP ...
[V][VWorker-Probe][PACKET_RECV] ... msg_type=MSG_CERT_C2V ...
[V][VWorker-Probe][PACKET_SEND] ... msg_type=MSG_CERT_V2C ...
```

每台主机的 Client 日志应出现：

```text
[Client][CAuthWorker][CONNECT] connect to AS ...
[Client][CAuthWorker][AUTH_STATE] AS_OK
[Client][CAuthWorker][AUTH_STATE] TGS_OK
[Client][CAuthWorker][AUTH_STATE] V_AUTH_OK
[Client][CAuthWorker][AUTH_STATE] AUTH_DONE
```

## 本机自动验收

开发机可以直接运行：

```powershell
ctest --test-dir build-mingw --output-on-failure
```

其中 `connect_probe_selftest` 会自动启动 AS/TGS/V 的临时监听进程，并运行一次 `client --connect-test`。
