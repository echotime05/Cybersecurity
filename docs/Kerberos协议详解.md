# Kerberos 协议详解

## 目录

1. [概述](#概述)
2. [报文通用格式](#报文通用格式)
3. [阶段一：AS 交换](#阶段一as-交换-client--as)
4. [阶段二：TGS 交换](#阶段二tgs-交换-client--tgs)
5. [阶段三：V_AUTH 交换](#阶段三v_auth-交换-client--v)
6. [阶段四：证书交换](#阶段四证书交换-client--v)
7. [全部密钥汇总](#全部密钥汇总)
8. [完整流程图](#完整流程图)

---

## 概述

本项目的 Kerberos 认证分为 **4 个阶段、7 条消息**：

| 阶段 | 消息 | 方向 | 报文类型 |
|------|------|------|----------|
| 1. AS 交换 | ① AS_REQ | Client → AS | `MSG_AS_REQ` (0x01) |
| 1. AS 交换 | ② AS_REP | AS → Client | `MSG_AS_REP` (0x02) |
| 2. TGS 交换 | ③ TGS_REQ | Client → TGS | `MSG_TGS_REQ` (0x03) |
| 2. TGS 交换 | ④ TGS_REP | TGS → Client | `MSG_TGS_REP` (0x04) |
| 3. V_AUTH 交换 | ⑤ V_AUTH_REQ | Client → V | `MSG_V_AUTH_REQ` (0x05) |
| 3. V_AUTH 交换 | ⑥ V_AUTH_REP | V → Client | `MSG_V_AUTH_REP` (0x06) |
| 4. 证书交换 | ⑦ CERT_C2V | Client → V | `MSG_CERT_C2V` (0x07) |
| 4. 证书交换 | ⑧ CERT_V2C | V → Client | `MSG_CERT_V2C` (0x08) |

所有消息通过 TCP 传输，使用 11 字节固定 Packet Header + 变长 Payload 格式。

---

## 报文通用格式

### Packet Header（11 字节固定头）

```
字节偏移  大小   字段         说明
─────────────────────────────────────────
0         1B     msg_type     消息类型 (MsgType 枚举)
1         1B     src          源实体 ID (EntityId 枚举)
2         1B     dst          目标实体 ID (EntityId 枚举)
3         4B     payload_len  payload 长度 (大端序 uint32)
7         4B     reserved     保留字段 (固定 0x00000000)
```

### 实体 ID 对照表

| 实体 | EntityId | 16 进制 |
|------|----------|---------|
| Client1 | `EntityId::client1` | `0x01` |
| Client2 | `EntityId::client2` | `0x02` |
| Client3 | `EntityId::client3` | `0x03` |
| Client4 | `EntityId::client4` | `0x04` |
| AS | `EntityId::as` | `0x11` |
| TGS | `EntityId::tgs` | `0x12` |
| V | `EntityId::v` | `0x13` |

---

## 阶段一：AS 交换 (Client → AS)

### ① MSG_AS_REQ — Client 认证请求

**方向**：Client → AS

**报文头**：
```
msg_type = 0x01 (MSG_AS_REQ)
src      = 0x01~0x04 (Client ID)
dst      = 0x11 (AS)
```

**Payload（10 字节，明文）**：

```
字节偏移  大小   字段     说明
─────────────────────────────────
0         1B     idc      请求认证的 Client ID
1         1B     idtgs    希望访问的 TGS ID = 0x12
2         8B     ts1      客户端时间戳（大端序 uint64）
```

**加密状态**：**不加密**（明文传输）。AS_REQ 不包含任何敏感数据，只是声明身份。

---

### ② MSG_AS_REP — AS 返回 TGT

**方向**：AS → Client

**报文头**：
```
msg_type = 0x02 (MSG_AS_REP)
src      = 0x11 (AS)
dst      = 0x01~0x04 (Client ID)
```

**Payload 整体加密**：用 **Kc**（Client 长期密钥，56 位 DES）加密。

#### 解密前（AS_REP Payload 密文）

全部字节为密文，Client 用 `Kc` 解密后才能读取。

#### 解密后（AsRepBody 明文，变长）

```
字节偏移  大小         字段         说明
─────────────────────────────────────────
0         8B           kc_tgs       Client↔TGS 会话密钥（AS 新生成）
8         1B           idtgs        TGS ID = 0x12
9         8B           ts2          AS 时间戳
17        8B           lifetime2    票据有效期（5 分钟 = 300,000 ms）
25        2B + 变长    ticket_tgs   加密的 TGT（长度前缀 + 密文）
```

**ticket_tgs 内部结构**（TicketTgsBody，30 字节，用 **KTGS** 加密）：

```
解密后 ticket_tgs 的明文（30 字节）：
字节偏移  大小   字段       说明
───────────────────────────────────
0         8B     kc_tgs     与 AsRepBody 中相同的会话密钥
8         1B     idc        Client ID
9         4B     adc        Client 网络地址（固定 0x7F000001）
13        1B     idtgs      TGS ID = 0x12
14        8B     ts2        签发时间戳
22        8B     lifetime2  有效期
```

**加密层次**：

```
AsRepBody 整体 ─── 用 Kc 加密 ───→ AS_REP payload（Client 可解密）

ticket_tgs 部分 ─── 用 KTGS 加密 ───→ 嵌在 AsRepBody 中的 ticket_tgs 字段（Client 无法解密，只能原样转发给 TGS）
```

**密钥使用**：

| 密钥 | 用途 | 谁知道 |
|------|------|--------|
| `Kc` | 加密整个 AS_REP payload | Client, AS（配置文件中预设） |
| `KTGS` | 加密 ticket_tgs | AS, TGS（配置文件中预设，Client 不知道） |
| `Kc_tgs` | AS 新生成的会话密钥，同时出现在 AsRepBody 明文和 ticket_tgs 明文中 | Client（解密获得），TGS（解密 ticket_tgs 获得） |

---

## 阶段二：TGS 交换 (Client → TGS)

### ③ MSG_TGS_REQ — Client 请求服务票据

**方向**：Client → TGS

**报文头**：
```
msg_type = 0x03 (MSG_TGS_REQ)
src      = 0x01~0x04 (Client ID)
dst      = 0x12 (TGS)
```

**Payload（变长，部分加密）**：

```
字节偏移  大小         字段               加密状态
────────────────────────────────────────────────────
0         1B           idv                明文：希望访问的目标 V = 0x13
1         2B + 变长    ticket_tgs         密文：由 KTGS 加密，Client 从 AS_REP 原样转发
...       2B + 变长    authenticator_tgs  密文：由 Kc_tgs 加密，Client 自行构造
```

**ticket_tgs**：从 AS_REP 原样转发，Client 无法解密（由 KTGS 加密）。

**authenticator_tgs 内部结构**（AuthenticatorBody，13 字节）：

```
加密前明文（13 字节）：
字节偏移  大小   字段   说明
─────────────────────────────
0         1B     idc    Client ID
1         4B     adc    Client 网络地址（固定 0x7F000001）
5         8B     ts     当前时间戳
```

用 **Kc_tgs** 加密后成为 16 字节（DES 8 字节块对齐，含 padding）。

**注意**：TGS_REQ 的外层 payload 不整体加密，但内嵌的两个字段（ticket_tgs 和 authenticator_tgs）各自是加密的。

---

### ④ MSG_TGS_REP — TGS 返回服务票据

**方向**：TGS → Client

**报文头**：
```
msg_type = 0x04 (MSG_TGS_REP)
src      = 0x12 (TGS)
dst      = 0x01~0x04 (Client ID)
```

**Payload 整体加密**：用 **Kc_tgs**（Client↔TGS 会话密钥）加密。

#### 解密后（TgsRepBody 明文，变长）

```
字节偏移  大小         字段      说明
──────────────────────────────────────
0         8B           kc_v      Client↔V 会话密钥（TGS 新生成）
8         1B           idv       V ID = 0x13
9         8B           ts4       TGS 时间戳
17        2B + 变长    ticket_v  加密的服务票据（长度前缀 + 密文）
```

**ticket_v 内部结构**（TicketVBody，30 字节，用 **KV** 加密）：

```
解密后 ticket_v 的明文（30 字节）：
字节偏移  大小   字段       说明
───────────────────────────────────
0         8B     kc_v       Client↔V 会话密钥
8         1B     idc        Client ID
9         4B     adc        Client 网络地址
13        1B     idv        V ID = 0x13
14        8B     ts4        签发时间戳
22        8B     lifetime4  有效期（5 分钟）
```

**加密层次**：

```
TgsRepBody 整体 ─── 用 Kc_tgs 加密 ───→ TGS_REP payload（Client 可解密）

ticket_v 部分 ─── 用 KV 加密 ───→ 嵌在 TgsRepBody 中（Client 无法解密，只能原样转发给 V）
```

**密钥使用**：

| 密钥 | 用途 | 谁知道 |
|------|------|--------|
| `Kc_tgs` | 加密整个 TGS_REP payload，也加密 authenticator_tgs | Client（AS_REP 获取），TGS（从 ticket_tgs 解密获得） |
| `KV` | 加密 ticket_v | TGS, V（配置文件中预设，Client 不知道） |
| `Kc_v` | TGS 新生成的会话密钥，同时出现在 TgsRepBody 明文和 ticket_v 明文中 | Client（解密获得），V（解密 ticket_v 获得） |

---

## 阶段三：V_AUTH 交换 (Client → V)

### ⑤ MSG_V_AUTH_REQ — Client 向 V 证明身份

**方向**：Client → V

**报文头**：
```
msg_type = 0x05 (MSG_V_AUTH_REQ)
src      = 0x01~0x04 (Client ID)
dst      = 0x13 (V)
```

**Payload（变长，部分加密）**：

```
字节偏移  大小         字段             加密状态
──────────────────────────────────────────────────
0         2B + 变长    ticket_v         密文：由 KV 加密，Client 从 TGS_REP 原样转发
...       2B + 变长    authenticator_v  密文：由 Kc_v 加密，Client 自行构造
```

**ticket_v**：从 TGS_REP 原样转发，Client 无法解密（由 KV 加密）。

**authenticator_v 内部结构**（AuthenticatorBody，13 字节，与 TGS 认证器格式相同）：

```
加密前明文（13 字节）：
字节偏移  大小   字段   说明
─────────────────────────────
0         1B     idc    Client ID
1         4B     adc    Client 网络地址
5         8B     ts5    当前时间戳
```

用 **Kc_v** 加密。

**V 收到后的处理**：
1. 用 `KV` 解密 ticket_v → 获得 `idc`、`kc_v` 等
2. 用 `kc_v` 解密 authenticator_v → 获得 `idc`、`adc`、`ts5`
3. 验证 ticket_v 中的 `idc` == authenticator_v 中的 `idc`（票据和认证器属于同一 Client）

---

### ⑥ MSG_V_AUTH_REP — V 认证应答

**方向**：V → Client

**报文头**：
```
msg_type = 0x06 (MSG_V_AUTH_REP)
src      = 0x13 (V)
dst      = 0x01~0x04 (Client ID)
```

**Payload 整体加密**：用 **Kc_v**（Client↔V 会话密钥）加密。

#### 解密后（VAuthRepBody 明文，8 字节）

```
字节偏移  大小   字段          说明
────────────────────────────────────
0         8B     ts5_plus_1   ts5 + 1（大端序 uint64）
```

**验证逻辑**：Client 解密后检查 `ts5_plus_1 == ts5 + 1`，确认 V 正确解密了认证器（因为只有拥有 `Kc_v` 的实体才能解密 authenticator_v 获取 `ts5`，然后加 1 返回）。

**密钥使用**：

| 密钥 | 用途 | 谁知道 |
|------|------|--------|
| `KV` | 解密 ticket_v | V, TGS |
| `Kc_v` | 加密 V_AUTH_REP payload，解密 authenticator_v | Client（TGS_REP 获取），V（从 ticket_v 解密获得） |

---

## 阶段四：证书交换 (Client → V)

### ⑦ MSG_CERT_C2V — Client 发送证书给 V

**方向**：Client → V

**报文头**：
```
msg_type = 0x07 (MSG_CERT_C2V)
src      = 0x01~0x04 (Client ID)
dst      = 0x13 (V)
```

**Payload 整体加密**：用 **Kc_v** 加密。

#### 解密后（CertC2VBody 明文，变长）

```
字节偏移  大小         字段       说明
──────────────────────────────────────
0         1B           client_id  Client ID
1         2B + 变长    cert       序列化的 X.509 风格证书
```

**证书内容**（`Certificate` 结构体）：
```
字段          说明
────────────────────────────
subject_id    Client ID
subject_pk    Client 的 RSA 公钥 (n, e)
issuer_id     CA 标识
signature     CA 用私钥对上述字段的 RSA 签名
```

**Client 侧构造**：`make_certificate(client_id, client_key_pair.public_key, ca_private_key)`

**V 侧验证**：`verify_certificate(client_cert, ca_public_key)` — 用 CA 公钥验证证书签名，确认 Client 公钥的真实性。

---

### ⑧ MSG_CERT_V2C — V 发送证书给 Client

**方向**：V → Client

**报文头**：
```
msg_type = 0x08 (MSG_CERT_V2C)
src      = 0x13 (V)
dst      = 0x01~0x04 (Client ID)
```

**Payload 整体加密**：用 **Kc_v** 加密。

#### 解密后（CertV2CBody 明文，变长）

```
字节偏移  大小         字段    说明
───────────────────────────────────
0         1B           v_id   V ID = 0x13
1         2B + 变长    cert   序列化的 V 证书
```

**Client 侧验证**：
1. `parse_certificate(v_cert)` 反序列化
2. 验证 `cert_v.v_id == EntityId::v`
3. `verify_certificate(v_cert, ca_public_key)` 用 CA 公钥验证签名
4. 验证 `v_cert.subject_id == EntityId::v`

验证通过后，Client 保存 V 的 RSA 公钥，用于后续游戏报文的签名验证和 ACK 验证。

**密钥使用**：

| 密钥 | 用途 | 谁知道 |
|------|------|--------|
| `Kc_v` | 加密 CERT_C2V 和 CERT_V2C 的 payload | Client, V |
| `SK_CA_D` (CA 私钥) | 签发证书（RSA 签名） | CA（配置文件中预设，AS/Client 签发时使用） |
| `PK_CA_N` / `PK_CA_E` (CA 公钥) | 验证证书（RSA 验签） | 所有角色（配置文件中预设） |

---

## 全部密钥汇总

| 密钥 | 类型 | 长度 | 配置文件项 | 持有方 | 用途 |
|------|------|------|-----------|--------|------|
| `Kc` | DES 对称密钥 | 56 bit | `C1_KC` ~ `C4_KC` | Client, AS | 加密 AS_REP |
| `KTGS` | DES 对称密钥 | 56 bit | `KTGS` | AS, TGS | 加密/解密 TicketTgsBody (TGT) |
| `Kc_tgs` | DES 对称密钥 | 56 bit | 动态生成 | Client, AS, TGS | Client↔TGS 会话密钥，加密 TGS_REP 和 Authenticator_tgs |
| `KV` | DES 对称密钥 | 56 bit | `KV` | TGS, V | 加密/解密 TicketVBody (服务票据) |
| `Kc_v` | DES 对称密钥 | 56 bit | 动态生成 | Client, TGS, V | Client↔V 会话密钥，加密 V_AUTH_REP、CERT 交换和游戏 payload |
| `PK_CA_N` / `PK_CA_E` | RSA 公钥 | 1024+ bit | `PK_CA_N`, `PK_CA_E` | 所有角色 | 验证证书签名 |
| `SK_CA_D` | RSA 私钥 | 1024+ bit | `SK_CA_D` | AS, Client（签发用） | 签发证书 |
| Client RSA KeyPair | RSA 密钥对 | 1024+ bit | 代码内置 | Client | 游戏报文签名和 ACK 签名 |
| V RSA KeyPair | RSA 密钥对 | 1024+ bit | 代码内置 | V | 游戏报文签名和 ACK 签名 |

### 密钥层次关系

```
Kc (长期密钥，配置文件预设)
  └─ 加密 AS_REP ──→ Client 获取 Kc_tgs

KTGS (长期密钥，配置文件预设)
  └─ 加密 TicketTgsBody ──→ TGS 获取 Kc_tgs

Kc_tgs (会话密钥，AS 动态生成)
  ├─ 加密 Authenticator_tgs (Client→TGS)
  └─ 加密 TGS_REP ──→ Client 获取 Kc_v

KV (长期密钥，配置文件预设)
  └─ 加密 TicketVBody ──→ V 获取 Kc_v

Kc_v (会话密钥，TGS 动态生成)
  ├─ 加密 Authenticator_v (Client→V)
  ├─ 加密 V_AUTH_REP
  ├─ 加密 CERT_C2V / CERT_V2C
  └─ 加密 MSG_APP 游戏 payload
```

### 动态密钥生命周期

| 密钥 | 生成方 | 生成时机 | 有效期 | 传递方式 |
|------|--------|----------|--------|----------|
| `Kc_tgs` | AS | 收到 AS_REQ 时 | 5 分钟（lifetime2） | 明文嵌在 AS_REP 中 + 密文嵌在 ticket_tgs 中 |
| `Kc_v` | TGS | 收到 TGS_REQ 时 | 5 分钟（lifetime4） | 明文嵌在 TGS_REP 中 + 密文嵌在 ticket_v 中 |

---

## 完整流程图

```
Client                                AS                      TGS                     V
EntityId: 0x01~0x04                   0x11                    0x12                    0x13
  │                                    │                       │                       │
  │═══════ 阶段一：AS 交换 ════════════│                       │                       │
  │                                    │                       │                       │
  │── ① MSG_AS_REQ ──────────────────>│                       │                       │
  │   Header: type=0x01 src=C dst=AS   │                       │                       │
  │   Payload(明文): idc, idtgs, ts1   │                       │                       │
  │                                    │ 查找 Client 密钥 Kc    │                       │
  │                                    │ 生成会话密钥 Kc_tgs    │                       │
  │                                    │ 构造 TicketTgsBody      │                       │
  │                                    │ 用 KTGS 加密 ticket_tgs│                       │
  │<─ ② MSG_AS_REP ──────────────────│                       │                       │
  │   Header: type=0x02 src=AS dst=C   │                       │                       │
  │   Payload(用 Kc 加密):             │                       │                       │
  │     Kc_tgs(明文), ts2, lifetime2,  │                       │                       │
  │     ticket_tgs(用 KTGS 加密)       │                       │                       │
  │                                    │                       │                       │
  │   Client 用 Kc 解密获得 Kc_tgs     │                       │                       │
  │                                    │                       │                       │
  │═══════ 阶段二：TGS 交换 ═══════════════════════════════════│                       │
  │                                    │                       │                       │
  │── ③ MSG_TGS_REQ ────────────────────────────────────────>│                       │
  │   Header: type=0x03 src=C dst=TGS  │                       │                       │
  │   Payload:                         │                       │                       │
  │     idv(明文)=0x13                 │                       │                       │
  │     ticket_tgs(用 KTGS 加密)       │                       │                       │
  │     authenticator_tgs(用 Kc_tgs 加密)                      │                       │
  │                                    │                       │ 用 KTGS 解密 ticket  │
  │                                    │                       │ 获得 Kc_tgs           │
  │                                    │                       │ 解密 authenticator    │
  │                                    │                       │ 校验 idc/idtgs/idv    │
  │                                    │                       │ 生成会话密钥 Kc_v     │
  │                                    │                       │ 构造 TicketVBody       │
  │                                    │                       │ 用 KV 加密 ticket_v   │
  │<─ ④ MSG_TGS_REP ────────────────────────────────────────│                       │
  │   Header: type=0x04 src=TGS dst=C  │                       │                       │
  │   Payload(用 Kc_tgs 加密):         │                       │                       │
  │     Kc_v(明文), ts4,               │                       │                       │
  │     ticket_v(用 KV 加密)           │                       │                       │
  │                                    │                       │                       │
  │   Client 用 Kc_tgs 解密获得 Kc_v   │                       │                       │
  │                                    │                       │                       │
  │═══════ 阶段三：V_AUTH 交换 ═══════════════════════════════════════════════════════│
  │                                    │                       │                       │
  │── ⑤ MSG_V_AUTH_REQ ────────────────────────────────────────────────────────────>│
  │   Header: type=0x05 src=C dst=V    │                       │                       │
  │   Payload:                         │                       │                       │
  │     ticket_v(用 KV 加密)           │                       │                       │
  │     authenticator_v(用 Kc_v 加密)  │                       │                       │
  │                                    │                       │                       │ 用 KV 解密 ticket_v
  │                                    │                       │                       │ 获得 Kc_v
  │                                    │                       │                       │ 解密 authenticator_v
  │                                    │                       │                       │ 校验 idc
  │<─ ⑥ MSG_V_AUTH_REP ────────────────────────────────────────────────────────────│
  │   Header: type=0x06 src=V dst=C    │                       │                       │
  │   Payload(用 Kc_v 加密):           │                       │                       │
  │     ts5_plus_1 = ts5 + 1           │                       │                       │
  │                                    │                       │                       │
  │   Client 验证 ts5+1 == ts5+1       │                       │                       │
  │                                    │                       │                       │
  │═══════ 阶段四：证书交换 ═════════════════════════════════════════════════════════│
  │                                    │                       │                       │
  │── ⑦ MSG_CERT_C2V ──────────────────────────────────────────────────────────────>│
  │   Header: type=0x07 src=C dst=V    │                       │                       │
  │   Payload(用 Kc_v 加密):           │                       │                       │
  │     client_id, Client证书          │                       │                       │
  │                                    │                       │                       │ 用 CA 公钥验签
  │                                    │                       │                       │ 保存 Client 公钥
  │<─ ⑧ MSG_CERT_V2C ──────────────────────────────────────────────────────────────│
  │   Header: type=0x08 src=V dst=C    │                       │                       │
  │   Payload(用 Kc_v 加密):           │                       │                       │
  │     v_id, V证书                    │                       │                       │
  │                                    │                       │                       │
  │   Client 用 CA 公钥验证 V 证书     │                       │                       │
  │   保存 V 的 RSA 公钥               │                       │                       │
  │                                    │                       │                       │
  │═══════ 认证完成 =════════════════════════════════════════════════════════════════│
  │                                    │                       │                       │
  │========== TCP 长连接，双方持有 Kc_v 和对方 RSA 公钥 =============================│
  │                                    │                       │                       │
  │   之后传输 MSG_APP (type=0x66)     │                       │                       │
  │   游戏 payload 用 Kc_v 加密        │                       │                       │
  │   游戏消息用 RSA 签名 + ACK 不可否认                                       │
  │                                    │                       │                       │
```

---

## 加密算法细节

### DES 变体

- 类型：16 轮 Feistel 网络
- 模式：ECB（Electronic Codebook）
- 密钥长度：56 位（`uint64_t`，仅低 56 位有效，mask `0x00FFFFFFFFFFFFFF`）
- Padding：类 PKCS7（1~8 字节，填充值为需填充的字节数）
- 块大小：8 字节（64 位）
- 实现文件：[`src/shared/crypto/crypto.cpp`](../src/shared/crypto/crypto.cpp)

### 哈希算法

- 类型：FNV-1a（Fowler-Noll-Vo）64 位变体
- 用途：将 `"client-kc-v1:<id>:<password>"` 字符串哈希转换为 56 位 DES 密钥
- 实现文件：[`src/shared/auth/auth_credentials.cpp`](../src/shared/auth/auth_credentials.cpp)

### RSA

- 用途：证书签名/验签、游戏报文签名/验签
- 操作：模幂运算 `m^e mod n` 和 `m^d mod n`
- 实现文件：[`src/shared/crypto/crypto.cpp`](../src/shared/crypto/crypto.cpp)
