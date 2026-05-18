# Protocol Reference

本文档是当前最终链路的报文字段参考。代码事实来源为 `include/cyber/protocol/`、`src/shared/protocol/`、`include/cyber/game/game_protocol.hpp` 和 `src/shared/game/game_protocol.cpp`。

## 编码规则

| 类型 | 字节数 | 编码 |
| --- | --- | --- |
| `u8` | 1 | 无符号 8 位整数 |
| `i8` | 1 | 有符号 8 位整数，按原始字节传输 |
| `u16` | 2 | 大端序 |
| `u32` | 4 | 大端序 |
| `u64` | 8 | 大端序 |
| `f32` | 4 | IEEE-754 float32 bit pattern，再按 `u32_be` 传输 |
| `bool_u8` | 1 | `0=false`，非 0 为 true；当前构造写 `0` 或 `1` |
| `bytes_u16` | variable | `u16 length` + 原始字节 |

所有 Packet 的固定首部保持明文。加密只作用于 payload 或 payload 内部的某些字段。

## 固定 Packet Header

| Offset | Size | Field | Type | Description |
| --- | --- | --- | --- | --- |
| 0 | 1 | `msg_type` | `u8` | `MSG_AS_REQ`、`MSG_AS_REP`、`MSG_TGS_REQ`、`MSG_TGS_REP`、`MSG_V_AUTH_REQ`、`MSG_V_AUTH_REP`、`MSG_CERT_C2V`、`MSG_CERT_V2C`、`MSG_ERROR`、`MSG_APP` |
| 1 | 1 | `src` | `u8` | 逻辑发送方 ID |
| 2 | 1 | `dst` | `u8` | 逻辑接收方 ID |
| 3 | 4 | `payload_len` | `u32_be` | payload 字节数 |
| 7 | 4 | `reserved` | `u32_be` | 当前固定为 0 |
| 11 | variable | `payload` | bytes | 由 `msg_type` 决定结构 |

## EntityId

| Name | Value |
| --- | --- |
| `Client1` | `0x01` |
| `Client2` | `0x02` |
| `Client3` | `0x03` |
| `Client4` | `0x04` |
| `AS` | `0x11` |
| `TGS` | `0x12` |
| `V` | `0x13` |
| `Unknown` | `0xFF` |

## MsgType

| MsgType | Value | Direction | Payload encryption |
| --- | --- | --- | --- |
| `MSG_AS_REQ` | `0x01` | Client -> AS | plain |
| `MSG_AS_REP` | `0x02` | AS -> Client | whole payload encrypted with `Kc` |
| `MSG_TGS_REQ` | `0x03` | Client -> TGS | outer payload plain, nested ticket/authenticator encrypted |
| `MSG_TGS_REP` | `0x04` | TGS -> Client | whole payload encrypted with `Kc_tgs` |
| `MSG_V_AUTH_REQ` | `0x05` | Client -> V | outer payload plain, nested ticket/authenticator encrypted |
| `MSG_V_AUTH_REP` | `0x06` | V -> Client | whole payload encrypted with `Kc_v` |
| `MSG_CERT_C2V` | `0x07` | Client -> V | whole payload encrypted with `Kc_v` |
| `MSG_CERT_V2C` | `0x08` | V -> Client | whole payload encrypted with `Kc_v` |
| `MSG_ERROR` | `0x65` | any | plain |
| `MSG_APP` | `0x66` | Client <-> V | final game chain encrypts whole payload with `Kc_v` |

## Kerberos Messages

### `MSG_AS_REQ`

Header: `src=ClientX`, `dst=AS`。Payload 不加密。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `idc` | `u8` | Client ID |
| 2 | `idtgs` | `u8` | TGS ID，当前为 `0x12` |
| 3 | `ts1` | `u64` | Client 发起 AS 请求的时间戳 |

### `Ticket_tgs`

该结构由 AS 构造，作为 `AS_REP.ticket_tgs` 发送给 Client，再由 Client 原样转交给 TGS。Wire 中字段为 `DES(KTGS, Ticket_tgs)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `kc_tgs` | `u64` | Client/TGS 会话密钥 |
| 2 | `idc` | `u8` | Client ID |
| 3 | `adc` | `u32` | Client 地址标识，当前默认 `0x7F000001` |
| 4 | `idtgs` | `u8` | TGS ID |
| 5 | `ts2` | `u64` | AS 生成票据时间 |
| 6 | `lifetime2` | `u64` | TGS 票据有效期 |

### `MSG_AS_REP`

Header: `src=AS`, `dst=ClientX`。整个 payload 为 `DES(Kc, AS_REP_BODY)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `kc_tgs` | `u64` | Client/TGS 会话密钥 |
| 2 | `idtgs` | `u8` | TGS ID |
| 3 | `ts2` | `u64` | AS 时间戳 |
| 4 | `lifetime2` | `u64` | TGS 票据有效期 |
| 5 | `ticket_tgs` | `bytes_u16` | `DES(KTGS, Ticket_tgs)` |

### `Authenticator`

TGS 阶段和 V_AUTH 阶段共用该结构。

| Order | Field | Type | Encryption |
| --- | --- | --- | --- |
| 1 | `idc` | `u8` | nested encrypted |
| 2 | `adc` | `u32` | nested encrypted |
| 3 | `ts` | `u64` | nested encrypted |

`authenticator_tgs = DES(Kc_tgs, Authenticator)`；`authenticator_v = DES(Kc_v, Authenticator)`。

### `MSG_TGS_REQ`

Header: `src=ClientX`, `dst=TGS`。外层 payload 不整体加密。

| Order | Field | Type | Encryption |
| --- | --- | --- | --- |
| 1 | `idv` | `u8` | plain，当前为 V |
| 2 | `ticket_tgs` | `bytes_u16` | `DES(KTGS, Ticket_tgs)` |
| 3 | `authenticator_tgs` | `bytes_u16` | `DES(Kc_tgs, Authenticator)` |

### `Ticket_v`

该结构由 TGS 构造，作为 `TGS_REP.ticket_v` 发送给 Client，再由 Client 原样转交给 V。Wire 中字段为 `DES(KV, Ticket_v)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `kc_v` | `u64` | Client/V 会话密钥 |
| 2 | `idc` | `u8` | Client ID |
| 3 | `adc` | `u32` | Client 地址标识 |
| 4 | `idv` | `u8` | V ID |
| 5 | `ts4` | `u64` | TGS 生成票据时间 |
| 6 | `lifetime4` | `u64` | V 票据有效期 |

### `MSG_TGS_REP`

Header: `src=TGS`, `dst=ClientX`。整个 payload 为 `DES(Kc_tgs, TGS_REP_BODY)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `kc_v` | `u64` | Client/V 会话密钥 |
| 2 | `idv` | `u8` | V ID |
| 3 | `ts4` | `u64` | TGS 时间戳 |
| 4 | `ticket_v` | `bytes_u16` | `DES(KV, Ticket_v)` |

### `MSG_V_AUTH_REQ`

Header: `src=ClientX`, `dst=V`。外层 payload 不整体加密。

| Order | Field | Type | Encryption |
| --- | --- | --- | --- |
| 1 | `ticket_v` | `bytes_u16` | `DES(KV, Ticket_v)` |
| 2 | `authenticator_v` | `bytes_u16` | `DES(Kc_v, Authenticator)` |

### `MSG_V_AUTH_REP`

Header: `src=V`, `dst=ClientX`。整个 payload 为 `DES(Kc_v, V_AUTH_REP_BODY)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `ts5_plus_1` | `u64` | V 对 Client 时间戳的确认 |

## Certificate Messages

证书交换发生在 V_AUTH 成功之后，同一条 V TCP 连接继续使用。

### `MSG_CERT_C2V`

Header: `src=ClientX`, `dst=V`。整个 payload 为 `DES(Kc_v, CERT_C2V_BODY)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `client_id` | `u8` | Client ID |
| 2 | `cert` | `bytes_u16` | Client 证书序列化字节 |

### `MSG_CERT_V2C`

Header: `src=V`, `dst=ClientX`。整个 payload 为 `DES(Kc_v, CERT_V2C_BODY)`。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `v_id` | `u8` | V ID |
| 2 | `cert` | `bytes_u16` | V 证书序列化字节 |

## Application Envelope

最终游戏链路中，所有游戏应用层内容都放在 `MSG_APP` 中。固定 header 不加密，payload 加密。

```text
Packet(MSG_APP).payload = DES(Kc_v, SignedAppPayload)
SignedAppPayload = app_code + app_payload + signature
signature = RSA_private(hash64(app_code + app_payload))
```

签名输入只包含 `app_code + app_payload`，不包含 Packet header。这样 header 仍可被网络层、日志层和 Protocol UI 解析。

### `SignedAppPayload`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `app_code` | `u8` | `AppCode` |
| 2 | `app_payload` | `bytes_u16` | 应用层业务 payload |
| 3 | `signature` | `bytes_u16` | 对 `app_code + app_payload` 的 RSA/hash 签名 |

### `AppCode`

| AppCode | Value | Direction | Payload |
| --- | --- | --- | --- |
| `GAME_JOIN_REQ` | `0x05` | Client -> V | `GameMessage(join, JoinMessage)` |
| `GAME_STATE` | `0x07` | V -> Client | `GameMessage(state, BattleStateSnapshot)` |
| `APP_ACK` | `0x08` | bidirectional | `AppAckPayload` |
| `GAME_MOVE` | `0x09` | Client -> V | `GameMessage(move, MoveMessage)` |
| `GAME_TARGET` | `0x0A` | Client -> V | `GameMessage(target, TargetMessage)` |
| `GAME_SHOOT` | `0x0B` | Client -> V | `GameMessage(shoot, ShootMessage)` |

### `AppAckPayload`

ACK 本身也是 `MSG_APP.APP_ACK`，同样签名并用 `Kc_v` 加密。ACK 不再被 ACK。

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `acked_msg_type` | `u8` | 被确认 packet 的 `msg_type`，游戏链路通常为 `MSG_APP` |
| 2 | `acked_app_code` | `u8` | 被确认 packet 的 `app_code` |
| 3 | `acked_src` | `u8` | 被确认 packet 的发送方 |
| 4 | `acked_dst` | `u8` | 被确认 packet 的接收方 |
| 5 | `acked_payload_len` | `u32` | 被确认 packet 的 wire payload 长度 |
| 6 | `acked_payload_hash` | `u64` | 被确认 packet wire payload 的 `hash64` |

## GameMessage

`SignedAppPayload.app_payload` 中的游戏业务 payload 先包一层 `GameMessage`：

```text
GameMessage = game_msg_type u8 + game_payload
```

| GameMsgType | Value | Payload |
| --- | --- | --- |
| `join` | `0x01` | `JoinMessage` |
| `move` | `0x02` | `MoveMessage` |
| `target` | `0x03` | `TargetMessage` |
| `shoot` | `0x04` | `ShootMessage` |
| `state` | `0x10` | `BattleStateSnapshot` |
| `error` | `0x7F` | reserved |

### `JoinMessage`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `client_id` | `u8` | 加入游戏的 Client ID；V 会校验它与 Packet `src` 一致 |

### `MoveMessage`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `x` | `i8` | 水平方向输入，常见值 `-1/0/1` |
| 2 | `y` | `i8` | 垂直方向输入，常见值 `-1/0/1` |

### `TargetMessage`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `angle` | `f32` | 炮塔角度 |

### `ShootMessage`

`ShootMessage` 当前为空 payload。一次 `GAME_SHOOT` 表示一次开火请求；没有 `shooting=true/false` 状态字段。

### `BattleStateSnapshot`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `server_time_ms` | `u64` | V 生成快照时的服务器时间 |
| 2 | `total_score` | `u16` | 全局总分 |
| 3 | `winner_team` | `i8` | 当前获胜队伍；`-1` 表示无获胜展示 |
| 4 | `team_count` | `u8` | 队伍数量 |
| 5 | `teams[]` | repeated `TeamSnapshot` | 队伍状态 |
| 6 | `tank_count` | `u8` | 坦克数量 |
| 7 | `tanks[]` | repeated `TankSnapshot` | 坦克状态 |
| 8 | `bullet_count` | `u16` | 子弹数量 |
| 9 | `bullets[]` | repeated `BulletSnapshot` | 子弹状态 |
| 10 | `pickable_count` | `u16` | 补给数量 |
| 11 | `pickables[]` | repeated `PickableSnapshot` | 补给状态 |

### `TeamSnapshot`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `team_id` | `u8` | 队伍 ID |
| 2 | `score` | `u16` | 队伍分数 |
| 3 | `tanks` | `u8` | 队伍坦克数量 |

### `TankSnapshot`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `client_id` | `u8` | Client ID |
| 2 | `team` | `u8` | 队伍 ID |
| 3 | `x` | `f32` | 坦克 x 坐标 |
| 4 | `y` | `f32` | 坦克 y 坐标 |
| 5 | `angle` | `f32` | 炮塔角度 |
| 6 | `hp` | `i8` | 生命值 |
| 7 | `shield` | `i8` | 护盾值 |
| 8 | `dead` | `bool_u8` | 是否死亡 |
| 9 | `score` | `u16` | 玩家分数 |

### `BulletSnapshot`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `id` | `u16` | 子弹 ID |
| 2 | `owner_client_id` | `u8` | 发射者 Client ID |
| 3 | `x` | `f32` | 子弹 x 坐标 |
| 4 | `y` | `f32` | 子弹 y 坐标 |
| 5 | `special` | `bool_u8` | 是否强化子弹 |

### `PickableSnapshot`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `id` | `u16` | 补给 ID |
| 2 | `type` | `u8` | `repair=1`、`damage=2`、`shield=3` |
| 3 | `x` | `f32` | 补给 x 坐标 |
| 4 | `y` | `f32` | 补给 y 坐标 |

## `MSG_ERROR`

| Order | Field | Type | Description |
| --- | --- | --- | --- |
| 1 | `err_code` | `u8` | `ErrorCode` |
| 2 | `err_msg` | bytes | 剩余字节作为错误文本 |

| ErrorCode | Value |
| --- | --- |
| `ERR_PASSWORD_WRONG` | `0x01` |
| `ERR_TGT_EXPIRED` | `0x02` |
| `ERR_TICKET_V_EXPIRED` | `0x03` |
| `ERR_TGS_ID_MISMATCH` | `0x04` |
| `ERR_V_ID_MISMATCH` | `0x05` |
| `ERR_REPLAY_DETECTED` | `0x06` |
| `ERR_UNSUPPORTED_MSG_TYPE` | `0x07` |

## Protocol Events For UI

`protocol_events/*.txt` 是 Protocol Monitor 的唯一输入源。每行是一个发送或接收事件。

| Field | Meaning |
| --- | --- |
| `ts` | 事件时间 |
| `direction` | `SEND` 或 `RECV` |
| `endpoint` | 逻辑方向，例如 `Client1->V` |
| `message` | 报文名；`MSG_APP` 和 `MSG_ERROR` 会追加 `AppCode` 或 `ErrorCode` 后缀 |
| `category` | `kerberos`、`app`、`error` |
| `msg_type` | header 中的 `msg_type` |
| `src` | header 中的 `src` |
| `dst` | header 中的 `dst` |
| `payload_len` | header 中的 payload 长度 |
| `reserved` | header 中的 reserved |
| `packet_hex` | 完整 packet bytes：11-byte header + wire payload |
| `payload_hex` | 网络上传输的 payload bytes |
| `payload_plain_hex` | 本进程可解密/可构造时记录的明文 payload |
| `payload_encrypted_hex` | 本进程可解密/可构造时记录的密文 payload |
| `fieldN_name` | Kerberos 内部加密字段名，例如 `ticket_tgs` |
| `fieldN_plain_hex` | 内部字段明文 |
| `fieldN_encrypted_hex` | 内部字段密文 |

SEND 事件通常能同时记录明文和即将上网的密文；RECV 事件通常先记录网络密文，再记录解密后的明文。Protocol UI 的 Header 区域只展示固定 11B header；Payload 区域展示结构化解析；Payload Hex 区域展示明文和密文 hex。

