# 终端验收优先实施计划

## 基本原则

1. 以《网络安全设计报告_日志设计修订版.docx》为唯一规格来源。
2. 当前阶段先不绑定 Qt 或 Web，可视化延后决策。
3. 所有阶段优先通过终端输出和日志文件验收。
4. 日志格式从一开始固定，后续 Qt/Web 只消费同一套日志。
5. 按四台主机、七个进程身份设计：AS、TGS、V、Client1、Client2、Client3、Client4。
6. AS/TGS/V 不做独立 UI，只写本机日志。Client 后续负责读取和展示本机相关日志。
7. 项目需要同步到 GitHub：`https://github.com/echotime05/Cybersecurity`。

## 四主机部署

| 主机 | 进程 | 日志 |
| --- | --- | --- |
| 主机 1 | AS + Client1 | `logs/as.log`、`logs/client_01.log` |
| 主机 2 | TGS + Client2 | `logs/tgs.log`、`logs/client_02.log` |
| 主机 3 | V + Client3 | `logs/v.log`、`logs/client_03.log` |
| 主机 4 | Client4 | `logs/client_04.log` |

服务端监听 `0.0.0.0`，Client 通过配置文件中的局域网 IP 连接 AS/TGS/V。

## 阶段计划

| 阶段 | 名称 | 内容 | 终端验收 |
| --- | --- | --- | --- |
| -1 | GitHub 同步准备 | 本地仓库绑定 `https://github.com/echotime05/Cybersecurity`；后续阶段按 commit 推送 | GitHub 能看到工程代码 |
| 0 | 修订版协议对齐 | 对齐 `EntityId`、`MsgType`、`ErrorCode`、`AppCode`、报文总览表、端口约定 | `protocol_selftest` 通过，常量自检覆盖修订版报文 |
| 1 | 四主机配置 | 配置 AS/TGS/V 的 LAN IP、端口、`LOCAL_CLIENT_ID`；服务端监听 `0.0.0.0` | `client --print-config` 能显示本机 Client 和 AS/TGS/V 地址 |
| 2 | 日志系统 | 实现线程安全 `Logger`；每进程只写自己的日志文件 | 运行各角色 `--self-test` 后生成对应日志文件 |
| 3 | 日志格式验收 | 固定 `[实体][线程名][事件] 内容`；事件包括 `THREAD_START/PACKET_SEND/PACKET_RECV/AUTH_STATE/GAME/ERROR` | `log_selftest` 能解析所有样例日志 |
| 4 | 报文层 | 实现 `PacketHeader`、`payload_len`、大端序、payload 构造/解析 | `protocol_selftest` 覆盖所有 `MsgType/AppCode/ErrorCode` |
| 5 | 统一收发接口 | 实现 `send_packet_logged()` / `recv_packet_logged()` | 本地 socket 测试能看到 `PACKET_SEND/PACKET_RECV` 日志 |
| 6 | 加密解密基础 | DES 分组加密、填充/解填充、hash、RSA、证书结构 | `crypto_selftest`：加密解密一致、签名验签通过 |
| 7 | AS 正常流程 | 实现 `MSG_AS_REQ/MSG_AS_REP`、`Kc_tgs`、`Ticket_tgs` | 启动 AS，Client 跑 AS 阶段，终端显示 `AUTH_STATE AS_OK` |
| 8 | TGS 正常流程 | 实现 `MSG_TGS_REQ/MSG_TGS_REP`、`Kc_v`、`Ticket_v` | Client 连 AS+TGS，终端显示 `AUTH_STATE TGS_OK` |
| 9 | V_AUTH 正常流程 | 实现 `MSG_V_AUTH_REQ/MSG_V_AUTH_REP` | Client 连 AS+TGS+V，终端显示 `AUTH_STATE V_AUTH_OK` |
| 10 | 证书交换 | 实现 `MSG_CERT_C2V/MSG_CERT_V2C`，C/V 互验证书 | Client 显示 `AUTH_STATE AUTH_DONE` |
| 11 | Kerberos 错误处理 | 密码错误、TGT 过期、Ticket_v 过期、身份不一致、重放、未知 msg_type | 每种错误能通过参数触发，并输出 `ERROR` + `MSG_ERROR` |
| 12 | Client 回退状态机 | 根据错误码回退对应阶段或终止 | 终端日志能看到“错误 -> 回退 -> 重新认证/退出” |
| 13 | 双向不可否认 | 应用层消息加入 `hash + signature`；实现 `APP_ACK` | 篡改消息/ACK 时验签失败并记录日志 |
| 14 | 应用层事件 | 实现 `KEY_DOWN/KEY_UP/AIM_EVENT/FIRE_EVENT/GAME_JOIN_REQ` | 终端输入命令能向 V 发送应用层事件 |
| 15 | V 权威世界状态 | V 每 50ms 消费事件队列，计算玩家、子弹、碰撞、生命值 | V 终端周期性输出 `GAME_STATE state_seq=...` |
| 16 | 四 Client 联机 | 四个 Client 完成认证后连接 V；V 等待 4 人后 `GAME_START` | V 日志显示 `joined_count=4`，四个 Client 收到 `GAME_START` |
| 17 | 终端演示脚本 | 写启动脚本、错误触发脚本、日志清理脚本 | 一组命令能完成正常流程和错误流程演示 |
| 18 | 可视化决策 | 根据最终需求选择 Qt 或 Web | 不影响前面代码，只消费已有日志和状态 |
| 19 | 可视化实现 | Qt Client UI 或 Web dashboard | 读取同一套日志，不改协议核心 |

## 当前优先执行范围

当前先执行到阶段 1：

1. 完成 GitHub 同步准备。
2. 对齐修订版协议常量和 AppCode。
3. 落地四主机 LAN 配置。
4. 增加终端 `--print-config` 验收能力。
5. 保持现有构建和 `protocol_selftest` 通过。

## 可视化延后约束

可视化虽然后置，但日志格式必须现在固定：

```text
[实体][线程名][事件] 具体内容
```

后续无论 Qt 还是 Web，都只读取日志和状态，不直接参与安全协议核心逻辑。
