# Kerberos 认证调用链

登录按钮按下后，从浏览器到 Kerberos 认证完成的完整函数调用顺序。

---

## 阶段 0：浏览器 → WebSocket → 解析 JSON

```
浏览器发送: {"type":"login","clientId":1,"password":"123456"}
  │
  ▼
[websocket.cpp] recv_some()                     # Winsock recv() 读取 TCP 数据
[websocket.cpp] parse_ws_frame()                # 解析 WebSocket frame，去 masking
[websocket.cpp] recv_ws_text()                  # 返回 JSON 文本字符串
  │
  ▼
[ui_bridge.cpp:179] handle_client()             # 每个浏览器连接的线程
[ui_bridge.cpp:190] recv_ws_text(socket)        # 接收 WebSocket 文本帧
[ui_bridge.cpp:115] parse_json_command(text)    # 解析 JSON → UiCommand{kind=login, clientId, password}
[ui_bridge.cpp:193] handler_(command)           # 回调 → TankGameClient::handle_ui_command
```

---

## 阶段 1：Client 收到登录命令

```
[tank_game_client.cpp:68] handle_ui_command(command)
[tank_game_client.cpp:72] handle_login(command)
  │
  ├─ [tank_game_client.cpp:86] 验证 is_client(id) && password 非空
  ├─ [tank_game_client.cpp:101] broadcast_text("authenticating")
  │
  ├─ [auth_credentials.cpp:9]  auth_derive_client_key(EntityId::client1, "123456")
  │     └─ 拼接 "client-kc-v1:1:123456" → hash64() → mask 56bit → 返回 kc
  │
  └─ [client_auth_flow.cpp:83] client_auth_connect_to_v_socket(config, client_id, kc)
```

---

## 阶段 2：Step 1 — 连接 AS（获取 Kc_tgs 和 Ticket_tgs）

```
[client_auth_flow.cpp:83] client_auth_connect_to_v_socket()
  │
  ├─ [L99-101] as_build_req({client_id, EntityId::tgs, ts1})  # 序列化 AS_REQ body
  │             [kerberos_messages.cpp:19] as_build_req()
  │             make_packet(MsgType::as_req, ...)               # 构造 Packet
  │
  ├─ [L102]    auth_exchange_packet(AS端点, as_req)
  │             [client_auth_flow.cpp:64] auth_exchange_packet()
  │               ├─ [net_socket.cpp] connect_tcp(AS_IP, AS_PORT)  # TCP 连接 AS
  │               ├─ [net_packet.cpp] send_packet_logged()         # 发送 MSG_AS_REQ
  │               │     └─ serialize_packet() [packet.cpp]
  │               ├─ [net_packet.cpp] recv_packet_logged()         # 接收 MSG_AS_REP
  │               │     └─ parse_packet() [packet.cpp]
  │               └─ close_socket()
  │
  ├─ [L105]    des_decrypt_payload(as_rep.payload, kc)  # 用 Client 长期密钥解密
  │             [crypto.cpp] des_decrypt_payload()
  │
  ├─ [L106]    as_parse_rep_body(plain)                 # 解析 AS_REP body
  │             [kerberos_messages.cpp:90] as_parse_rep_body()
  │
  └─ [L111-112] 提取 kc_tgs 和 ticket_tgs 存入 state
```

### AS 服务器侧（并行运行的另一个进程/线程）

```
[as_service.cpp:74] as_process_connection(socket, config)
  ├─ recv_packet_logged()       → 收到 MSG_AS_REQ
  ├─ as_parse_req()             → 解析请求
  ├─ as_find_client_secret()    → 查找 Client 的 kc
  ├─ generate_des_key56()       → 生成 kc_tgs
  ├─ tgs_ticket_encrypt(body, KTGS) → 用 KTGS 加密 TicketTgsBody
  ├─ as_build_rep_body()        → 序列化 AS_REP body
  ├─ des_encrypt_payload(rep, kc)   → 用 kc 加密
  └─ send_packet_logged()       → 发送 MSG_AS_REP
```

---

## 阶段 3：Step 2 — 连接 TGS（获取 Kc_v 和 Ticket_v）

```
[client_auth_flow.cpp:115-116] 仍在 client_auth_connect_to_v_socket() 中
  │
  ├─ authenticator_build_body({client_id, adc, ts})       # 序列化 Authenticator
  │   [kerberos_messages.cpp:103] authenticator_build_body()
  ├─ authenticator_encrypt(body, kc_tgs)                   # 用 Kc_tgs 加密
  │   [kerberos_messages.cpp:123] authenticator_encrypt()
  │
  ├─ tgs_build_req({EntityId::v, ticket_tgs, auth_tgs})   # 序列化 TGS_REQ
  │   [kerberos_messages.cpp:133] tgs_build_req()
  ├─ make_packet(MsgType::tgs_req, ...)
  │
  ├─ [L125-126] auth_exchange_packet(TGS端点, tgs_req, view)
  │   [client_auth_flow.cpp:64] auth_exchange_packet()
  │     ├─ connect_tcp(TGS_IP, TGS_PORT)                   # TCP 连接 TGS
  │     ├─ send_packet_logged()                            # 发送 MSG_TGS_REQ
  │     ├─ recv_packet_logged()                            # 接收 MSG_TGS_REP
  │     └─ close_socket()
  │
  ├─ [L128] des_decrypt_payload(tgs_rep.payload, kc_tgs)   # 用 Kc_tgs 解密
  ├─ [L129] tgs_parse_rep_body(plain)                      # 解析 TGS_REP body
  │   [kerberos_messages.cpp:199] tgs_parse_rep_body()
  │
  └─ [L134-135] 提取 kc_v 和 ticket_v 存入 state
```

### TGS 服务器侧

```
[tgs_service.cpp:60] tgs_process_connection(socket, config)
  ├─ [L66]  recv_packet_logged()          → 收到 MSG_TGS_REQ
  ├─ [L69]  tgs_parse_req(payload)        → 解析 TgsReq
  │          [kerberos_messages.cpp:142] tgs_parse_req()
  ├─ [L70-71] tgs_ticket_decrypt(ticket_tgs, KTGS)    → 用 KTGS 解密得到 TicketTgsBody
  │           [kerberos_messages.cpp:74] tgs_ticket_decrypt()
  ├─ [L72-73] authenticator_decrypt(auth_tgs, kc_tgs)  → 用 Kc_tgs 解密得到 AuthenticatorBody
  │           [kerberos_messages.cpp:128] authenticator_decrypt()
  ├─ [L74-78] 校验 ticket.idc==auth.idc && idtgs==0x12 && idv==0x13
  ├─ [L88]    generate_des_key56()        → 生成 kc_v
  ├─ [L90-91] 构造 TicketVBody{kc_v, idc, adc, EntityId::v, ts4, lifetime}
  ├─ [L92]    v_ticket_encrypt(body, KV)  → 用 KV 加密得到 ticket_v
  │           [kerberos_messages.cpp:179] v_ticket_encrypt()
  ├─ [L93-94] tgs_build_rep_body({kc_v, idv, ts4, ticket_v})
  │           [kerberos_messages.cpp:189] tgs_build_rep_body()
  ├─ [L95-97] des_encrypt_payload(rep_plain, kc_tgs)  → 用 Kc_tgs 加密 TGS_REP
  └─ [L103]   send_packet_logged()        → 发送 MSG_TGS_REP
```

---

## 阶段 4：Step 3 — 连接 V（V_AUTH + 证书交换）

```
[client_auth_flow.cpp:137-138] 仍在 client_auth_connect_to_v_socket() 中
  │
  ├─ connect_tcp(V_IP, V_PORT)                           # TCP 连接 V（保持长连接）
  │
  ├─ authenticator_encrypt({client_id, adc, ts5}, kc_v)  # 用 Kc_v 加密认证器
  ├─ v_auth_build_req({ticket_v, authenticator_v})       # 序列化 V_AUTH_REQ
  │   [kerberos_messages.cpp:211] v_auth_build_req()
  ├─ make_packet(MsgType::v_auth_req, ...)
  ├─ [L153] send_packet_logged(socket, v_req)            # 发送 MSG_V_AUTH_REQ
  │
  ├─ [L154] recv_packet_logged(socket)                   # 接收 MSG_V_AUTH_REP
  ├─ [L156] des_decrypt_payload(v_rep.payload, kc_v)     # 用 Kc_v 解密
  ├─ [L157] v_auth_parse_rep_body(plain)                 # 解析
  ├─ [L161] 验证 ts5_plus_1 == ts5 + 1                  # 防重放
  │
  ├─ 证书交换:
  │   ├─ make_certificate(client_id, client_pk, ca_sk)   # 创建 Client 证书
  │   │   [crypto.cpp]
  │   ├─ cert_build_c2v_body({client_id, cert_bytes})    # 序列化 CERT_C2V
  │   │   [certificate_messages.cpp]
  │   ├─ send_packet_logged(socket, cert_req)             # 发送 MSG_CERT_C2V
  │   ├─ recv_packet_logged(socket)                       # 接收 MSG_CERT_V2C
  │   ├─ cert_parse_v2c_body(plain)                       # 解析 V 证书
  │   ├─ parse_certificate(cert_bytes)                    # 反序列化证书
  │   │   [crypto.cpp]
  │   └─ verify_certificate(v_cert, ca_pk)                # 验证 V 证书签名
  │       [crypto.cpp]
  │
  └─ [L192] return VAuthenticatedSocket{state, socket}    # 返回已认证的 V socket
```

---

## 阶段 5：回到 Client — 认证完成

```
[tank_game_client.cpp:113-114] 回到 handle_login()
  │
  ├─ [L118-123] 保存 kc_v, client_key_pair, v_public_key, v_socket
  ├─ [L125]     启动 rx_thread_ = std::thread(receive_loop)  # 后台接收线程
  └─ [L126-127] broadcast_text("authenticated")            # 通知浏览器登录成功
```

---

## 各阶段涉及的文件总览

| 阶段 | 文件 | 关键函数 |
|------|------|----------|
| 浏览器→WebSocket 帧 | `src/roles/client/websocket.cpp` | `recv_some`, `parse_ws_frame`, `recv_ws_text` |
| JSON→UiCommand | `src/roles/client/ui_bridge.cpp` | `handle_client`, `parse_json_command` |
| 命令分发 | `src/roles/client/tank_game_client.cpp` | `handle_ui_command`, `handle_login` |
| 密钥派生 | `src/shared/auth/auth_credentials.cpp` | `auth_derive_client_key` |
| AS/TGS/V 认证流程 | `src/roles/client/client_auth_flow.cpp` | `client_auth_connect_to_v_socket` |
| AS 服务端处理 | `src/roles/as/as_service.cpp` | `as_process_connection` |
| TGS 服务端处理 | `src/roles/tgs/tgs_service.cpp` | `tgs_process_connection` |
| Kerberos 消息序列化 | `src/shared/protocol/kerberos_messages.cpp` | 全部 build/parse/encrypt/decrypt 函数 |
| 证书消息序列化 | `src/shared/protocol/certificate_messages.cpp` | `cert_build_c2v_body`, `cert_parse_v2c_body` |
| Packet 构造/解析 | `src/shared/protocol/packet.cpp` | `make_packet`, `serialize_packet`, `parse_packet` |
| DES 加解密 | `src/shared/crypto/crypto.cpp` | `des_encrypt_payload`, `des_decrypt_payload` |
| RSA/证书 | `src/shared/crypto/crypto.cpp` | `make_certificate`, `parse_certificate`, `verify_certificate` |
| 网络收发 | `src/shared/net/net_packet.cpp` | `send_packet_logged`, `recv_packet_logged` |
| TCP 连接 | `src/shared/net/net_socket.cpp` | `connect_tcp` |

---

## 完整认证网络交互图

```
Browser                  client.exe                 AS(9001)        TGS(9002)        V(9003)
  │                          │                         │               │               │
  │-- login JSON ---------->│                         │               │               │
  │   (WebSocket)            │                         │               │               │
  │                          │-- MSG_AS_REQ --------->│               │               │
  │                          │   idc, idtgs, ts1       │               │               │
  │                          │                         │ 验证Client     │               │
  │                          │                         │ 生成Kc_tgs     │               │
  │                          │                         │ 签发Ticket_tgs │               │
  │                          │<-- MSG_AS_REP ---------│               │               │
  │                          │    Kc_tgs, Ticket_tgs   │               │               │
  │                          │                         │               │               │
  │                          │-- MSG_TGS_REQ ----------------------->│               │
  │                          │   idv, Ticket_tgs,      │               │               │
  │                          │   Authenticator_tgs      │               │               │
  │                          │                         │               │ 解密验证       │
  │                          │                         │               │ 生成Kc_v       │
  │                          │                         │               │ 签发Ticket_v   │
  │                          │<-- MSG_TGS_REP -----------------------│               │
  │                          │    Kc_v, Ticket_v        │               │               │
  │                          │                         │               │               │
  │                          │-- MSG_V_AUTH_REQ ----------------------------------->│
  │                          │   Ticket_v, Auth_v       │               │               │
  │                          │<-- MSG_V_AUTH_REP -----------------------------------│
  │                          │   ts5+1                  │               │               │
  │                          │                         │               │               │
  │                          │-- MSG_CERT_C2V ------------------------------------->│
  │                          │   Client证书              │               │               │
  │                          │<-- MSG_CERT_V2C -------------------------------------│
  │                          │   V证书                   │               │               │
  │                          │                         │               │               │
  │                          │========== TCP 长连接已建立 ===========================│
  │                          │   (之后传输 MSG_APP 游戏报文)              │               │
  │                          │                         │               │               │
  │<-- loginState ----------│                         │               │               │
  │    "authenticated"       │                         │               │               │
```
