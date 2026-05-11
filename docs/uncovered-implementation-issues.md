# 实现时发现的关键缺口

本文只记录“按当前报告直接写代码时，可能导致程序无法稳定跑通”的问题。不讨论真实安全强度、抗攻击能力、RSA/DES 是否足够安全等课设范围外内容。

## 1. 报文没有长度字段，TCP 收包边界无法确定

早期报告中定义的通用首部是：

```text
msg_type(1B) + src_ID(1B) + dst_ID(1B) + reserved(4B) + payload
```

但 `payload` 是不定长的。TCP 是字节流，不保留一次 `send` 对应一次 `recv` 的边界，所以接收方不知道当前包什么时候收完。

如果不补这个字段，后续所有 Kerberos 报文、错误报文、应用层报文都可能出现粘包/半包问题。

修订版已补充为：

```text
msg_type(1B) + src_ID(1B) + dst_ID(1B) + payload_len(4B) + reserved(4B) + payload
```

也就是保留 `reserved(4B)`，并在其前面增加 `payload_len(4B)`。

验收标准：

- 接收端先固定读取 7B 首部。
- 从首部读出 `payload_len`。
- 再循环读取指定长度的 payload。
- 连续发送两个包时，接收端能正确拆成两个 Packet。

## 2. Payload 的二进制格式没有完全落定

报告里写了很多结构，例如 `AS_REP`、`Ticket_tgs`、`Ticket_v`、`Auth`、`GAME_STATE`，但大部分没有明确每个字段的字节长度、顺序、整数端序、float 表示方式。

这会导致 Client、AS、TGS、V 各自“理解”同一个 payload 的方式不一致。

建议补充：

- 所有整数统一使用大端序。
- `ID` 使用 `uint8_t`。
- 时间戳使用 `uint64_t`，单位毫秒。
- DES key 使用 56 bit，但传输时用 8B 容器，最高 1B 置 0 或忽略。
- `float` 暂时使用 IEEE754 4B，直接按字节传输。
- 字符串使用 `uint16 length + bytes`。

验收标准：

- 每种 payload 都有 `build_xxx_payload` 和 `parse_xxx_payload`。
- 同一个 payload 经 `build -> parse` 后字段完全一致。
- 四个角色不能手写临时字符串拼接来解析协议字段。

## 3. DES 的 0 填充无法恢复原始 payload 长度

报告中写到 DES 分组加密时，如果 payload 不是 8B 整数倍，就在末尾填充 `0x0K`。这里表达不够明确。

如果只是填充 0，那么解密后无法区分“原始数据末尾本来就是 0”和“为了补齐块长填的 0”。这会影响二进制 payload，尤其是 `GAME_STATE`、签名、证书等字段。

建议采用简单明确的 PKCS#7 风格填充：

```text
缺 n 个字节，就填充 n 个值为 n 的字节
```

例如缺 3B，就填：

```text
03 03 03
```

如果刚好是 8B 整数倍，也额外填充 8 个 `08`。

验收标准：

- 任意长度 payload 经 `des_encrypt -> des_decrypt` 后和原始 payload 完全一致。
- 原始 payload 末尾含 `0x00` 时也能正确恢复。

## 4. Kerberos 阶段的连接生命周期和 V 应用连接没有完全说明

报告里说明 Kerberos 四个阶段每阶段完成后都断开连接，尤其第三阶段和第四阶段也会断开。但应用层游戏通信需要 Client 和 V 保持一个长期 socket，用于输入上报和 `GAME_STATE` 广播。

这里需要明确：证书交换完成后，Client 是否重新连接 V 进入应用层，还是证书交换使用的连接转为应用层连接。

建议采用：

- AS 阶段：短连接。
- TGS 阶段：短连接。
- V_AUTH 阶段：短连接。
- CERT 阶段：短连接。
- APP 阶段：Client 重新连接 V，发送第一个 `MSG_APP` 前必须携带或绑定已认证的 `client_id`。

验收标准：

- V 能区分某个 socket 处于 `unauthenticated`、`v_auth_done`、`cert_done`、`app_ready` 哪个状态。
- 未完成认证的 Client 发送 `MSG_APP` 时，V 拒绝处理。
- APP 阶段连接断开后，V 能清理该玩家 socket。

## 5. Client 多线程模型还不够细

报告里对 V 的线程模型写得比较清楚，但 Client 的线程边界还不够。Client 同时要做这些事：

- 认证阶段顺序执行 Kerberos。
- 进入游戏后采集键盘鼠标输入。
- 周期性发送 `AIM_EVENT`。
- 接收 V 广播的 `GAME_STATE`。
- 渲染画面。
- 对需要 ACK 的报文回 ACK。

如果这些逻辑都放在一个线程里，游戏循环和网络收包会互相阻塞。

建议 Client 至少拆成：

- 主线程：启动、登录、认证、状态切换。
- 输入/渲染线程：采集输入，渲染最新世界状态。
- 网络接收线程：阻塞接收 V 下发报文，更新共享世界状态，必要时回 ACK。
- 定时发送逻辑：每 50ms 发送 `AIM_EVENT`，可以放在输入/渲染线程中按时间触发。

共享数据至少包括：

- 当前世界状态。
- socket 写锁。
- Client 当前阶段状态。
- 待发送输入事件队列。

验收标准：

- 网络收包阻塞时，界面或控制台渲染不会卡死。
- 渲染线程读取世界状态时不会和网络线程写入冲突。
- 多个线程不会同时向同一个 socket 写数据。

## 6. V 的玩家注册表和 socket 管理需要单独设计

报告提到 V 为每个 Client 创建通信线程，并保存 socket，但没有具体说明如何维护 “Client ID 到 socket/状态” 的映射。

这个映射是游戏广播的核心。如果没有它，V 游戏计算线程不知道该把 `GAME_STATE` 发给谁，也不知道某个输入事件属于哪个已认证玩家。

建议 V 维护：

```text
ClientSession {
    client_id
    socket
    auth_state
    write_mutex
    last_seen_time
    public_key
}
```

并用全局 `ClientRegistry` 管理 C1 到 C4。

验收标准：

- 每个 Client 完成 APP 阶段连接后能注册到唯一槽位。
- 同一个 `client_id` 重复连接时，V 有明确处理策略，例如拒绝新连接或替换旧连接。
- 游戏线程广播 `GAME_STATE` 时只发给 `app_ready` 的 Client。

## 7. ACK 和响应分离后，需要定义哪些报文必须 ACK

报告中说 ACK 和响应分开，这是合理的，但没有列出哪些报文需要 ACK、ACK 的 payload 长什么样、ACK 是否属于 `MSG_APP`。

如果不定义，Client 和 V 可能出现一边等待 ACK，另一边根本不会发 ACK 的死锁。

建议先简化：

- Kerberos 阶段不单独 ACK，只按请求/响应模型处理。
- 应用层上行事件 `KEY_DOWN`、`KEY_UP`、`AIM_EVENT`、`FIRE_EVENT` 需要 V 回 ACK。
- `GAME_STATE` 初期可以不要求 Client 回 ACK，避免广播被慢客户端拖住。
- ACK 使用 `MSG_APP`，修订版约定 `APP_ACK = 0x08`。

验收标准：

- Client 发送上行事件后，能收到对应 ACK。
- V 不等待 `GAME_STATE` 的 ACK。
- ACK 中至少包含 `app_code` 和一个本地递增 `seq`，方便对应请求。

## 8. 错误恢复策略有一处冲突

报告中对“消息重放”的处理有两种说法：

- 一处写到认为 Client 是潜在异常用户，要求终结 Client 进程。
- 后面状态机说明里又写“对于消息重放错误：只需要重新执行该阶段的 kerberos 认证即可”。

这两个策略会直接影响 Client 状态机实现，需要选一个。

建议按更简单、更容易演示的方式处理：

- `ERR_REPLAY_DETECTED`：Client 打印错误并退出。

验收标准：

- 重放测试时，服务端返回 `ERR_REPLAY_DETECTED`。
- Client 收到后不再重试，直接结束。

## 9. 时间戳和票据生命周期单位需要统一

报告中有 `TS2`、`TS4`、`LifeTime2`、`LifeTime4`，但没有明确单位和比较方式。

如果一个模块用秒，另一个模块用毫秒，会导致票据立即过期或长期不过期。

建议统一：

- 时间戳使用 `uint64_t`。
- 单位使用 Unix epoch milliseconds。
- `now_ms > ticket_ts_ms + lifetime_ms` 判定为过期。
- 课设演示中可设置 `lifetime_ms = 5 * 60 * 1000`。

验收标准：

- 正常认证过程中票据不会误过期。
- 人为把 lifetime 设成 1ms 后，能稳定触发过期错误。

## 10. 游戏开始条件需要明确

报告默认是 4 人坦克大战，但没有说明 V 是否必须等待四个 Client 全部完成认证后才开始，或者少于四人是否允许开始。

这会影响 `GAME_START` 什么时候广播，也影响出生点和玩家编号分配。

建议课设演示采用固定规则：

- V 等待 C1 到 C4 都进入 `app_ready`。
- 全部就绪后，V 广播 `GAME_START`。
- 收到 `GAME_START` 后，Client 才开始发送游戏输入。

验收标准：

- 只有 1 到 3 个 Client 连接时，V 不开始游戏，只显示等待状态。
- 第 4 个 Client 就绪后，四个 Client 都收到同一份 `GAME_START`。
- `GAME_STATE` 广播在 `GAME_START` 后开始。

## 11. 日志格式需要提前约定

这个不是功能协议问题，但会影响最后演示和调试。当前报告没有规定日志输出，真正调试四进程时，如果日志不统一，很难判断错误发生在哪个阶段。

建议每个角色统一输出：

```text
[role][client_id][stage] message
```

例如：

```text
[AS][C1][AS_REQ] received
[AS][C1][AS_REP] sent Kc_tgs and Ticket_tgs
[TGS][C1][ERROR] ERR_TGT_EXPIRED
[V][C1][APP] KEY_DOWN W
```

验收标准：

- 正常完整认证能从日志中看到 AS、TGS、V、CERT、APP 五段。
- 错误演示能从日志中直接看到错误码和回退/退出行为。
