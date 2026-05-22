# AS 模块学习版：像上课一样理解这个项目

这份文档的目标不是堆知识点，而是让你真的能顺着项目想明白。  
先不急着背 `config`、`payload`、`ticket` 这些词。我们先把它讲成一个故事。

## 0. 先记一句话

AS 做的事情就一句话：

```text
用户说：我是 Client1，我想去找 TGS。
AS 查登记表确认这个 Client 存在，然后发给他一把临时钥匙 Kc_tgs，
再顺手给他一张 Ticket_tgs，让他拿着这张票去找 TGS。
```

所以 AS 模块不是负责整个游戏，也不是负责 TGS/V。  
AS 只负责第一关：

```text
Client 登录
  -> 找 AS
  -> AS 发 Kc_tgs 和 Ticket_tgs
  -> Client 拿这些东西继续找 TGS
```

你先把 AS 想成“学校门口的身份登记处”。  
学生来了说自己是谁，登记处查名单，确认有这个人，就给他一张去教务处的通行证。

## 1. 先把几个词翻译成人话

以后你看到这些词，先不要慌，先按下面这样理解。

| 项目里的词 | 人话理解 | 在代码里大概是什么 |
|---|---|---|
| `config` | 登记表，里面写着服务器地址、端口、各个 Client 的密钥 | `config/course_config.txt` 和 `Config` |
| `Packet` | 信封，写着这封信是什么类型、谁发给谁 | `Packet` 结构 |
| `payload` | 信纸，真正的业务内容 | `Bytes payload` |
| `AS_REQ` | Client 写给 AS 的请求信 | `AsReq` |
| `AS_REP` | AS 回给 Client 的回复信 | `AsRepBody` |
| `Ticket_tgs` | AS 盖章的通行证，给 TGS 看的 | `TicketTgsBody` 加密后 |
| `Kc` | Client 的长期钥匙，由密码算出来 | `auth_derive_client_key()` 或 config 里的 `C1_KC` |
| `Kc_tgs` | Client 和 TGS 后面聊天用的临时钥匙 | `generate_des_key56()` 生成 |
| `KTGS` | AS 和 TGS 之间共享的长期钥匙 | config 里的 `KTGS` |
| `build` | 把结构体写成字节，也就是写信 | `as_build_req()` |
| `parse` | 把字节读回结构体，也就是拆信 | `as_parse_req()` |
| `encrypt` | 上锁 | `des_encrypt_payload()` |
| `decrypt` | 开锁 | `des_decrypt_payload()` |

最重要的是先分清两层：

```text
Packet = 外层信封
payload = 信封里面的信纸
```

比如：

```cpp
make_packet(MsgType::as_req, state.client_id, EntityId::as, as_build_req(...))
```

人话就是：

```text
做一个信封：
  信的类型：AS_REQ
  发件人：Client
  收件人：AS
  信纸内容：as_build_req(...) 生成的字节
```

## 2. 你先只看 AS 相关的一条路

整个项目有 AS、TGS、V、Client。  
但是你现在先不要全看。先看 AS 的一条路：

```text
网页输入 clientId/password
  -> Client 端算出 Kc
  -> Client 发 AS_REQ 给 AS
  -> AS 收到 AS_REQ
  -> AS 查 config 里的 C1_KC
  -> AS 生成 Kc_tgs
  -> AS 生成 Ticket_tgs
  -> AS 生成 AS_REP
  -> AS 用 Kc 加密 AS_REP
  -> Client 用自己算出的 Kc 解开 AS_REP
```

先不要想 TGS 和 V。  
AS 阶段结束时，Client 只需要拿到两个东西：

```text
1. Kc_tgs
   后面跟 TGS 说话用。

2. Ticket_tgs
   后面交给 TGS 的票据。
```

这就是 AS 这关的全部目标。

## 3. 学这个项目要按三层看，不要混在一起

你之前会晕，是因为脑子里同时出现了 `config`、`payload`、`socket`、`ticket`。  
我们把它分成三层。

第一层：配置层。

```text
config/course_config.txt
```

这里是登记表。  
比如：

```text
C1_PASSWORD=123456
C1_KC=0x59ef3db7cb8c8d
KTGS=0x1c24deecc136e
AS_IP=...
AS_PORT=...
```

它回答的问题是：

```text
AS 在哪里？
Client1 的长期密钥是多少？
TGS 的长期密钥是多少？
```

第二层：网络层。

```text
Packet
Socket
send_packet_logged()
recv_packet_logged()
```

这里负责“信怎么发出去、怎么收回来”。  
它关心的是：

```text
这封信是什么类型？
谁发给谁？
信纸有多长？
信纸内容是什么？
```

第三层：业务层。

```text
AsReq
AsRepBody
TicketTgsBody
```

这里才是 Kerberos 逻辑。  
它关心的是：

```text
Client 是谁？
要找哪个 TGS？
当前时间是多少？
票据里放什么？
用哪把钥匙加密？
```

你看代码时也按这个顺序问自己：

```text
这行代码是在读配置？
还是在收发信封？
还是在处理 Kerberos 信纸内容？
```

这样就不会乱。

## 4. 从用户点击登录开始看

入口文件：

```text
src/roles/client/tank_game_client.cpp
函数：TankGameClient::handle_login()
```

这一段是网页点击登录后进入的地方。

关键代码：

```cpp
const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

慢慢翻译：

```text
command.client_id
  -> 网页传来的 Client 编号，比如 Client1。

command.password
  -> 网页传来的密码，比如 123456。

auth_derive_client_key(...)
  -> 用 client_id 和 password 算出 Kc。

client_auth_connect_to_v_socket(...)
  -> 开始完整认证流程。
     虽然函数名说 connect_to_v_socket，但它里面先找 AS，再找 TGS，最后才找 V。
```

这里最容易误解的是 `Kc`。

`Kc` 不是用户直接输入的。  
用户输入的是密码，Client 用密码算出 `Kc`。

```text
正确密码 -> 算出来的 Kc 和 config 里的 C1_KC 一样
错误密码 -> 算出来的 Kc 和 config 里的 C1_KC 不一样
```

所以 Client 不是直接拿 config 里的 `C1_KC`。  
Client 是根据用户输入现场算。

## 5. Client 和 AS 是怎么连上的

你现在看到 `tank_game_client.cpp` 里的这一段：

```cpp
const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

这两行是 Client 认证流程的入口。  
第一行算 `Kc`，第二行开始真正连接 AS/TGS/V。

这里先记住一句：

```text
Client 不是在 tank_game_client.cpp 里直接 connect AS。
tank_game_client.cpp 只是收到网页登录命令，然后调用 client_auth_connect_to_v_socket()。
真正连接 AS 的 socket 在 client_auth_flow.cpp 里。
```

相关文件：

```text
src/roles/client/tank_game_client.cpp
  -> TankGameClient::handle_login()

src/roles/client/client_auth_flow.cpp
  -> client_auth_connect_to_v_socket()
  -> auth_exchange_packet()
  -> connect_tcp()
```

整体路线：

```text
网页点登录
  -> UiBridge 收到 WebSocket JSON
  -> TankGameClient::handle_login()
  -> auth_derive_client_key() 算 Kc
  -> client_auth_connect_to_v_socket()
  -> 构造 AS_REQ
  -> auth_exchange_packet(AS_IP:AS_PORT, AS_REQ)
  -> connect_tcp() 连接 AS
  -> send_packet_logged() 发 AS_REQ
  -> recv_packet_logged() 收 AS_REP
  -> close_socket() 关闭和 AS 的短连接
```

### 5.1 handle_login() 只是把登录流程启动起来

文件：

```text
src/roles/client/tank_game_client.cpp
函数：TankGameClient::handle_login()
```

关键代码：

```cpp
const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

人话：

```text
网页传来 client_id 和 password。
Client 先根据 password 算出 Kc。
然后把 config、client_id、kc 交给 client_auth_connect_to_v_socket()。
```

这里还没有真正发 AS_REQ。  
它只是把“登录认证流程”启动起来。

### 5.2 client_auth_connect_to_v_socket() 里先找 AS

文件：

```text
src/roles/client/client_auth_flow.cpp
函数：client_auth_connect_to_v_socket()
```

虽然函数名叫 `connect_to_v_socket`，但它不是一上来就连 V。  
它里面的顺序是：

```text
1. 先找 AS，拿 Kc_tgs 和 Ticket_tgs。
2. 再找 TGS，拿 Kc_v 和 Ticket_v。
3. 最后才找 V，建立真正的游戏连接。
```

AS 这一段代码是：

```cpp
const std::uint64_t ts1 = auth_time_now_ms();
const Packet as_req =
    make_packet(MsgType::as_req, state.client_id, EntityId::as,
                as_build_req({state.client_id, EntityId::tgs, ts1}));
const Packet as_rep =
    auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
```

分成三步看：

```text
第一步：auth_time_now_ms()
  -> 生成 ts1，也就是 Client 发 AS_REQ 的时间。

第二步：make_packet(... as_build_req(...))
  -> 构造 AS_REQ 这个 Packet。

第三步：auth_exchange_packet(...)
  -> 连接 AS，发送 AS_REQ，等待 AS_REP。
```

### 5.3 config_build_endpoint() 负责拿 AS 的地址

代码：

```cpp
auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
```

先看里面这段：

```cpp
config_build_endpoint(config, "AS_IP", "AS_PORT")
```

函数实现：

```cpp
TcpEndpoint config_build_endpoint(const Config& config, const char* ip_key,
                                  const char* port_key)
{
    return {config.get_string(ip_key), config.get_u16(port_key)};
}
```

人话：

```text
从 config 里读 AS_IP。
从 config 里读 AS_PORT。
合成一个 TcpEndpoint。
```

如果 config 是：

```text
AS_IP=172.27.17.5
AS_PORT=9001
```

那这里得到的 endpoint 就是：

```text
172.27.17.5:9001
```

这个 endpoint 就是 Client 要连接的 AS 地址。

注意区分：

```text
AS_BIND_IP
  -> AS 自己监听时用。

AS_IP
  -> Client 连接 AS 时用。
```

AS 服务端用 `AS_BIND_IP:AS_PORT` 监听。  
Client 用 `AS_IP:AS_PORT` 去连接。

### 5.4 auth_exchange_packet() 里真正创建 socket

文件：

```text
src/roles/client/client_auth_flow.cpp
函数：auth_exchange_packet()
```

代码：

```cpp
Packet auth_exchange_packet(const TcpEndpoint& endpoint, const Packet& request,
                            const ProtocolPayloadView& request_payload_view = {})
{
    SocketHandle socket = connect_tcp(endpoint);
    try
    {
        send_packet_logged(socket, request, request_payload_view);
        Packet response = recv_packet_logged(socket);
        close_socket(socket);
        return response;
    }
    catch (...)
    {
        close_socket(socket);
        throw;
    }
}
```

这一段就是 Client 和 AS 连接的关键。

逐句翻译：

```cpp
SocketHandle socket = connect_tcp(endpoint);
```

人话：

```text
Client 主动连接 AS。
endpoint 就是 AS_IP:AS_PORT。
连接成功后得到 socket。
```

这个 socket 可以理解成：

```text
Client 和 AS 之间临时开的一条电话线。
```

然后：

```cpp
send_packet_logged(socket, request, request_payload_view);
```

人话：

```text
通过这条 socket，把 AS_REQ 发给 AS。
```

然后：

```cpp
Packet response = recv_packet_logged(socket);
```

人话：

```text
通过同一条 socket，等待 AS 回 AS_REP。
```

然后：

```cpp
close_socket(socket);
return response;
```

人话：

```text
AS 阶段是短连接。
发完 AS_REQ，收完 AS_REP，就关闭这条连接。
```

如果中间出错：

```cpp
catch (...)
{
    close_socket(socket);
    throw;
}
```

人话：

```text
就算连接失败、发送失败、接收失败，也要先关闭 socket，再把错误抛出去。
```

### 5.5 AS 这边是谁接住这个 socket

Client 这边是：

```text
connect_tcp(AS_IP:AS_PORT)
send_packet_logged(socket, AS_REQ)
```

AS 这边对应的是：

```text
src/shared/runtime/role_runtime.cpp
runtime_run_auth_server()
```

里面：

```cpp
SocketHandle listener = listen_tcp(endpoint);
SocketHandle accepted = accept_tcp(listener, &peer);
std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
```

人话：

```text
AS 先 listen_tcp() 监听 AS_BIND_IP:AS_PORT。
Client connect_tcp() 连过来后，AS 的 accept_tcp() 会接住这个连接。
接住后得到 accepted socket。
然后 AS 开一个 worker 线程，把 accepted 交给 as_process_connection()。
```

所以 Client 和 AS 的 socket 是一对：

```text
Client 侧 socket：
  connect_tcp(endpoint) 返回的 socket

AS 侧 socket：
  accept_tcp(listener) 返回的 accepted
```

它们表示同一条 TCP 连接的两端。

### 5.6 一定要分清：AS 阶段是短连接，V 阶段是长连接

AS 阶段：

```text
Client 连接 AS
发 AS_REQ
收 AS_REP
close_socket()
```

也就是：

```text
一次请求，一次回复，连接关闭。
```

V 阶段不同。  
Client 最后连上 V 后，会保存：

```cpp
v_socket_ = auth.socket;
```

然后游戏过程中持续用这个 `v_socket_` 发移动、瞄准、射击等消息。

所以：

```text
AS socket 是临时短连接。
V socket 是游戏期间持续使用的长连接。
```

这也是为什么 `auth_exchange_packet()` 里面会 `close_socket(socket)`，而 V 的 socket 会返回给 `TankGameClient` 保存。

## 6. Kc 为什么要 Client 算一次，AS 又从 config 里拿一次

相关文件：

```text
src/shared/auth/auth_credentials.cpp
函数：auth_derive_client_key()

src/shared/config/config.cpp
函数：Config::clients()

src/roles/as/as_service.cpp
函数：as_find_client_secret()
```

先看 Client 这边：

```cpp
const std::string material =
    "client-kc-v1:" + std::to_string(static_cast<int>(client_id)) + ":" + password;
const Bytes bytes(material.begin(), material.end());
std::uint64_t key = hash64(bytes) & 0x00FFFFFFFFFFFFFFULL;
```

人话：

```text
Client 把 client_id 和 password 拼成一段文字。
然后 hash 一下。
最后取 56 位，当作 DES 密钥 Kc。
```

再看 AS 这边：

```cpp
const ClientSecret secret = as_find_client_secret(config, as_req.idc);
```

`as_find_client_secret()` 会从 `config.clients()` 里面找对应的 Client。

`Config::clients()` 会读：

```text
C1_ID
C1_PASSWORD
C1_KC
C2_ID
C2_PASSWORD
C2_KC
...
```

人话：

```text
Client 手里没有直接保存 Kc，它用密码算。
AS 不接收明文密码，它查登记表里的 C1_KC。
如果用户密码正确，Client 算出的 Kc 就等于 AS 查到的 C1_KC。
```

这就像老师和学生各自算同一道题：

```text
学生根据密码算答案。
老师登记表里有标准答案。
学生能不能看懂老师发回来的加密内容，就能证明答案对不对。
```

## 6. Client 怎么写 AS_REQ 这封信

相关文件：

```text
src/roles/client/client_auth_flow.cpp
函数：client_auth_connect_to_v_socket()
```

AS 相关核心代码：

```cpp
const std::uint64_t ts1 = auth_time_now_ms();
const Packet as_req =
    make_packet(MsgType::as_req, state.client_id, EntityId::as,
                as_build_req({state.client_id, EntityId::tgs, ts1}));
const Packet as_rep =
    auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
```

分开理解。

第一行：

```cpp
const std::uint64_t ts1 = auth_time_now_ms();
```

人话：

```text
记下现在时间。
这个时间叫 TS1，表示 Client 发 AS_REQ 的时间。
```

第二段：

```cpp
as_build_req({state.client_id, EntityId::tgs, ts1})
```

人话：

```text
写 AS_REQ 的信纸。
信纸里写三件事：
  1. 我是谁：state.client_id
  2. 我要找谁：EntityId::tgs
  3. 我什么时候发的：ts1
```

第三段：

```cpp
make_packet(MsgType::as_req, state.client_id, EntityId::as, ...)
```

人话：

```text
把信纸装进信封。
信封上写：
  类型：AS_REQ
  发件人：Client
  收件人：AS
```

第四段：

```cpp
config_build_endpoint(config, "AS_IP", "AS_PORT")
```

人话：

```text
从 config 里查 AS 的 IP 和端口。
这一步只是为了知道信往哪里发。
```

第五段：

```cpp
auth_exchange_packet(..., as_req)
```

人话：

```text
连接 AS。
发送 AS_REQ。
等待 AS_REP。
```

所以这一段不要混着看。  
它其实只有三件事：

```text
写信纸 -> 装信封 -> 发给 AS
```

## 7. AS_REQ 的信纸长什么样

结构定义在：

```text
include/cyber/protocol/kerberos_messages.hpp
结构体：AsReq
```

代码：

```cpp
struct AsReq
{
    EntityId idc = EntityId::unknown;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts1 = 0;
};
```

人话：

```text
idc
  -> Client 的身份。
     例如 Client1。

idtgs
  -> Client 想找的 TGS。
     这里一般就是 EntityId::tgs。

ts1
  -> Client 发请求时的时间。
```

把结构体变成字节的函数在：

```text
src/shared/protocol/kerberos_messages.cpp
函数：as_build_req()
```

代码：

```cpp
Bytes as_build_req(const AsReq& value)
{
    Bytes out;
    out.push_back(static_cast<std::uint8_t>(value.idc));
    out.push_back(static_cast<std::uint8_t>(value.idtgs));
    binary_write_u64(out, value.ts1);
    return out;
}
```

人话：

```text
创建一个空字节数组 out。
先写 idc，占 1 字节。
再写 idtgs，占 1 字节。
再写 ts1，占 8 字节。
最后返回这 10 个字节。
```

所以 AS_REQ 的 payload 是：

```text
第 1 字节：idc
第 2 字节：idtgs
后 8 字节：ts1
总共 10 字节
```

AS 收到后，反过来解析：

```cpp
AsReq as_parse_req(const Bytes& payload)
{
    if (payload.size() != 10U)
    {
        throw PacketError("AS_REQ payload must be 10 bytes");
    }
    std::size_t offset = 0;
    AsReq value;
    value.idc = static_cast<EntityId>(payload[offset++]);
    value.idtgs = static_cast<EntityId>(payload[offset++]);
    value.ts1 = binary_read_u64(payload, offset, "payload");
    binary_require_end(payload, offset, "AS_REQ");
    return value;
}
```

人话：

```text
先确认这张信纸是不是 10 字节。
然后从第 1 字节读 idc。
从第 2 字节读 idtgs。
从后 8 字节读 ts1。
最后确认没有多余内容。
```

这就是 build 和 parse 的关系：

```text
build：结构体 -> 字节
parse：字节 -> 结构体
```

## 8. AS 服务是怎么启动到 as_process_connection 的

AS 程序入口：

```text
src/roles/as/main.cpp
```

代码很短：

```cpp
int main(int argc, char** argv) {
    return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
}
```

人话：

```text
这个 main 函数没有自己写 AS 逻辑。
它只是告诉公共启动框架：我要以 AS 角色启动。
```

公共启动框架在：

```text
src/shared/runtime/role_runtime.cpp
函数：run_role_main()
```

这里会做几件事：

```text
1. 根据 RoleKind::as_server 生成 AS 的角色说明 RoleSpec。
2. 解析命令行参数，比如 --config、--serve、--max-connections。
3. 加载 config 文件。
4. 如果是 --serve，就进入 runtime_run_auth_server()。
```

AS 真正开始监听端口在：

```text
src/shared/runtime/role_runtime.cpp
函数：runtime_run_auth_server()
```

人话：

```text
从 config 里拿 AS_BIND_IP 和 AS_PORT。
创建监听 socket。
等 Client 连进来。
每来一个连接，就交给 runtime_handle_server_connection()。
```

真正分发给 AS 处理的地方：

```text
src/shared/runtime/role_runtime.cpp
函数：runtime_handle_server_connection()
```

关键逻辑：

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
```

人话：

```text
如果当前角色是 AS，就把这个连接交给 as_process_connection()。
```

所以 AS 服务端主线是：

```text
main.cpp
  -> run_role_main()
  -> runtime_run_auth_server()
  -> runtime_handle_server_connection()
  -> as_process_connection()
```

你验收时要讲 AS 业务逻辑，重点就落在最后这个函数：

```text
src/roles/as/as_service.cpp
as_process_connection()
```

## 9. as_process_connection 按老师讲课方式拆开

文件：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
```

先看整体，不看细节：

```text
收到 AS_REQ
  -> 确认消息类型是 AS_REQ
  -> 解析 AS_REQ
  -> 查 Client 的长期密钥 Kc
  -> 生成 Kc_tgs
  -> 生成 Ticket_tgs
  -> 生成 AS_REP 明文
  -> 用 Kc 加密 AS_REP
  -> 发回 Client
```

下面逐段看。

### 9.1 收信

代码：

```cpp
const Packet request = recv_packet_logged(socket);
```

人话：

```text
从这个 socket 连接里收一封信。
收到的是完整 Packet，也就是信封加信纸。
```

这里的 `request` 里有：

```text
request.msg_type  -> 信的类型
request.src       -> 谁发的
request.dst       -> 发给谁
request.payload   -> 信纸内容
```

### 9.2 确认这是 AS_REQ

代码：

```cpp
packet_require_msg_type(request, MsgType::as_req);
```

人话：

```text
检查这封信的类型是不是 AS_REQ。
不是 AS_REQ 就抛异常。
```

它检查的是信封上的类型，不是信纸内容。

### 9.3 拆 AS_REQ 信纸

代码：

```cpp
const AsReq as_req = as_parse_req(request.payload);
```

人话：

```text
把 request.payload 这张字节信纸，解析成 AsReq 结构体。
```

解析后你就能用：

```text
as_req.idc
as_req.idtgs
as_req.ts1
```

这一步之后，AS 才真的知道：

```text
这个 Client 说自己是谁
这个 Client 想找哪个 TGS
这个 Client 是什么时候发的请求
```

### 9.4 查 Client 的长期密钥 Kc

代码：

```cpp
const ClientSecret secret = as_find_client_secret(config, as_req.idc);
```

人话：

```text
AS 拿 as_req.idc 去登记表 config 里查这个 Client 的资料。
查到的资料放在 secret 里。
```

`secret` 里主要有：

```text
secret.id
  -> Client 的 ID。

secret.password
  -> 配置里记录的密码。
     AS 当前认证流程里不直接用它。

secret.kc
  -> Client 的长期密钥 Kc。
     AS 后面要用它加密 AS_REP。
```

注意：

```text
AS 没有拿明文 password 比较。
它是用 config 里的 Kc 加密回复。
Client 如果密码错，算不出正确 Kc，就解不开回复。
```

### 9.5 生成 Kc_tgs

代码：

```cpp
const std::uint64_t kc_tgs = generate_des_key56();
```

人话：

```text
AS 随机生成一把临时钥匙。
这把钥匙叫 Kc_tgs。
它后面给 Client 和 TGS 之间通信使用。
```

为什么叫 `Kc_tgs`？

```text
K    -> key
c    -> client
tgs  -> TGS

Kc_tgs = Client 和 TGS 之间共享的会话密钥
```

### 9.6 记录 AS 当前时间 TS2

代码：

```cpp
const std::uint64_t ts2 = auth_time_now_ms();
```

人话：

```text
AS 记录自己发票据的时间。
这个时间叫 TS2。
```

它后面会放进：

```text
Ticket_tgs
AS_REP
```

### 9.7 准备 Ticket_tgs 的明文内容

代码：

```cpp
const TicketTgsBody ticket_body{
    kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
```

人话：

```text
AS 准备一张给 TGS 看的票据。
这张票据暂时还是明文，变量叫 ticket_body。
```

这张票里放了：

```text
kc_tgs
  -> Client 和 TGS 后面共用的临时钥匙。

as_req.idc
  -> 这个票据属于哪个 Client。

kDefaultAdc
  -> Client 地址。这里默认写 127.0.0.1。

EntityId::tgs
  -> 这张票是给 TGS 用的。

ts2
  -> AS 发票时间。

kDefaultLifetimeMs
  -> 票据有效期。
```

这张票据是给 TGS 看的，不是给 Client 看明文的。

### 9.8 用 KTGS 加密 Ticket_tgs

代码：

```cpp
const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
```

人话：

```text
AS 用 KTGS 把 ticket_body 锁起来。
锁起来之后得到 ticket_tgs。
```

为什么用 `KTGS`？

```text
KTGS 是 AS 和 TGS 都知道的长期钥匙。
Client 不知道 KTGS。
所以 Client 拿到 ticket_tgs 也打不开。
Client 只能把它原样交给 TGS。
```

这里 `config.get_u64("KTGS")` 的意思是：

```text
从登记表里拿出 KTGS 这把钥匙。
```

### 9.9 准备 AS_REP 的明文内容

代码：

```cpp
const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
```

人话：

```text
AS 准备回给 Client 的信纸。
这张信纸还没加密，变量叫 rep_body。
```

里面放了：

```text
kc_tgs
  -> 告诉 Client：你后面找 TGS 时用这把临时钥匙。

EntityId::tgs
  -> 告诉 Client：这把钥匙是跟 TGS 用的。

ts2
  -> AS 发票时间。

kDefaultLifetimeMs
  -> 有效期。

ticket_tgs
  -> 这张票给你带着，但你打不开，后面交给 TGS。
```

这里最关键的一点：

```text
AS_REP 里面装着 ticket_tgs。
ticket_tgs 自己又是被 KTGS 加密过的一团字节。
```

所以这是“盒子套盒子”。

```text
AS_REP 明文 [
  Kc_tgs
  idtgs
  ts2
  lifetime2
  ticket_tgs 密文 [
    TicketTgsBody 明文内容，但被 KTGS 锁住
  ]
]
```

### 9.10 把 AS_REP 结构体写成字节

代码：

```cpp
const Bytes rep_plain = as_build_rep_body(rep_body);
```

人话：

```text
把 rep_body 这个结构体写成字节数组。
这个字节数组还没有加密，所以叫 rep_plain。
```

`plain` 的意思就是明文。

### 9.11 用 Client 的 Kc 加密 AS_REP

代码：

```cpp
const Packet response =
    as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                              rep_plain, secret.kc);
```

人话：

```text
AS 做一封回复信。
信的类型是 AS_REP。
发件人是 AS。
收件人是这个 Client。
信纸内容是 rep_plain。
但是装进信封前，用 secret.kc 加密。
```

为什么用 `secret.kc`？

```text
secret.kc 是 AS 从 config 里查到的 Client 长期密钥。
如果 Client 输入密码正确，它自己算出来的 Kc 就等于 secret.kc。
所以 Client 能解开。

如果密码错误，Client 算出来的 Kc 不对。
所以 Client 解不开 AS_REP。
```

### 9.12 写协议日志，方便监视器显示

代码：

```cpp
ProtocolPayloadView view =
    protocol_build_encrypted_payload_view(rep_plain, response.payload);
protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                             tgs_ticket_build_body(ticket_body));
```

人话：

```text
这部分不是认证核心逻辑。
它是为了协议监视器能显示：
  AS_REP 加密前是什么
  AS_REP 加密后是什么
  ticket_tgs 加密前后是什么
```

你可以把它理解为“给老师看的过程记录”。

### 9.13 发回 Client

代码：

```cpp
send_packet_logged(socket, response, view);
close_socket(socket);
```

人话：

```text
把 AS_REP 发回 Client。
顺便写协议日志。
然后关闭这次连接。
```

到这里，AS 的任务结束。

## 10. Client 收到 AS_REP 后做什么

文件：

```text
src/roles/client/client_auth_flow.cpp
函数：client_auth_connect_to_v_socket()
```

AS_REP 相关代码：

```cpp
packet_require_msg_type(as_rep, MsgType::as_rep);
const Bytes as_rep_plain = des_decrypt_payload(as_rep.payload, state.kc);
const AsRepBody as_body = as_parse_rep_body(as_rep_plain);
state.kc_tgs = as_body.kc_tgs;
state.ticket_tgs = as_body.ticket_tgs;
```

人话：

```text
先确认收到的是 AS_REP。
然后用 state.kc 解密 AS_REP。
解密后解析成 AsRepBody。
最后把 kc_tgs 和 ticket_tgs 保存起来。
```

这里的 `state.kc` 从哪来？

```text
从用户输入的 password 算出来。
```

如果密码错了：

```text
state.kc 是错的。
AS_REP 解密出来就是乱的。
后面 parse 会失败。
最终登录失败。
```

如果密码对了：

```text
state.kc 和 AS 用的 secret.kc 一样。
Client 能成功解出 AS_REP。
Client 拿到 kc_tgs 和 ticket_tgs。
```

## 11. 最重要的盒子套盒子图

这张图建议你背下来。

```text
AS 发给 Client 的 AS_REP：

Packet 信封
[
  msg_type = AS_REP
  src = AS
  dst = Client
  payload = 用 Kc 加密的 AS_REP 信纸
]

AS_REP 信纸解密后：
[
  Kc_tgs
  idtgs = TGS
  ts2
  lifetime2
  Ticket_tgs
]

Ticket_tgs 再解密后，TGS 才能看到：
[
  Kc_tgs
  idc = Client
  adc = Client 地址
  idtgs = TGS
  ts2
  lifetime2
]
```

谁能打开什么？

```text
Client 能打开 AS_REP，因为 AS_REP 用 Kc 加密。
Client 不能打开 Ticket_tgs，因为 Ticket_tgs 用 KTGS 加密。
TGS 能打开 Ticket_tgs，因为 TGS 知道 KTGS。
```

这就是 Kerberos 的关键设计：

```text
Client 得到一张票，但不能篡改票。
TGS 能打开票，并相信这是 AS 发的。
```

## 12. 你看代码时的推荐顺序

不要从 `role_runtime.cpp` 一头扎进去。  
它是公共启动框架，先看会晕。

推荐顺序：

第一步，看网页登录入口：

```text
src/roles/client/tank_game_client.cpp
TankGameClient::handle_login()
```

只看它怎么拿到 `client_id/password`，怎么调用 `auth_derive_client_key()`。

第二步，看 Client 怎么发 AS_REQ：

```text
src/roles/client/client_auth_flow.cpp
client_auth_connect_to_v_socket()
```

只看 AS 相关部分，先不要看 TGS/V。

第三步，看报文结构：

```text
include/cyber/protocol/kerberos_messages.hpp
AsReq
TicketTgsBody
AsRepBody
```

第四步，看报文怎么变成字节：

```text
src/shared/protocol/kerberos_messages.cpp
as_build_req()
as_parse_req()
tgs_ticket_build_body()
tgs_ticket_encrypt()
as_build_rep_body()
as_parse_rep_body()
```

第五步，看 AS 真正处理逻辑：

```text
src/roles/as/as_service.cpp
as_process_connection()
```

第六步，最后再看启动框架：

```text
src/roles/as/main.cpp
src/shared/runtime/role_runtime.cpp
```

这样看是从“用户行为”走到“AS 业务”，比较顺。

## 13. 每个文件你应该怎么介绍

### src/roles/client/tank_game_client.cpp

一句话：

```text
这是 Client 游戏端的入口逻辑，网页登录后先到这里。
```

重点函数：

```text
TankGameClient::handle_login()
```

你要会说：

```text
这个函数从 UI 命令里拿到 client_id 和 password。
然后调用 auth_derive_client_key() 算出 Kc。
再调用 client_auth_connect_to_v_socket() 走 AS/TGS/V 认证流程。
```

### src/shared/auth/auth_credentials.cpp

一句话：

```text
这里负责把用户输入的 password 算成长期密钥 Kc。
```

重点函数：

```text
auth_derive_client_key()
```

你要会说：

```text
它把 client_id 和 password 拼成字符串，再 hash 成 56 位密钥。
这样密码不直接传给 AS。
```

### src/roles/client/client_auth_flow.cpp

一句话：

```text
这里是 Client 认证流程的主线，先找 AS，再找 TGS，最后找 V。
```

AS 相关重点：

```text
as_build_req()
auth_exchange_packet()
des_decrypt_payload(as_rep.payload, state.kc)
as_parse_rep_body()
```

你要会说：

```text
Client 在这里构造 AS_REQ，发给 AS。
收到 AS_REP 后，用自己算出的 Kc 解密。
解开后保存 Kc_tgs 和 Ticket_tgs。
```

### include/cyber/protocol/kerberos_messages.hpp

一句话：

```text
这里定义 Kerberos 报文的结构体。
```

重点结构体：

```text
AsReq
TicketTgsBody
AsRepBody
```

你要会说：

```text
这些结构体描述了信纸里有哪些字段。
但它们还不是网络上传的字节。
```

### src/shared/protocol/kerberos_messages.cpp

一句话：

```text
这里负责结构体和字节之间的转换。
```

重点函数：

```text
as_build_req()
as_parse_req()
tgs_ticket_build_body()
tgs_ticket_encrypt()
as_build_rep_body()
as_parse_rep_body()
```

你要会说：

```text
build 是把结构体写成字节。
parse 是把字节读回结构体。
encrypt 是先 build 再加密。
```

### src/roles/as/as_service.cpp

一句话：

```text
这里是 AS 的核心业务逻辑。
```

重点函数：

```text
as_process_connection()
```

你要会说：

```text
它收到 AS_REQ，解析出 Client 身份，查 config 找 Kc，
生成 Kc_tgs，制作 Ticket_tgs，再把 AS_REP 用 Kc 加密发回去。
```

### src/shared/runtime/role_runtime.cpp

一句话：

```text
这里是公共启动框架，不是 AS 独有逻辑。
```

AS 相关重点：

```text
run_role_main()
runtime_run_auth_server()
runtime_handle_server_connection()
```

你要会说：

```text
AS main.cpp 调用 run_role_main()。
run_role_main() 读取配置和参数。
runtime_run_auth_server() 负责监听端口。
收到连接后交给 as_process_connection()。
```

## 14. 验收时最容易被问的几个问题

### 问：AS 是怎么验证身份的？

回答：

```text
AS 收到 AS_REQ 后，从 payload 里解析出 idc。
然后用 idc 去 config.clients() 里找到对应的 ClientSecret。
ClientSecret 里有这个客户端的长期密钥 Kc。
AS 用这个 Kc 加密 AS_REP。
如果用户密码正确，Client 自己算出的 Kc 和 AS 的 Kc 一样，就能解开 AS_REP。
如果密码错，就解不开，所以认证失败。
```

### 问：密码有没有发给 AS？

回答：

```text
没有。
Client 本地用 password 通过 auth_derive_client_key() 算出 Kc。
AS 从 config 里拿 C1_KC。
两边不直接传密码。
```

### 问：Ticket_tgs 为什么 Client 不能改？

回答：

```text
Ticket_tgs 是 AS 用 KTGS 加密的。
Client 不知道 KTGS，所以打不开，也不能重新加密伪造。
Client 只能把它原样交给 TGS。
TGS 知道 KTGS，所以能打开并验证。
```

### 问：AS_REP 里为什么既有 Kc_tgs，又有 Ticket_tgs？

回答：

```text
Kc_tgs 是给 Client 自己用的，后面和 TGS 通信用。
Ticket_tgs 是给 TGS 看的，证明这个 Client 已经经过 AS。
Client 需要两样都拿到，才能进入下一步 TGS_REQ。
```

### 问：config 里的 C1_PASSWORD 有什么用？

回答：

```text
AS 当前认证逻辑主要用 C1_KC，不直接比对 C1_PASSWORD。
C1_PASSWORD 更像配置记录，也被一些测试或批量客户端流程用来重新派生 Kc。
真正决定 AS_REP 能不能被解开的，是 Client 算出的 Kc 是否等于 config 里的 C1_KC。
```

### 问：payload 到底是什么？

回答：

```text
payload 就是 Packet 信封里的信纸内容。
AS_REQ 的 payload 是 AsReq 结构体 build 后的字节。
AS_REP 的 payload 是 AsRepBody build 后再用 Kc 加密的字节。
```

## 15. 你可以这样练一遍讲解

你可以照着这段练：

```text
用户在网页输入 Client1 和密码后，代码先进入 tank_game_client.cpp 的 handle_login()。
这里先用 auth_derive_client_key() 根据 client_id 和 password 算出 Kc。

然后进入 client_auth_flow.cpp 的 client_auth_connect_to_v_socket()。
Client 先记录当前时间 ts1，再用 as_build_req() 生成 AS_REQ 的 payload。
这个 payload 里有 idc、idtgs、ts1。
接着 make_packet() 把 payload 包成一个 AS_REQ Packet，发给 AS_IP:AS_PORT。

AS 服务端启动时，main.cpp 只是调用 run_role_main()。
role_runtime.cpp 负责读 config、监听端口、接收连接。
收到连接后，runtime_handle_server_connection() 会调用 as_service.cpp 里的 as_process_connection()。

as_process_connection() 先 recv_packet_logged() 收到 Packet。
然后检查 msg_type 必须是 AS_REQ。
再用 as_parse_req() 把 payload 解析成 AsReq。
AS 根据 as_req.idc 去 config.clients() 里找到这个客户端的 Kc。

接着 AS 生成临时密钥 Kc_tgs，并生成 TicketTgsBody。
TicketTgsBody 用 KTGS 加密，得到 Ticket_tgs。
然后 AS 把 Kc_tgs、TGS id、时间、有效期、Ticket_tgs 放进 AsRepBody。
AsRepBody build 成明文字节后，再用 Client 的 Kc 加密成 AS_REP payload。
最后 AS 把 AS_REP 发回 Client。

Client 收到 AS_REP 后，用自己刚才根据密码算出的 Kc 解密。
如果密码正确，就能得到 Kc_tgs 和 Ticket_tgs。
如果密码错误，就解不开，登录失败。
```

这段如果你能顺下来，AS 模块主线就已经掌握了。

## 16. 你现在最该背的不是全部代码，而是这 8 个对应关系

```text
网页登录入口
  -> src/roles/client/tank_game_client.cpp
  -> TankGameClient::handle_login()

密码变 Kc
  -> src/shared/auth/auth_credentials.cpp
  -> auth_derive_client_key()

Client 构造 AS_REQ
  -> src/roles/client/client_auth_flow.cpp
  -> client_auth_connect_to_v_socket()

AS_REQ 结构
  -> include/cyber/protocol/kerberos_messages.hpp
  -> AsReq

AS_REQ 字节转换
  -> src/shared/protocol/kerberos_messages.cpp
  -> as_build_req() / as_parse_req()

AS 核心处理
  -> src/roles/as/as_service.cpp
  -> as_process_connection()

AS 查 Kc
  -> src/roles/as/as_service.cpp
  -> as_find_client_secret()

AS 生成回复
  -> src/roles/as/as_service.cpp
  -> TicketTgsBody / AsRepBody / as_build_encrypted_packet()
```

先把这 8 个对应关系记住。  
之后老师让你跳文件，你就不会迷路。
