# AS 验收现场可能改代码清单

这份文档不讲完整原理，只整理“老师现场可能让我改哪里”。  
读法是：先看老师问法，再看对应文件和函数，最后看最小改法。

## 0. 先记住 AS 相关代码主线

AS 服务端启动线：

```text
src/roles/as/main.cpp
  -> cyber::run_role_main(RoleKind::as_server, argc, argv)
  -> src/shared/runtime/role_runtime.cpp
  -> runtime_run_auth_server()
  -> runtime_handle_server_connection()
  -> src/roles/as/as_service.cpp
  -> as_process_connection()
```

Client 登录请求 AS 线：

```text
src/roles/client/tank_game_client.cpp
  -> TankGameClient::handle_login()
  -> auth_derive_client_key()
  -> client_auth_connect_to_v_socket()
  -> src/roles/client/client_auth_flow.cpp
  -> as_build_req()
  -> auth_exchange_packet()
  -> AS 收到 AS_REQ
```

报文结构线：

```text
include/cyber/protocol/kerberos_messages.hpp
  -> 定义 AsReq / TicketTgsBody / AsRepBody 这些结构体

src/shared/protocol/kerberos_messages.cpp
  -> as_build_req() / as_parse_req()
  -> tgs_ticket_build_body() / tgs_ticket_encrypt()
  -> as_build_rep_body() / as_parse_rep_body()
```

## 1. 高概率：让 AS 校验 AS_REQ 里的身份是否合理

老师可能问：

```text
AS 收到请求后有没有检查这个请求真的是发给 AS 的？
AS_REQ 里的 idc 和报文头 src 不一致怎么办？
客户端请求的 idtgs 不是 TGS 怎么办？
```

要改的地方：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
位置：const AsReq as_req = as_parse_req(request.payload); 后面
```

现在代码大概是：

```cpp
const Packet request = recv_packet_logged(socket);
packet_require_msg_type(request, MsgType::as_req);

const AsReq as_req = as_parse_req(request.payload);
const ClientSecret secret = as_find_client_secret(config, as_req.idc);
```

可以加的检查：

```cpp
if (request.dst != EntityId::as)
{
    throw std::runtime_error("AS_REQ destination must be AS");
}
if (!is_client(request.src))
{
    throw std::runtime_error("AS_REQ source must be a client");
}
if (request.src != as_req.idc)
{
    throw std::runtime_error("AS_REQ source does not match idc");
}
if (as_req.idtgs != EntityId::tgs)
{
    throw std::runtime_error("AS_REQ idtgs must be TGS");
}
```

人话解释：

`request.src` 是报文头里写的“谁发来的”。  
`request.dst` 是报文头里写的“发给谁”。  
`as_req.idc` 是 AS_REQ 正文里写的“客户端是谁”。  
`as_req.idtgs` 是 AS_REQ 正文里写的“客户端想找哪个 TGS”。  

所以这个改动是在防止“信封上写 Client1，信里面写 Client2”这种不一致。

验收时可以这样说：

```text
我这里先解析外层 Packet，再解析 AS_REQ 正文。
外层 src/dst 负责网络包身份，正文 idc/idtgs 负责 Kerberos 业务身份。
两层身份必须对得上，否则 AS 不应该继续发票据。
```

## 2. 高概率：把 Ticket_tgs 有效期改成 1 分钟或 10 分钟

老师可能问：

```text
你把票据有效期改短一点。
Ticket_tgs 的 lifetime 是在哪里定的？
AS_REP 里的 lifetime 和 Ticket_tgs 里的 lifetime 是不是同一个？
```

要改的地方：

```text
src/roles/as/as_service.cpp
变量：kDefaultLifetimeMs
```

现在：

```cpp
constexpr std::uint64_t kDefaultLifetimeMs = 5ULL * 60ULL * 1000ULL;
```

改成 1 分钟：

```cpp
constexpr std::uint64_t kDefaultLifetimeMs = 1ULL * 60ULL * 1000ULL;
```

改成 10 分钟：

```cpp
constexpr std::uint64_t kDefaultLifetimeMs = 10ULL * 60ULL * 1000ULL;
```

它被用在两个地方：

```cpp
const TicketTgsBody ticket_body{
    kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};

const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
```

人话解释：

`ticket_body` 是给 TGS 看的票据，里面的 `lifetime2` 代表 TGS 应该认可这张票多久。  
`rep_body` 是给 Client 看的回复，里面的 `lifetime2` 是告诉客户端“这张 Ticket_tgs 大概能用多久”。  

这两个地方共用同一个 `kDefaultLifetimeMs`，所以改一次，两边保持一致。

注意：

```text
如果老师问 Ticket_v 的有效期，那不是 AS，而是 TGS。
对应文件是 src/roles/tgs/tgs_service.cpp。
```

## 3. 高概率：让 AS 校验 TS1 时间戳，防止旧请求重放

老师可能问：

```text
AS_REQ 里的 ts1 现在有没有用？
如果别人抓到旧 AS_REQ 再发一次怎么办？
能不能加一个时间窗口校验？
```

要改的地方：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
变量：as_req.ts1
```

现在 `as_req.ts1` 会被解析出来，但 AS 没有拿它做时间校验。

可以在文件顶部常量区加：

```cpp
constexpr std::uint64_t kMaxClockSkewMs = 2ULL * 60ULL * 1000ULL;
```

然后在 `as_process_connection()` 解析 AS_REQ 后加：

```cpp
const std::uint64_t now = auth_time_now_ms();
if (as_req.ts1 + kMaxClockSkewMs < now || as_req.ts1 > now + kMaxClockSkewMs)
{
    throw std::runtime_error("AS_REQ timestamp is outside allowed clock skew");
}
```

人话解释：

`ts1` 是 Client 发请求时的当前时间。  
AS 收到后拿自己的当前时间 `now` 对比。  
如果两个时间差太大，就说明这个请求可能太旧，或者客户端时间不对。

现场注意：

```text
这属于“安全增强”，但可能会因为两台电脑时间不一致导致登录失败。
如果演示是在同一台电脑上跑，一般没问题。
```

## 4. 高概率：老师让你解释或修改 Kc_tgs 是怎么生成的

老师可能问：

```text
Kc_tgs 在哪生成？
能不能临时改成固定值方便测试？
为什么 Client 和 TGS 都能拿到 Kc_tgs？
```

要改的地方：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
变量：kc_tgs
```

现在：

```cpp
const std::uint64_t kc_tgs = generate_des_key56();
```

临时改成固定值：

```cpp
const std::uint64_t kc_tgs = 0x123456789ABCU;
```

人话解释：

`kc_tgs` 是 Client 后面跟 TGS 通信用的临时钥匙。  
AS 会把同一把 `kc_tgs` 放进两个地方：

```text
1. 放进 AsRepBody，用 Client 的长期密钥 Kc 加密，所以 Client 能看见。
2. 放进 TicketTgsBody，用 KTGS 加密，所以 TGS 能看见。
```

对应代码：

```cpp
const TicketTgsBody ticket_body{
    kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};

const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));

const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
```

验收时可以这样说：

```text
AS 自己生成会话密钥 Kc_tgs，然后一份给客户端，一份封进票据给 TGS。
客户端不能打开 Ticket_tgs，因为 Ticket_tgs 是用 KTGS 加密的。
```

## 5. 高概率：老师让你把错误返回给客户端，而不是只在服务端打印

老师可能问：

```text
如果 AS_REQ 格式错了，客户端能不能收到 MSG_ERROR？
现在服务端 throw 后客户端知道具体错误吗？
```

要改的地方：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
相关函数：make_error_payload()
相关枚举：include/cyber/shared/types.hpp 里的 ErrorCode
```

当前情况：

```text
AS 内部 throw 后，会被 runtime_handle_server_connection() 捕获并打印。
客户端一般只会表现为认证失败，不一定拿到明确 MSG_ERROR。
```

如果要最小实现，需要两边改。

AS 侧思路：

```cpp
Packet request;
EntityId error_dst = EntityId::unknown;
try
{
    request = recv_packet_logged(socket);
    error_dst = request.src;

    packet_require_msg_type(request, MsgType::as_req);
    // 后面照旧处理
}
catch (const std::exception& ex)
{
    if (error_dst != EntityId::unknown)
    {
        const Packet error = make_packet(
            MsgType::error,
            EntityId::as,
            error_dst,
            make_error_payload(ErrorCode::unsupported_msg_type, ex.what()));
        send_packet_logged(socket, error);
    }
    close_socket(socket);
    return;
}
```

Client 侧也要认 `MsgType::error`：

```text
src/roles/client/client_auth_flow.cpp
函数：client_auth_connect_to_v_socket()
位置：auth_exchange_packet() 返回 as_rep 后
```

可以在：

```cpp
const Packet as_rep =
    auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
packet_require_msg_type(as_rep, MsgType::as_rep);
```

中间加：

```cpp
if (as_rep.msg_type == MsgType::error)
{
    throw std::runtime_error(parse_error_message(as_rep.payload));
}
```

人话解释：

AS 只打印错误，客户端只知道“失败了”。  
AS 发 `MSG_ERROR`，客户端就能拿到“为什么失败”。

## 6. 中高概率：老师让你改 AS_REQ 或 AS_REP 的字段

老师可能问：

```text
给 AS_REQ 加一个 nonce。
AS_REP 里多返回一个字段。
Ticket_tgs 里多放一个字段。
你能不能改报文结构？
```

要改的文件有四类，不能只改一个：

```text
1. include/cyber/protocol/kerberos_messages.hpp
   改结构体定义。

2. src/shared/protocol/kerberos_messages.cpp
   改 build/parse 函数。

3. src/roles/client/client_auth_flow.cpp
   Client 构造请求、解析回复。

4. src/roles/as/as_service.cpp
   AS 读取请求、填充回复。
```

以“AS_REQ 加 nonce”为例：

第一步，改结构体：

```cpp
struct AsReq
{
    EntityId idc = EntityId::unknown;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts1 = 0;
    std::uint64_t nonce = 0;
};
```

第二步，改 `as_build_req()`：

```cpp
write_u64(out, value.nonce);
```

第三步，改 `as_parse_req()`：

```cpp
value.nonce = read_u64(payload, offset);
```

同时注意原来的长度检查要从 10 字节变成 18 字节。  
因为：

```text
idc   1 字节
idtgs 1 字节
ts1   8 字节
nonce 8 字节
总共 18 字节
```

第四步，Client 构造 AS_REQ 时填上 nonce：

```cpp
as_build_req({state.client_id, EntityId::tgs, ts1, nonce})
```

第五步，AS 解析后使用：

```cpp
const AsReq as_req = as_parse_req(request.payload);
// as_req.nonce 就是客户端带来的随机数
```

人话解释：

结构体只是“内存里的样子”。  
真正上网传输的是 `build` 函数写出来的字节。  
收到后能不能读回来，靠的是 `parse` 函数按同样顺序读。  
所以加字段必须结构体、build、parse、使用方一起改。

## 7. 中高概率：老师让你改密码或解释密码为什么能验证

老师可能问：

```text
密码在哪里验证？
为什么 AS 没有直接看到 password？
改了 config 里的密码为什么还要改 Kc？
```

相关文件：

```text
src/roles/client/tank_game_client.cpp
函数：TankGameClient::handle_login()

src/shared/auth/auth_credentials.cpp
函数：auth_derive_client_key()

src/shared/config/config.cpp
函数：Config::clients()

src/roles/as/as_service.cpp
函数：as_find_client_secret()
```

主线是：

```text
网页输入 password
  -> handle_login()
  -> auth_derive_client_key(client_id, password)
  -> 算出 Kc
  -> Client 用 Kc 解 AS_REP

AS 不收明文 password
  -> AS 从 config.clients() 找到这个 client_id 的 Kc
  -> AS 用配置里的 Kc 加密 AS_REP
```

人话解释：

这个项目更像 Kerberos 的做法：密码不直接发给 AS。  
Client 用输入的密码自己算出 `Kc`。  
AS 用配置文件里保存的 `C1_KC` 加密回复。  
如果用户密码错了，Client 算出来的 `Kc` 就不对，后面解不开 AS_REP。

如果老师让你“改 Client1 密码”，通常要改：

```text
config/course_config.txt
C1_PASSWORD
C1_KC
```

还要改测试：

```text
tests/auth_credentials_selftest.cpp
```

因为 `C1_KC` 必须和 `auth_derive_client_key(EntityId::client1, 新密码)` 算出来的值一致。

## 8. 中高概率：老师让你新增一个客户端 Client5

老师可能问：

```text
现在只有 4 个客户端，能不能加第 5 个？
为什么 config.clients() 只读 4 个？
```

要改的地方比较多：

```text
include/cyber/shared/types.hpp
  -> EntityId 里加 client5
  -> is_known()
  -> is_client()
  -> to_string(EntityId)

src/shared/config/config.cpp
  -> Config::clients() 的循环从 i <= 4 改成 i <= 5

config/course_config.txt
  -> 加 C5_ID / C5_PASSWORD / C5_KC

src/shared/crypto/crypto.cpp
  -> demo_rsa_key_pair_for() 需要有 Client5 的 RSA 演示密钥

tests/protocol_selftest.cpp
tests/auth_credentials_selftest.cpp
tests/game_load_client.cpp
  -> 可能有写死 4 个客户端的断言
```

人话解释：

新增 Client 不是只加配置。  
因为 `EntityId` 是枚举，代码要知道 `0x05` 代表 Client5。  
配置加载函数也要从原来的 4 个读到 5 个。  
如果 Client5 要参与证书和签名，还要给它准备 RSA 演示密钥。

现场不建议主动选这个改动，因为牵涉范围比较大。

## 9. 中概率：老师让你改 AS 的 IP 或端口

老师可能问：

```text
AS 监听端口在哪改？
Client 连接 AS 的 IP 在哪改？
AS_BIND_IP 和 AS_IP 有什么区别？
```

大多数情况只改配置：

```text
config/course_config.txt
AS_IP
AS_BIND_IP
AS_PORT
```

代码对应关系：

```text
src/shared/runtime/role_runtime.cpp
runtime_build_role_spec()
  -> AS_PORT / AS_IP / AS_BIND_IP 这些 key 是这里绑定给 AS 角色的

runtime_bind_endpoint()
  -> 服务端监听 AS_BIND_IP:AS_PORT

src/roles/client/client_auth_flow.cpp
config_build_endpoint(config, "AS_IP", "AS_PORT")
  -> Client 连接 AS_IP:AS_PORT
```

人话解释：

`AS_BIND_IP` 是 AS 自己在哪个网卡上监听。  
`AS_IP` 是别人连接 AS 时使用的地址。  
本机演示一般都是 `127.0.0.1`。  
局域网演示时，`AS_BIND_IP` 可以是 `0.0.0.0`，`AS_IP` 应该写 AS 那台机器的局域网 IP。

## 10. 中概率：老师让你改 AS 只处理一次请求就退出

老师可能问：

```text
能不能让 AS 处理一个连接后自动退出？
max_connections 是怎么工作的？
```

一般不用改代码，启动参数就能做：

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve --max-connections 1
```

如果一定要看代码：

```text
src/shared/runtime/role_runtime.cpp
函数：runtime_run_auth_server()
变量：max_connections / accepted_count
```

关键逻辑：

```cpp
if (max_connections > 0)
{
    worker.join();
    if (accepted_count >= max_connections)
    {
        break;
    }
}
else
{
    worker.detach();
}
```

人话解释：

`max_connections == 0` 表示一直服务，线程处理完自己结束。  
`max_connections > 0` 表示处理固定数量连接，处理够了就跳出循环。

## 11. 中概率：老师让你加 AS 调试输出

老师可能问：

```text
AS 收到了哪个 Client 的请求？
能不能把 Kc_tgs、ts2 打印出来？
```

要改的地方：

```text
src/roles/as/as_service.cpp
函数：as_process_connection()
```

先加头文件：

```cpp
#include <iostream>
```

再在关键位置加：

```cpp
std::cout << "AS_REQ from " << to_string(as_req.idc)
          << ", ts1=" << as_req.ts1 << '\n';

std::cout << "AS generated Kc_tgs=0x" << std::hex << kc_tgs << std::dec
          << ", ts2=" << ts2
          << ", lifetime=" << kDefaultLifetimeMs << "ms\n";
```

人话解释：

这只是调试输出，不改变协议。  
如果想看报文加密前后内容，项目本来就有 `send_packet_logged()` 和 `recv_packet_logged()` 写协议事件。

## 12. 中概率：老师让你改 AS 默认配置文件搜索路径

老师可能问：

```text
不传 --config 的时候默认读哪里？
能不能让它默认读另一个配置文件？
```

要改的地方：

```text
src/shared/runtime/role_runtime.cpp
函数：config_find_default_path()
```

现在候选路径：

```cpp
const std::vector<std::filesystem::path> candidates = {
    "config/course_config.txt",
    "../config/course_config.txt",
    "../../config/course_config.txt"};
```

要加新路径就往这个列表里加：

```cpp
const std::vector<std::filesystem::path> candidates = {
    "config/local_config.txt",
    "config/course_config.txt",
    "../config/course_config.txt",
    "../../config/course_config.txt"};
```

人话解释：

这个函数只负责“不写 --config 时去哪找配置”。  
如果命令行明确传了 `--config`，就不会走默认搜索。

## 13. 中概率：老师让你改 AS_REP 加密用的密钥

老师可能问：

```text
AS_REP 是用什么加密的？
为什么 Client 能解？
Ticket_tgs 是用什么加密的？
为什么 Client 不能解 Ticket_tgs？
```

要看两个变量：

```text
src/roles/as/as_service.cpp
secret.kc
config.get_u64("KTGS")
```

关键代码：

```cpp
const ClientSecret secret = as_find_client_secret(config, as_req.idc);

const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));

const Packet response =
    as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                              rep_plain, secret.kc);
```

人话解释：

`secret.kc` 是 Client 的长期密钥，AS 和 Client 都知道，所以 AS_REP 用它加密。  
`KTGS` 是 TGS 的长期密钥，只有 AS 和 TGS 知道，所以 Ticket_tgs 用它加密。  

如果老师让你“把 Ticket_tgs 换个密钥加密”，AS 和 TGS 必须一起改：

```text
src/roles/as/as_service.cpp
  -> tgs_ticket_encrypt(..., 新密钥)

src/roles/tgs/tgs_service.cpp
  -> tgs_ticket_decrypt(..., 同一个新密钥)
```

只改一边会导致 TGS 解不开票据。

## 14. 中低概率：老师让你改协议监视器里显示的字段

老师可能问：

```text
监视器里能不能显示 ticket_tgs 明文？
能不能显示 AS_REP 加密前后的内容？
```

AS 侧相关代码：

```text
src/roles/as/as_service.cpp
函数：protocol_build_encrypted_payload_view()
函数：protocol_add_encrypted_field()
函数：send_packet_logged(socket, response, view)
```

当前 AS 已经把 AS_REP 的明文和密文视图传给日志系统：

```cpp
ProtocolPayloadView view =
    protocol_build_encrypted_payload_view(rep_plain, response.payload);
protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                             tgs_ticket_build_body(ticket_body));
send_packet_logged(socket, response, view);
```

人话解释：

`rep_plain` 是 AS_REP 加密前的内容。  
`response.payload` 是 AS_REP 加密后的密文。  
`ticket_tgs` 是票据密文。  
`tgs_ticket_build_body(ticket_body)` 是票据加密前的明文内容。

如果只是改日志字段名字，改 `protocol_add_encrypted_field(view, "ticket_tgs", ...)` 的字符串。  
如果要改前端显示样式，才需要看 protocol monitor 或 web-ui。

## 15. 中低概率：老师让你改 Client 登录失败提示

老师可能问：

```text
密码错了，前端能不能显示更具体的提示？
连接 AS 失败时能不能提示 AS 不在线？
```

要改的地方：

```text
src/roles/client/tank_game_client.cpp
函数：TankGameClient::handle_login()
```

这段捕获了登录流程中的异常：

```cpp
catch (const std::exception& ex)
{
    // 这里会广播 failed 状态
}
```

人话解释：

`handle_login()` 是网页点击登录后进来的地方。  
它里面调用 `client_auth_connect_to_v_socket()` 完成 AS/TGS/V 全流程。  
任何一步失败都会抛异常回到这里，所以前端登录失败提示一般从这里改。

如果老师要“区分密码错和服务器没开”，要结合异常信息判断，例如：

```text
connect failed -> AS/TGS/V 没启动或端口不通
decrypt/parse failed -> 可能密码错或密钥不匹配
MSG_ERROR -> 服务端明确返回的协议错误
```

## 16. 不建议现场主动选：把 AS 真正改成校验明文 password

老师可能问：

```text
AS 能不能直接判断 password 对不对？
```

这个问题要谨慎回答。

当前设计不是把 password 发给 AS，而是：

```text
Client 输入 password
  -> 本地派生 Kc
AS 读取 config 里的 Cx_KC
  -> 用 Kc 加密 AS_REP
Client 如果 password 错
  -> 派生出的 Kc 不对
  -> 解不开 AS_REP
```

如果真的要让 AS 直接校验 password，就要改变协议：

```text
AS_REQ 要多带 password 或 password hash
AS 要解析这个字段
AS 要和 config 里的 Cx_PASSWORD 对比
Client 要构造新 AS_REQ
kerberos_messages.hpp/cpp 要一起改
```

这会破坏 Kerberos“不直接传密码”的设计。  
验收时更好的说法是：

```text
这里不是直接传明文密码给 AS，而是用密码派生出来的长期密钥 Kc 参与加密。
密码错时，Client 解不开 AS_REP，认证流程自然失败。
```

## 17. 最推荐提前练的 5 个现场改动

如果时间有限，优先练这五个：

| 优先级 | 改动 | 文件 | 函数 |
|---|---|---|---|
| 1 | AS 校验 request.src / request.dst / as_req.idc / as_req.idtgs | `src/roles/as/as_service.cpp` | `as_process_connection()` |
| 2 | 改 Ticket_tgs 有效期 | `src/roles/as/as_service.cpp` | `kDefaultLifetimeMs` |
| 3 | 加 TS1 时间窗口校验 | `src/roles/as/as_service.cpp` | `as_process_connection()` |
| 4 | AS 错误返回 MSG_ERROR | `src/roles/as/as_service.cpp`、`src/roles/client/client_auth_flow.cpp` | `as_process_connection()`、`client_auth_connect_to_v_socket()` |
| 5 | 给 AS_REQ/AS_REP 加字段 | `kerberos_messages.hpp/cpp`、`client_auth_flow.cpp`、`as_service.cpp` | build/parse/构造/处理 |

## 18. 现场回答模板

老师问：“这个功能在哪里改？”

可以先按这个顺序回答：

```text
如果是启动、参数、IP、端口，先看 role_runtime.cpp 和 config。
如果是 Client 发 AS_REQ，先看 client_auth_flow.cpp。
如果是 AS 收包、验身份、发 Ticket_tgs，先看 as_service.cpp。
如果是报文字段本身，先看 kerberos_messages.hpp/cpp。
如果是密码和 Kc，先看 auth_credentials.cpp 和 config.cpp。
```

老师问：“你为什么改这个函数？”

可以这样说：

```text
因为这个函数正好是数据第一次变成这个形态的地方。
比如 AS_REQ 的字段是在 kerberos_messages.cpp 里 build/parse 的，
但 AS 真正做业务判断是在 as_service.cpp 的 as_process_connection()。
所以字段格式改 build/parse，业务校验改 as_process_connection()。
```

老师问：“改完怎么验证？”

可以这样说：

```text
先编译，确认语法没问题。
然后开 AS/TGS/V/Client，用正确密码登录一次，确认原流程不坏。
再用错误密码或篡改字段测试，确认新加的校验能触发。
最后看 protocol_events 或监视器，确认 AS_REQ/AS_REP 仍然正常记录。
```

