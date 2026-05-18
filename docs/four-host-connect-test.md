# 四主机联调说明

本文档只保留最终加密链路的联调方式。历史上的轻量探测入口、旧认证测试入口、普通 `logs/*.log` 验收已经删除；当前验收以 `--game-auth-encrypted`、Web UI 和 `_generated/logs/protocol_events/*.txt` 为准。

## 前置条件

四台主机都使用同一份代码，并把 `config/course_config.txt` 中的 AS/TGS/V 地址改成一致的局域网地址：

```text
AS_IP=<AS 主机地址>
TGS_IP=<TGS 主机地址>
V_IP=<V 主机地址>
AS_BIND_IP=0.0.0.0
TGS_BIND_IP=0.0.0.0
V_BIND_IP=0.0.0.0
LOG_ROOT=_generated/logs
```

浏览器仍然只连接本机 `client.exe` 和本机 `monitor.exe`：

```text
Client bridge:   ws://127.0.0.1:7001
Protocol monitor: ws://127.0.0.1:7010
```

## 推荐分工

```text
Host 1: Client1
Host 2: AS + Client2
Host 3: TGS + Client3
Host 4: V + Client4
```

## 启动服务端

AS 主机：

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

TGS 主机：

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

V 主机：

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

## 启动每个玩家主机

每台玩家主机启动本机 Client、Monitor 和 Web UI：

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
.\_generated\build-mingw\monitor.exe --ui-port 7010 --events-dir .\_generated\logs\protocol_events
.\scripts\run_web.ps1
```

浏览器打开：

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

## 验收点

1. Web UI 能用对应 Client ID 和密码登录。
2. 登录后能加入游戏，并看到 V 广播的世界状态。
3. Protocol 面板能看到本机进程的 `MSG_AS_REQ`、`MSG_AS_REP`、`MSG_TGS_REQ`、`MSG_TGS_REP`、`MSG_V_AUTH_REQ`、`MSG_V_AUTH_REP`、`MSG_CERT_C2V`、`MSG_CERT_V2C` 和 `MSG_APP.*`。
4. 游戏 `MSG_APP` 报文能显示明文 payload hex 和加密 payload hex。
5. 非 ACK 游戏报文能看到对应的 `MSG_APP.APP_ACK`。

## 开发机自动验收

本机完整测试：

```powershell
ctest --test-dir _generated\build-mingw --output-on-failure
```

最终加密游戏链路专项测试：

```powershell
ctest --test-dir _generated\build-mingw -R auth_encrypted_game_flow_selftest --output-on-failure
```

四 Client 压测：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_perf_4clients.ps1 -DurationSeconds 60 -InputHz 10
```
