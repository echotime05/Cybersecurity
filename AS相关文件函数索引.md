# AS 相关文件和函数索引

这份文档专门回答一个问题：

```text
项目里哪些文件、哪些函数和 AS 有关？
它们分别负责 AS 流程里的哪一段？
```

先记住 AS 主线：

```text
AS 启动：
src/roles/as/main.cpp
  -> src/shared/runtime/role_runtime.cpp
  -> runtime_run_auth_server()
  -> runtime_handle_server_connection()
  -> src/roles/as/as_service.cpp
  -> as_process_connection()

Client 找 AS：
src/roles/client/tank_game_client.cpp
  -> auth_derive_client_key()
  -> src/roles/client/client_auth_flow.cpp
  -> as_build_req()
  -> auth_exchange_packet(AS_IP:AS_PORT, AS_REQ)

AS 处理：
as_process_connection()
  -> recv_packet_logged()
  -> as_parse_req()
  -> as_find_client_secret()
  -> generate_des_key56()
  -> tgs_ticket_encrypt()
  -> as_build_rep_body()
  -> as_build_encrypted_packet()
  -> send_packet_logged()
```

## 1. 最核心必看的 AS 文件

| 文件 | 作用 | 你要重点看的函数 |
|---|---|---|
| `src/roles/as/main.cpp` | AS 可执行程序入口 | `main()` |
| `include/cyber/roles/as/as_service.hpp` | AS 服务对外声明 | `as_process_connection()` |
| `src/roles/as/as_service.cpp` | AS 核心业务：收 AS_REQ、发 AS_REP | `as_process_connection()` 和内部 helper |
| `src/shared/runtime/role_runtime.cpp` | AS 启动、监听、多线程分发 | `run_role_main()`、`runtime_run_auth_server()`、`runtime_handle_server_connection()` |
| `include/cyber/protocol/kerberos_messages.hpp` | AS_REQ/AS_REP/Ticket_tgs 结构定义 | `AsReq`、`TicketTgsBody`、`AsRepBody` |
| `src/shared/protocol/kerberos_messages.cpp` | AS 相关报文 build/parse/encrypt | `as_build_req()`、`as_parse_req()`、`as_build_rep_body()` |
| `src/roles/client/client_auth_flow.cpp` | Client 发 AS_REQ、收 AS_REP | `client_auth_connect_to_v_socket()`、`auth_exchange_packet()` |

## 2. AS 启动相关

### src/roles/as/main.cpp

作用：

```text
AS 程序入口。它自己不写 AS_REQ 处理逻辑，只把角色交给公共 runtime。
```

函数：

```cpp
int main(int argc, char** argv)
```

关键调用：

```cpp
return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
```

和 AS 的关系：

```text
告诉 role_runtime：当前启动的是 AS 角色。
```

## 3. AS runtime 启动和多线程相关

### src/shared/runtime/role_runtime.cpp

这个文件和 AS 有关，但它不是 AS 业务逻辑。  
它负责 AS 怎么启动、怎么监听端口、怎么开线程处理连接。

### runtime_build_role_spec()

作用：

```text
根据 RoleKind 生成角色说明书 RoleSpec。
```

AS 分支：

```cpp
case RoleKind::as_server:
    return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
            "handle AS_REQ and return AS_REP"};
```

和 AS 的关系：

```text
告诉 runtime：
AS 的 ID 从 AS_ID 读；
AS 端口从 AS_PORT 读；
AS 监听地址从 AS_BIND_IP 读；
别人连接 AS 用 AS_IP。
```

### runtime_print_usage()

作用：

```text
打印启动命令格式。
```

和 AS 的关系：

```text
当 AS 参数写错或使用 --help 时，会打印 as_server 的用法。
```

### config_find_default_path()

作用：

```text
如果命令行没传 --config，就找默认 config/course_config.txt。
```

和 AS 的关系：

```text
AS 启动时需要 config 里的 AS_ID、AS_PORT、AS_BIND_IP、KTGS、Client Kc 等。
```

### config_print_endpoint()

作用：

```text
打印某个角色的 connect/listen 地址。
```

和 AS 的关系：

```text
--print-config 时会打印：
AS connect: AS_IP:AS_PORT
AS listen:  AS_BIND_IP:AS_PORT
```

### config_print_deployment()

作用：

```text
打印整体部署配置。
```

和 AS 的关系：

```text
会调用 config_print_endpoint(config, "AS", "AS_IP", "AS_BIND_IP", "AS_PORT")。
```

### net_format_endpoint()

作用：

```text
把 TcpEndpoint 转成 "ip:port" 字符串。
```

和 AS 的关系：

```text
AS 启动后打印 AS listening on 0.0.0.0:9001 这类信息。
```

### runtime_bind_endpoint()

作用：

```text
根据 RoleSpec 从 config 里取监听 IP 和端口。
```

AS 实际读取：

```text
AS_BIND_IP
AS_PORT
```

和 AS 的关系：

```text
AS 监听哪个地址、哪个端口，就是这个函数给 listen_tcp() 准备的。
```

### runtime_handle_server_connection()

作用：

```text
worker 线程里的分发函数。
```

AS 分支：

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
```

和 AS 的关系：

```text
每个 AS 连接最终都会从这里转到 as_process_connection()。
```

### runtime_run_auth_server()

作用：

```text
AS/TGS 的服务器监听和多线程核心。
```

关键代码：

```cpp
SocketHandle listener = listen_tcp(endpoint);
SocketHandle accepted = accept_tcp(listener, &peer);
std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
```

和 AS 的关系：

```text
AS 的多线程在这里实现。
主线程负责 listen/accept。
每接到一个 Client 连接，就创建 worker 线程。
worker 线程再调用 as_process_connection()。
```

### run_role_main()

作用：

```text
四个角色共用的总入口。
```

和 AS 的关系：

```text
AS main.cpp 调用它。
它解析 --config、--serve、--max-connections。
当 serve=true 时，调用 runtime_run_auth_server() 启动 AS 监听。
```

AS 常用启动：

```powershell
.\as_server.exe --config .\config\course_config.txt --serve
```

## 4. AS 核心处理相关

### include/cyber/roles/as/as_service.hpp

作用：

```text
声明 AS 对外提供的处理函数。
```

函数：

```cpp
void as_process_connection(SocketHandle socket, const Config& config);
```

和 AS 的关系：

```text
role_runtime.cpp 通过这个声明调用 AS 业务处理函数。
```

### src/roles/as/as_service.cpp

这是 AS 最核心文件。

### kDefaultAdc

```cpp
constexpr std::uint32_t kDefaultAdc = 0x7F000001U;
```

作用：

```text
默认 Client 地址，0x7F000001 对应 127.0.0.1。
```

AS 里用在：

```cpp
TicketTgsBody ticket_body{kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
```

### kDefaultLifetimeMs

```cpp
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;
```

作用：

```text
Ticket_tgs 默认有效期，当前是 5 分钟。
```

AS 里用在：

```text
TicketTgsBody.lifetime2
AsRepBody.lifetime2
```

### auth_time_now_ms()

作用：

```text
获取当前系统时间，单位毫秒。
```

AS 里用来生成：

```text
ts2：AS 签发票据的时间。
```

### as_find_client_secret()

函数：

```cpp
ClientSecret as_find_client_secret(const Config& config, EntityId id)
```

作用：

```text
根据 Client ID 到 config.clients() 里找对应 ClientSecret。
```

返回内容里重要的是：

```text
secret.id
secret.password
secret.kc
```

AS 真正用的是：

```text
secret.kc
```

用来加密 AS_REP。

### as_build_encrypted_packet()

函数：

```cpp
Packet as_build_encrypted_packet(MsgType type, EntityId src, EntityId dst,
                                 const Bytes& plain, std::uint64_t key)
```

作用：

```text
把明文 plain 用 key 加密后，封装成 Packet。
```

AS 里用于：

```text
用 Client 的 Kc 加密 AS_REP。
```

关键逻辑：

```cpp
return make_packet(type, src, dst, des_encrypt_payload(plain, key));
```

### protocol_build_encrypted_payload_view()

作用：

```text
生成协议监视器需要的明文/密文对照信息。
```

和 AS 的关系：

```text
让 monitor 能看到 AS_REP 加密前后的内容。
```

注意：

```text
这是日志展示辅助，不是认证必须逻辑。
```

### protocol_add_encrypted_field()

作用：

```text
给协议监视器补充嵌套字段的明文/密文。
```

AS 里用于：

```cpp
protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                             tgs_ticket_build_body(ticket_body));
```

也就是让 monitor 能显示：

```text
ticket_tgs 密文
TicketTgsBody 明文
```

### packet_require_msg_type()

作用：

```text
检查收到的 Packet 类型是不是预期类型。
```

AS 里检查：

```cpp
packet_require_msg_type(request, MsgType::as_req);
```

也就是：

```text
AS 只接受 MSG_AS_REQ。
```

### as_process_connection()

函数：

```cpp
void as_process_connection(SocketHandle socket, const Config& config)
```

作用：

```text
处理一个 Client 到 AS 的 TCP 连接。
```

AS 主流程：

```text
1. recv_packet_logged(socket)
   收到 Client 发来的 Packet。

2. packet_require_msg_type(request, MsgType::as_req)
   确认消息类型是 AS_REQ。

3. as_parse_req(request.payload)
   把 AS_REQ payload 拆成 AsReq。

4. as_find_client_secret(config, as_req.idc)
   根据 Client ID 找 Kc。

5. generate_des_key56()
   生成 Kc_tgs。

6. auth_time_now_ms()
   生成 ts2。

7. TicketTgsBody ticket_body{...}
   准备 Ticket_tgs 明文。

8. tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"))
   用 KTGS 加密 Ticket_tgs。

9. AsRepBody rep_body{...}
   准备 AS_REP 明文结构体。

10. as_build_rep_body(rep_body)
    把 AS_REP 结构体写成明文字节 rep_plain。

11. as_build_encrypted_packet(..., rep_plain, secret.kc)
    用 Client 的 Kc 加密 AS_REP，封成 Packet。

12. send_packet_logged(socket, response, view)
    发回 Client，并记录协议日志。

13. close_socket(socket)
    关闭这次连接。
```

## 5. Client 发 AS_REQ、收 AS_REP 相关

### src/roles/client/tank_game_client.cpp

### TankGameClient::handle_login()

作用：

```text
网页点击登录后，Client 进入这个函数。
```

和 AS 的关系：

```cpp
const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
VAuthenticatedSocket auth =
    client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

人话：

```text
先用密码算 Kc。
再进入 client_auth_connect_to_v_socket()，里面第一步就是找 AS。
```

### include/cyber/roles/client/client_auth_flow.hpp

### AuthClientState

和 AS 有关字段：

```cpp
EntityId client_id;
std::uint64_t kc;
std::uint64_t kc_tgs;
Bytes ticket_tgs;
```

字段含义：

| 字段 | 和 AS 的关系 |
|---|---|
| `client_id` | Client 自己是谁，放进 AS_REQ |
| `kc` | 用户密码派生出的 Kc，用来解 AS_REP |
| `kc_tgs` | AS_REP 解出来的临时钥匙 |
| `ticket_tgs` | AS_REP 里带回来的 TGS 票据 |

### client_auth_connect_to_v_socket()

声明：

```cpp
VAuthenticatedSocket client_auth_connect_to_v_socket(const Config& config,
                                                     EntityId client_id,
                                                     std::uint64_t kc);
```

和 AS 的关系：

```text
这是 Client 完整认证流程函数。
它第一步就是构造 AS_REQ，连接 AS，接收 AS_REP。
```

### src/roles/client/client_auth_flow.cpp

### auth_time_now_ms()

作用：

```text
Client 生成 ts1。
```

AS_REQ 里会带：

```text
ts1 = Client 发请求时间。
```

### config_build_endpoint()

函数：

```cpp
TcpEndpoint config_build_endpoint(const Config& config, const char* ip_key,
                                  const char* port_key)
```

AS 阶段调用：

```cpp
config_build_endpoint(config, "AS_IP", "AS_PORT")
```

作用：

```text
从 config 里拿 AS_IP 和 AS_PORT，组成 Client 连接 AS 的地址。
```

### packet_require_msg_type()

Client 侧作用：

```text
确认 AS 返回的是 MSG_AS_REP。
```

AS 阶段调用：

```cpp
packet_require_msg_type(as_rep, MsgType::as_rep);
```

### auth_exchange_packet()

函数：

```cpp
Packet auth_exchange_packet(const TcpEndpoint& endpoint, const Packet& request,
                            const ProtocolPayloadView& request_payload_view = {})
```

作用：

```text
短连接请求响应：连接服务器、发 Packet、收 Packet、关闭连接。
```

AS 阶段：

```text
connect_tcp(AS_IP:AS_PORT)
send_packet_logged(socket, AS_REQ)
recv_packet_logged(socket) 得到 AS_REP
close_socket(socket)
```

关键代码：

```cpp
SocketHandle socket = connect_tcp(endpoint);
send_packet_logged(socket, request, request_payload_view);
Packet response = recv_packet_logged(socket);
close_socket(socket);
```

### client_auth_connect_to_v_socket()

AS 相关代码：

```cpp
const std::uint64_t ts1 = auth_time_now_ms();
const Packet as_req =
    make_packet(MsgType::as_req, state.client_id, EntityId::as,
                as_build_req({state.client_id, EntityId::tgs, ts1}));
const Packet as_rep =
    auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
packet_require_msg_type(as_rep, MsgType::as_rep);
const Bytes as_rep_plain = des_decrypt_payload(as_rep.payload, state.kc);
const AsRepBody as_body = as_parse_rep_body(as_rep_plain);
state.kc_tgs = as_body.kc_tgs;
state.ticket_tgs = as_body.ticket_tgs;
```

作用：

```text
构造 AS_REQ。
连接 AS。
收到 AS_REP。
用 Kc 解密 AS_REP。
解析出 Kc_tgs 和 Ticket_tgs。
```

## 6. 密码、Kc、config 相关

### src/shared/auth/auth_credentials.cpp

### auth_derive_client_key()

函数：

```cpp
std::uint64_t auth_derive_client_key(EntityId client_id, const std::string& password)
```

作用：

```text
Client 根据用户输入的 password 算 Kc。
```

和 AS 的关系：

```text
AS 用 config 里的 C1_KC 加密 AS_REP。
Client 用 auth_derive_client_key() 算出的 Kc 解 AS_REP。
密码正确时，两边 Kc 一样。
密码错误时，Client 解不开 AS_REP。
```

### src/shared/config/config.cpp

### Config::load()

作用：

```text
读取 config/course_config.txt。
```

AS 需要 config 里的：

```text
AS_ID
AS_BIND_IP
AS_IP
AS_PORT
C1_ID / C1_PASSWORD / C1_KC
C2_ID / C2_PASSWORD / C2_KC
C3_ID / C3_PASSWORD / C3_KC
C4_ID / C4_PASSWORD / C4_KC
KTGS
LOG_ROOT
```

### Config::get_string()

AS 相关用途：

```text
读取 AS_BIND_IP、AS_IP、LOG_ROOT 等字符串。
```

### Config::get_u16()

AS 相关用途：

```text
读取 AS_PORT。
```

### Config::get_u64()

AS 相关用途：

```text
读取 KTGS、C1_KC 等 64 位密钥。
```

### Config::get_entity_id()

AS 相关用途：

```text
读取 AS_ID、C1_ID 等实体 ID。
```

### Config::clients()

作用：

```text
把 C1/C2/C3/C4 的 ID、password、Kc 读成 ClientSecret 列表。
```

AS 里调用链：

```text
as_find_client_secret()
  -> config.clients()
  -> 找到 as_req.idc 对应的 secret.kc
```

### config/course_config.txt

AS 直接相关 key：

```text
AS_ID
AS_BIND_IP
AS_IP
AS_HOST
AS_PORT
LOG_ROOT
KTGS
C1_ID/C1_PASSWORD/C1_KC
C2_ID/C2_PASSWORD/C2_KC
C3_ID/C3_PASSWORD/C3_KC
C4_ID/C4_PASSWORD/C4_KC
```

## 7. Kerberos 报文结构相关

### include/cyber/protocol/kerberos_messages.hpp

这个文件定义 AS 相关“信纸结构”。

### AsReq

```cpp
struct AsReq
{
    EntityId idc;
    EntityId idtgs;
    std::uint64_t ts1;
};
```

作用：

```text
Client 发给 AS 的请求正文。
```

字段：

| 字段 | 含义 |
|---|---|
| `idc` | Client 是谁 |
| `idtgs` | Client 想找哪个 TGS |
| `ts1` | Client 发请求的时间 |

### TicketTgsBody

```cpp
struct TicketTgsBody
{
    std::uint64_t kc_tgs;
    EntityId idc;
    std::uint32_t adc;
    EntityId idtgs;
    std::uint64_t ts2;
    std::uint64_t lifetime2;
};
```

作用：

```text
AS 给 TGS 看的票据明文。
```

AS 会用：

```text
KTGS
```

把它加密成：

```text
ticket_tgs
```

### AsRepBody

```cpp
struct AsRepBody
{
    std::uint64_t kc_tgs;
    EntityId idtgs;
    std::uint64_t ts2;
    std::uint64_t lifetime2;
    Bytes ticket_tgs;
};
```

作用：

```text
AS 回给 Client 的 AS_REP 明文结构。
```

之后会用：

```text
Client 的 Kc
```

加密成 AS_REP payload。

### AS 相关函数声明

```cpp
Bytes as_build_req(const AsReq& value);
AsReq as_parse_req(const Bytes& payload);
Bytes tgs_ticket_build_body(const TicketTgsBody& value);
TicketTgsBody tgs_ticket_parse_body(const Bytes& payload);
Bytes tgs_ticket_encrypt(const TicketTgsBody& value, std::uint64_t ktgs);
TicketTgsBody tgs_ticket_decrypt(const Bytes& cipher, std::uint64_t ktgs);
Bytes as_build_rep_body(const AsRepBody& value);
AsRepBody as_parse_rep_body(const Bytes& payload);
```

## 8. Kerberos 报文 build/parse 实现相关

### src/shared/protocol/kerberos_messages.cpp

### as_build_req()

作用：

```text
Client 把 AsReq 结构体写成 AS_REQ payload 字节。
```

字节顺序：

```text
idc   1 字节
idtgs 1 字节
ts1   8 字节
总共 10 字节
```

调用位置：

```text
src/roles/client/client_auth_flow.cpp
```

### as_parse_req()

作用：

```text
AS 把 request.payload 解析成 AsReq。
```

调用位置：

```text
src/roles/as/as_service.cpp
```

它会检查：

```cpp
if (payload.size() != 10U)
```

也就是 AS_REQ payload 必须是 10 字节。

### tgs_ticket_build_body()

作用：

```text
把 TicketTgsBody 明文结构体写成字节。
```

AS 用在两个地方：

```text
1. tgs_ticket_encrypt() 内部先 build 再加密。
2. protocol_add_encrypted_field() 里给 monitor 展示 ticket_tgs 明文。
```

### tgs_ticket_parse_body()

作用：

```text
把 Ticket_tgs 解密后的字节读回 TicketTgsBody。
```

严格说它主要由 TGS 使用。  
但因为 AS 生成的 Ticket_tgs 必须能被 TGS parse，所以它也和 AS 产物有关。

### tgs_ticket_encrypt()

作用：

```text
AS 用 KTGS 加密 TicketTgsBody，生成 ticket_tgs。
```

调用位置：

```text
src/roles/as/as_service.cpp
```

### tgs_ticket_decrypt()

作用：

```text
TGS 用 KTGS 解密 AS 生成的 ticket_tgs。
```

调用位置：

```text
src/roles/tgs/tgs_service.cpp
```

它不是 AS 代码直接调用，但它验证 AS 生成的票能不能被 TGS 打开。

### as_build_rep_body()

作用：

```text
AS 把 AsRepBody 结构体写成 AS_REP 明文字节 rep_plain。
```

字节内容：

```text
kc_tgs
idtgs
ts2
lifetime2
ticket_tgs
```

调用位置：

```text
src/roles/as/as_service.cpp
```

### as_parse_rep_body()

作用：

```text
Client 解密 AS_REP 后，把明文字节解析成 AsRepBody。
```

调用位置：

```text
src/roles/client/client_auth_flow.cpp
```

## 9. Packet 和消息类型相关

### include/cyber/shared/types.hpp

### EntityId

AS 相关：

```cpp
EntityId::as = 0x11
EntityId::tgs = 0x12
EntityId::client1 = 0x01
...
```

作用：

```text
Packet 的 src/dst 和 Kerberos 字段 idc/idtgs 都用这个枚举表示身份。
```

### MsgType

AS 相关：

```cpp
MsgType::as_req = 1
MsgType::as_rep = 2
```

作用：

```text
Packet.msg_type 用它区分 AS_REQ 和 AS_REP。
```

### is_client()

AS 相关：

```text
可以判断一个 EntityId 是不是 Client。
当前 AS 代码没有显式调用它检查 request.src，但现场可作为增强点。
```

### to_string()

AS 相关：

```text
协议日志、调试输出、monitor 显示 AS/Client1 等名字时会用。
```

### include/cyber/protocol/packet.hpp

### Packet

结构：

```cpp
struct Packet
{
    MsgType msg_type;
    EntityId src;
    EntityId dst;
    std::uint32_t reserved;
    Bytes payload;
};
```

和 AS 的关系：

```text
AS_REQ 和 AS_REP 外层都是 Packet。
payload 里面才是 AsReq 或加密后的 AsRepBody。
```

### make_packet()

作用：

```text
创建 Packet。
```

AS 使用方式：

```text
as_build_encrypted_packet() 内部调用 make_packet() 创建 AS_REP。
Client 创建 AS_REQ 时也调用 make_packet()。
```

### serialize_packet() / parse_packet()

作用：

```text
Packet 和网络字节之间互相转换。
```

AS 间接使用：

```text
send_packet_logged() 内部 serialize_packet()
recv_packet_logged() 内部 parse_packet()
```

### src/shared/protocol/packet.cpp

和 AS 有关的实现：

| 函数 | AS 关系 |
|---|---|
| `make_packet()` | 创建 AS_REQ/AS_REP 外层 Packet |
| `packet_header()` | 根据 payload.size() 生成包头 |
| `serialize_packet()` | AS_REP 发送前转成网络字节 |
| `parse_packet()` | AS_REQ 接收后转成 Packet |
| `bytes_to_hex()` | 协议日志展示明文/密文十六进制 |

## 10. TCP 收发相关

### include/cyber/shared/net_packet.hpp

AS 相关函数：

```cpp
void close_socket(SocketHandle socket);
bool send_packet_logged(SocketHandle socket, const Packet& packet);
bool send_packet_logged(SocketHandle socket, const Packet& packet,
                        const ProtocolPayloadView& payload_view);
Packet recv_packet_logged(SocketHandle socket);
```

### src/shared/net/net_packet.cpp

### send_packet_logged()

作用：

```text
把 Packet 序列化后通过 socket 发出去，并写协议事件日志。
```

AS 调用：

```cpp
send_packet_logged(socket, response, view);
```

### recv_packet_logged()

作用：

```text
从 socket 读取 Packet，并写协议事件日志。
```

AS 调用：

```cpp
const Packet request = recv_packet_logged(socket);
```

### close_socket()

作用：

```text
关闭 socket。
```

AS 正常处理完和异常时都会关闭连接。

### include/cyber/shared/net_socket.hpp / src/shared/net/net_socket.cpp

AS 相关函数：

| 函数 | AS 关系 |
|---|---|
| `listen_tcp()` | AS 服务端监听 AS_BIND_IP:AS_PORT |
| `accept_tcp()` | AS 主线程接收 Client 连接 |
| `connect_tcp()` | Client 连接 AS_IP:AS_PORT |

注意：

```text
AS 服务端用 listen_tcp/accept_tcp。
Client 找 AS 用 connect_tcp。
```

## 11. 加密相关

### include/cyber/shared/crypto.hpp / src/shared/crypto/crypto.cpp

AS 相关函数：

| 函数 | AS 关系 |
|---|---|
| `generate_des_key56()` | AS 生成 Kc_tgs |
| `des_encrypt_payload()` | AS 加密 AS_REP、Ticket_tgs |
| `des_decrypt_payload()` | Client 解 AS_REP，TGS 解 Ticket_tgs |
| `hash64()` | Client 从 password 派生 Kc 时用 |

AS 里的直接调用：

```text
generate_des_key56()
des_encrypt_payload() 通过 as_build_encrypted_packet() 间接调用
tgs_ticket_encrypt() 内部调用 des_encrypt_payload()
```

## 12. 协议监视器日志相关

### include/cyber/protocol/protocol_event.hpp / src/shared/protocol/protocol_event.cpp

AS 相关概念：

```text
ProtocolPayloadView
write_protocol_event()
ProtocolDirection::send / recv
```

AS 关系：

```text
recv_packet_logged() 收到 AS_REQ 时写 RECV 日志。
send_packet_logged() 发送 AS_REP 时写 SEND 日志。
AS 自己额外传入 ProtocolPayloadView，让 monitor 能看到 AS_REP 明文/密文和 ticket_tgs 字段。
```

### src/roles/monitor/protocol_monitor.cpp

作用：

```text
读取 _generated/logs/protocol_events 里的事件，推给网页 monitor。
```

和 AS 的关系：

```text
它不参与 AS 认证，只负责展示 AS_REQ/AS_REP 日志。
```

## 13. TGS 中和 AS 产物有关的函数

严格说 TGS 不是 AS，但它会验证 AS 生成的 Ticket_tgs。  
所以如果你讲 AS 票据能不能被使用，需要知道这几个。

### src/roles/tgs/tgs_service.cpp

AS 产物相关代码：

```cpp
const TicketTgsBody ticket =
    tgs_ticket_decrypt(tgs_req.ticket_tgs, config.get_u64("KTGS"));
const AuthenticatorBody authenticator =
    authenticator_decrypt(tgs_req.authenticator_tgs, ticket.kc_tgs);
```

和 AS 的关系：

```text
AS 用 KTGS 加密 Ticket_tgs。
TGS 用同一个 KTGS 解密 Ticket_tgs。
AS 放进去的 kc_tgs，被 TGS 取出来后用于解 authenticator_tgs。
```

这能证明：

```text
AS 生成的 ticket_tgs 不是摆设，它是下一步 TGS_REQ 的核心输入。
```

## 14. 测试文件里和 AS 有关的部分

### tests/auth_payload_selftest.cpp

作用：

```text
测试 AS_REQ、Ticket_tgs、AS_REP 等认证 payload 的 build/parse/encrypt/decrypt 是否能跑通。
```

AS 相关：

```text
AsReq
as_build_req()
as_parse_req()
TicketTgsBody
tgs_ticket_encrypt()
tgs_ticket_decrypt()
AsRepBody
as_build_rep_body()
as_parse_rep_body()
```

### tests/protocol_selftest.cpp

作用：

```text
测试 MsgType、EntityId、config 基础字段。
```

AS 相关：

```text
MsgType::as_req / as_rep
AS_ID
AS_PORT
AS_BIND_IP
AS_IP
```

### tests/protocol_event_selftest.cpp

作用：

```text
测试协议事件日志和 payload field 展示。
```

AS 相关：

```text
ticket_tgs 字段显示。
```

### tests/protocol_monitor_tail_selftest.cpp

作用：

```text
测试 monitor 读取 AS_REQ/AS_REP 日志顺序和展示。
```

AS 相关：

```text
MSG_AS_REQ
MSG_AS_REP
AS->Client1
Client1->AS
```

### tests/auth_encrypted_game_flow_selftest.ps1

作用：

```text
完整链路测试，启动 AS/TGS/V/Client。
```

AS 相关：

```text
Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '3')
```

## 15. 按学习顺序推荐你看这些文件

不要一上来就看所有 shared 文件。  
建议顺序：

```text
1. src/roles/client/tank_game_client.cpp
   看 handle_login() 怎么进入认证。

2. src/roles/client/client_auth_flow.cpp
   看 Client 怎么构造 AS_REQ，怎么收 AS_REP。

3. include/cyber/protocol/kerberos_messages.hpp
   看 AsReq / TicketTgsBody / AsRepBody 字段。

4. src/shared/protocol/kerberos_messages.cpp
   看这些结构怎么 build/parse。

5. src/roles/as/as_service.cpp
   看 AS 怎么处理 AS_REQ 并返回 AS_REP。

6. src/shared/runtime/role_runtime.cpp
   看 AS 怎么启动、多线程怎么调用 as_process_connection()。

7. src/shared/config/config.cpp
   看 AS 怎么从 config 找 Kc、KTGS、IP、端口。

8. src/shared/net/net_packet.cpp
   看 Packet 怎么真正通过 socket 收发。
```

## 16. 一句话版总结

```text
AS 的启动在 role_runtime.cpp；
AS 的核心处理在 as_service.cpp；
AS_REQ/AS_REP/Ticket_tgs 的结构在 kerberos_messages.hpp；
这些结构和字节互转在 kerberos_messages.cpp；
Client 发 AS_REQ 和收 AS_REP 在 client_auth_flow.cpp；
Kc 来自 auth_credentials.cpp 和 config.cpp；
真正网络收发在 net_packet.cpp / net_socket.cpp；
协议展示在 protocol_event.cpp 和 monitor。
```

