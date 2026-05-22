# AS 自己理解稿评价与补充

这份是针对你写的 `自己理解.txt` 做的评价和补充。  
你这版最大的优点是：你不是在背概念，而是真的顺着代码跑了一遍，这是对的。  
现在要做的是把几个地方讲准，这样验收时老师追问细节，你不会卡住。

## 0. 总体评价

整体主线：正确。

你已经抓住了 AS 的核心流程：

```text
AS main.cpp
  -> run_role_main()
  -> runtime_run_auth_server()
  -> runtime_handle_server_connection()
  -> as_process_connection()
  -> 收 AS_REQ
  -> 查 Client 的 Kc
  -> 生成 Kc_tgs
  -> 生成 Ticket_tgs
  -> 生成 AS_REP
  -> 用 Kc 加密发回 Client
```

这个方向没有问题。  
如果按验收讲解水平来看，你现在已经能讲出大框架了。

但是你还需要补几个细节：

```text
1. role_runtime.cpp 里不是一开始就直接拿到真实 AS_IP/AS_PORT，而是先通过 RoleSpec 知道要读哪些 config key。
2. config_path = argv[++i] 只在命令行传了 --config 时发生；不传时才走默认路径。
3. runtime_run_auth_server() 不是直接执行 as_process_connection()，而是先监听、accept 到连接，再开线程处理。
4. recv_packet_logged() 不是“封装 Packet”，而是从 socket 读字节并解析成 Packet。
5. as_parse_req() 的作用不是普通类型转换，而是按固定字节格式把 payload 拆回 AsReq。
6. rep_plain 是 AS_REP 正文的明文字节，不是 Packet，也不是最终网络包。
7. 当前 AS 代码只检查 msg_type 是 AS_REQ，还没有检查 request.src/request.dst/as_req.idc/as_req.idtgs 是否一致。
```

## 1. 你理解正确的地方

### 1.1 AS 的启动线理解对了

你说从：

```text
src/roles/as/main.cpp
  -> run_role_main()
  -> src/shared/runtime/role_runtime.cpp
```

这个是对的。

`src/roles/as/main.cpp` 里面确实只有：

```cpp
return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
```

它的意思不是自己实现 AS，而是告诉公共启动框架：

```text
这次我要以 AS 服务器身份运行。
```

### 1.2 `--serve` 控制是否启动服务，这个理解对了

`serve` 默认是：

```cpp
bool serve = false;
```

只有命令行里传了：

```powershell
--serve
```

才会执行：

```cpp
serve = true;
```

最后：

```cpp
if (serve)
{
    runtime_run_auth_server(role, config, spec, max_connections);
}
```

所以你说“用户通过命令框启动，然后 serve 变成 true，后面 if(serve) 启动服务”，这个是对的。

### 1.3 AS 核心处理流程理解对了

你对 `as_process_connection()` 的理解基本正确：

```text
收到 AS_REQ
  -> 检查是不是 AS_REQ
  -> 解析 payload 得到 AsReq
  -> 根据 idc 找 Client 的 Kc
  -> 生成 Kc_tgs
  -> 生成 Ticket_tgs
  -> 用 KTGS 加密 Ticket_tgs
  -> 生成 AS_REP
  -> 用 Client 的 Kc 加密 AS_REP
  -> 发回 Client
```

这是 AS 这一关最关键的主线。

### 1.4 Ticket_tgs 用 KTGS 加密，这个理解对了

代码是：

```cpp
const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
```

你说“通过 KTGS 加密 TicketTgs”，这个是准确的。

人话解释：

```text
Ticket_tgs 是给 TGS 看的票据。
所以它不能用 Client 的 Kc 加密，而要用 AS 和 TGS 都知道的 KTGS 加密。
Client 拿到 ticket_tgs 以后打不开，只能转交给 TGS。
```

### 1.5 AS_REP 用 Client 的 Kc 加密，这个理解对了

代码是：

```cpp
const Packet response =
    as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                              rep_plain, secret.kc);
```

这里的 `secret.kc` 是 AS 从 config 里根据 `as_req.idc` 找到的 Client 长期密钥。

你说“通过 config 里的 kc 加密 AS_REP 发回 client”，这个方向是对的。

更严谨一点应该说：

```text
AS 先根据 as_req.idc 从 config.clients() 找到对应 ClientSecret。
然后用 ClientSecret 里的 secret.kc 加密 AS_REP。
```

## 2. 需要修正的地方

### 2.1 main.cpp 不是“填了 run_role_main 的变量”

你原话大概是：

```text
main.cpp 文件，填了 run_role_main 函数的变量告诉现在要跑 as 的角色
```

这个意思接近，但可以说得更准：

```text
main.cpp 没有填很多变量。
它只是把 RoleKind::as_server、argc、argv 传给 run_role_main()。
RoleKind::as_server 用来告诉公共启动框架：当前角色是 AS。
argc/argv 是命令行参数，比如 --config、--serve。
```

建议你验收时这样讲：

```text
AS 的 main.cpp 很薄，它不写具体业务，只把 RoleKind::as_server 传给公共 runtime。
后面 runtime 根据这个角色去选择 AS 对应的配置 key 和处理函数。
```

### 2.2 runtime_build_role_spec() 不是直接获取真实 IP 和端口

你原话里说：

```text
runtime_build_role_spec 获取到 asid、as监听端口，as的ip
```

这里要小修一下。

`runtime_build_role_spec()` 返回的是：

```cpp
return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
        "handle AS_REQ and return AS_REP"};
```

它里面的 `"AS_PORT"`、`"AS_IP"`、`"AS_BIND_IP"` 不是端口和 IP 的真实值。  
它们只是 config 里的 key 名字。

也就是说：

```text
runtime_build_role_spec()
  -> 告诉 runtime：AS 要去 config 里读 AS_PORT、AS_IP、AS_BIND_IP。

Config::load()
  -> 真正读取 config/course_config.txt。

config.get_u16("AS_PORT")
config.get_string("AS_BIND_IP")
  -> 这里才拿到真实端口和 IP。
```

所以更准确的说法是：

```text
runtime_build_role_spec() 先生成 AS 的角色说明书。
说明书里记录 AS 这个角色对应哪些 config key。
后面真正加载 config 后，才通过这些 key 取到实际 IP 和端口。
```

### 2.3 config_path 的来源要分两种情况

你括号里问：

```text
config_path = argv[++i]; 我记得不修改是默认路径
```

这里你记得基本对，但是要分清。

第一种：命令行传了 `--config`

```powershell
.\as_server.exe --config .\config\course_config.txt --serve
```

这时执行：

```cpp
config_path = argv[++i];
```

意思是：

```text
argv[++i] 取 --config 后面的那个参数。
比如 --config 后面是 .\config\course_config.txt，
那 config_path 就等于这个路径。
```

第二种：命令行没传 `--config`

这时循环结束后：

```cpp
if (config_path.empty())
{
    config_path = config_find_default_path();
}
```

然后 `config_find_default_path()` 会按顺序找：

```text
config/course_config.txt
../config/course_config.txt
../../config/course_config.txt
```

所以你可以这样记：

```text
传了 --config：用命令行给的路径。
没传 --config：自动找默认路径。
```

### 2.4 runtime_run_auth_server() 不是“看 AS/TGS 身份来显示监听 IP”

`runtime_run_auth_server()` 确实会根据 `role` 判断是否允许当前角色以认证服务器方式启动：

```cpp
if (role == RoleKind::client)
{
    throw std::runtime_error("client cannot run --serve");
}
if (role == RoleKind::v_server)
{
    throw std::runtime_error("v_server uses --game-auth-encrypted in the final runtime");
}
```

但它不是靠 `if as / if tgs` 来分别显示监听 IP。  
真正的监听地址来自：

```cpp
const TcpEndpoint endpoint = runtime_bind_endpoint(config, spec);
```

而 `runtime_bind_endpoint()` 是用 `spec.bind_ip_key` 和 `spec.port_key` 去 config 里拿值：

```cpp
return {config.get_string(spec.bind_ip_key), config.get_u16(spec.port_key)};
```

对 AS 来说：

```text
spec.bind_ip_key = "AS_BIND_IP"
spec.port_key = "AS_PORT"
```

对 TGS 来说：

```text
spec.bind_ip_key = "TGS_BIND_IP"
spec.port_key = "TGS_PORT"
```

所以更准确的说法是：

```text
runtime_run_auth_server() 根据当前角色对应的 RoleSpec，从 config 里取出监听 IP 和端口，然后创建监听 socket。
```

### 2.5 runtime_run_auth_server() 不会直接执行 runtime_handle_server_connection()

你写的是：

```text
同时执行 runtime_handle_server_connection
```

这里要讲准一点。

实际流程是：

```cpp
SocketHandle listener = listen_tcp(endpoint);

while (true)
{
    SocketHandle accepted = accept_tcp(listener, &peer);
    std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
}
```

人话：

```text
先 listen_tcp() 开始监听。
然后 accept_tcp() 等客户端连进来。
只有真的收到一个连接以后，才开一个线程调用 runtime_handle_server_connection()。
```

所以它不是服务一启动就立刻处理 AS_REQ。  
它是：

```text
先等连接。
连接来了。
再处理。
```

### 2.6 runtime 里不会打印“哪个 Client 正在登录”

你写到：

```text
然后打印是哪个 client 进行登录验证了
```

当前代码里 `role_runtime.cpp` 主要打印的是：

```text
role: AS
entity id
listen port
stage goal
AS listening on ...
protocol events path
```

它不会打印“Client1 正在登录”。  
AS 收到 AS_REQ 后，`as_process_connection()` 能从 `as_req.idc` 知道是哪个 Client，但当前代码也没有 `std::cout` 打印它。

如果验收时说这个，容易被老师抓住。  
建议你改成：

```text
runtime 负责打印当前角色、监听端口和协议日志路径。
具体是哪个 Client 发请求，要到 as_process_connection() 解析 AS_REQ 后才知道。
当前代码没有直接打印出来，只是在协议日志里记录收发包。
```

### 2.7 “接收 socket，封装成 Packet”要改成“从 socket 读出 Packet”

你原话：

```text
首先接收socket，封装成packet
```

这句话有一点不准。

`socket` 不是被接收的数据本身。  
`socket` 是连接句柄，可以理解成“这条网络连接”。

代码：

```cpp
const Packet request = recv_packet_logged(socket);
```

更准确的人话是：

```text
AS 通过这个 socket 连接读取网络字节。
recv_packet_logged() 会把网络字节解析成 Packet。
所以 request 是收到的一封完整协议包。
```

也就是说：

```text
socket = 通信通道
Packet = 从通道里读出来的一封信
```

## 3. 你括号里问的两个重点

### 3.1 为什么要把 payload 转换为 AsReq

你写到：

```text
先把 payload 转换为 as_req（这里解释一下为啥要转换）
```

这里非常值得讲清楚。

`payload` 的类型是 `Bytes`，本质是字节数组。  
它长这样：

```text
[ 1字节 idc ][ 1字节 idtgs ][ 8字节 ts1 ]
```

但是代码如果一直拿字节数组用，就很难读：

```text
payload[0] 是谁？
payload[1] 是谁？
后 8 字节怎么转成 uint64？
```

所以项目定义了结构体：

```cpp
struct AsReq
{
    EntityId idc = EntityId::unknown;
    EntityId idtgs = EntityId::unknown;
    std::uint64_t ts1 = 0;
};
```

然后用：

```cpp
const AsReq as_req = as_parse_req(request.payload);
```

把字节数组拆成有名字的字段。

人话：

```text
payload 是原始信纸，是一串字节。
as_parse_req() 是拆信纸。
拆完以后，AS 就能通过 as_req.idc、as_req.idtgs、as_req.ts1 直接读出业务含义。
```

注意：这不是普通的 C++ 类型强转。  
它是按协议规定的顺序手动解析：

```cpp
value.idc = static_cast<EntityId>(payload[offset++]);
value.idtgs = static_cast<EntityId>(payload[offset++]);
value.ts1 = binary_read_u64(payload, offset, "payload");
```

所以验收时你可以说：

```text
payload 只是网络上传来的字节，AS 要做业务判断就必须先 parse 成 AsReq。
这样才能知道请求里的 idc、idtgs 和 ts1 分别是什么。
```

### 3.2 rep_body 为什么要封装成 rep_plain

你写到：

```text
rep_body封装为rep_plain（这里也解释一下）
```

这个点也很关键。

`rep_body` 是 C++ 结构体：

```cpp
const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
```

结构体在程序内存里好用，但是不能直接当网络 payload 发。  
网络上传的是字节，所以要调用：

```cpp
const Bytes rep_plain = as_build_rep_body(rep_body);
```

把结构体写成字节。

这里变量名里的 `plain` 意思是明文。  
所以：

```text
rep_body
  -> C++ 里的 AS_REP 结构体

rep_plain
  -> rep_body 按协议格式写出来的明文字节

response.payload
  -> rep_plain 用 secret.kc 加密后的密文字节
```

完整顺序是：

```text
AsRepBody 结构体
  -> as_build_rep_body()
  -> rep_plain 明文字节
  -> des_encrypt_payload(rep_plain, secret.kc)
  -> response.payload 密文字节
  -> make_packet()
  -> Packet response
```

你可以这样讲：

```text
rep_body 是为了代码里好组织字段。
rep_plain 是为了加密和网络传输。
加密函数处理的是字节，不是结构体，所以中间必须 build 成 Bytes。
```

## 4. 你漏掉但建议补上的细节

### 4.1 AsRepBody 里还有 idtgs

你写 AS_REP 里有：

```text
kc_tgs、签发时间、票据有效期、TicketTgs
```

还漏了一个：

```text
EntityId::tgs
```

代码：

```cpp
const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
```

也就是说 AS_REP 里有：

```text
kc_tgs
idtgs
ts2
lifetime2
ticket_tgs
```

`idtgs` 的作用是告诉 Client：

```text
这把 Kc_tgs 是给你和 TGS 通信用的。
```

### 4.2 当前 AS 没有校验 request.src 和 as_req.idc 是否一致

你讲了：

```text
判断 packet 是不是 AS_REQ
```

这是当前代码确实做了的。

代码：

```cpp
packet_require_msg_type(request, MsgType::as_req);
```

但当前代码没有继续检查：

```text
request.src 是否是 Client
request.dst 是否是 AS
request.src 是否等于 as_req.idc
as_req.idtgs 是否等于 TGS
```

所以如果老师问：

```text
AS 有没有验证报文头 src 和正文 idc 一致？
```

你要诚实说：

```text
当前代码主要检查了消息类型，并根据 as_req.idc 去 config 查密钥。
更严格的身份一致性检查可以补在 as_parse_req() 后面。
```

这个不是你理解错，而是一个可以现场改代码的点。

### 4.3 as_find_client_secret() 不是直接验证密码

你说：

```text
去找 config 里对应这个 client 的 kc
```

这个对。

但如果老师问“AS 怎么验证密码”，你要注意不能说 AS 直接比较 password。

当前项目是：

```text
Client 用输入的 password 算 Kc。
AS 用 config 里的 Cx_KC 加密 AS_REP。
Client 如果 password 正确，算出的 Kc 和 AS 用的 Kc 一样，就能解开。
Client 如果 password 错，解不开。
```

所以身份验证是间接完成的。

`as_find_client_secret()` 只负责：

```text
根据 idc 找这个 Client 在 config 里的资料。
```

### 4.4 protocol view 只是日志和监视器，不是 AS 核心逻辑

代码：

```cpp
ProtocolPayloadView view =
    protocol_build_encrypted_payload_view(rep_plain, response.payload);
protocol_add_encrypted_field(view, "ticket_tgs", ticket_tgs,
                             tgs_ticket_build_body(ticket_body));
send_packet_logged(socket, response, view);
```

这里容易被误认为也是认证的一部分。

更准确的理解：

```text
response 才是真正要发给 Client 的 AS_REP。
view 是给协议日志和监视器看的辅助信息。
```

也就是说：

```text
没有 view，协议本身仍然能跑。
有 view，老师可以在监视器里看到明文/密文对照。
```

### 4.5 close_socket() 在正常和异常时都会调用

正常流程：

```cpp
send_packet_logged(socket, response, view);
close_socket(socket);
```

异常流程：

```cpp
catch (...)
{
    close_socket(socket);
    throw;
}
```

人话：

```text
AS 处理完这次 AS_REQ 后会关闭连接。
如果中途出错，也会关闭连接，然后把异常继续抛给 runtime 打印。
```

这个细节老师问“连接生命周期”时可以讲。

## 5. 你这版讲解里建议换掉的说法

| 你原来的说法 | 建议改成 |
|---|---|
| main.cpp 填了 run_role_main 函数的变量 | main.cpp 把 RoleKind::as_server 和命令行参数交给公共 runtime |
| runtime_build_role_spec 获取到 AS 监听端口和 IP | runtime_build_role_spec 返回 AS 对应的 config key，真实 IP/端口后面从 config 读 |
| 打印是哪个 client 进行登录验证了 | runtime 打印角色和监听信息，具体 Client 要解析 AS_REQ 后才知道，当前代码没有直接打印 |
| 接收 socket，封装成 Packet | 通过 socket 读取网络字节，并解析成 Packet |
| payload 转换为 as_req | payload 按协议格式解析成 AsReq，不是普通类型转换 |
| rep_body 封装为 rep_plain | rep_body 序列化成 AS_REP 明文字节 rep_plain，之后再用 Kc 加密 |
| 通过 config 里的 kc 加密 as_rep | 根据 as_req.idc 找到 ClientSecret，再用 secret.kc 加密 AS_REP 明文 |

## 6. 可以直接补进你理解里的关键解释

### payload 为什么要 parse

```text
payload 是网络上传输的一串原始字节。
AS 不能直接从字节里看出 Client 是谁，所以要用 as_parse_req() 按 AS_REQ 的格式拆开。
拆开以后才有 as_req.idc、as_req.idtgs、as_req.ts1 这些带名字的字段。
```

### rep_plain 是什么

```text
rep_plain 是 AS_REP 的明文字节。
rep_body 是结构体，方便代码组织字段。
as_build_rep_body(rep_body) 把结构体写成字节，得到 rep_plain。
然后 AS 用 Client 的 Kc 加密 rep_plain，最终密文放进 response.payload。
```

### config 在 AS 里的作用

```text
config 对 AS 来说像登记表。
AS 用它拿 AS_BIND_IP/AS_PORT 来启动监听。
AS 也用它通过 config.clients() 找 Client 的 Kc。
AS 还用它拿 KTGS 来加密 Ticket_tgs。
```

### AS 怎么证明用户密码正确

```text
AS 不直接收 password。
Client 根据输入的 password 算 Kc。
AS 根据 idc 从 config 里查 Cx_KC。
AS 用查到的 Kc 加密 AS_REP。
如果密码正确，Client 算出的 Kc 一样，就能解开。
如果密码错误，Client 算出的 Kc 不一样，就解不开。
```

## 7. 按你这种风格整理出来的讲解稿

下面这段尽量保留你原来“顺着代码一路讲”的风格，但是把几个容易说错的地方修正了。  
你验收前可以照这个顺一遍。

```text
首先我先看 roles 下 as 文件夹里的 main.cpp。
这个文件其实很薄，它没有直接写 AS 的业务逻辑，
只是调用 run_role_main，并且传进去 RoleKind::as_server，
意思就是告诉公共运行框架：我现在要以 AS 服务器这个角色启动。
argc 和 argv 也一起传进去，是为了后面解析命令行参数，比如 --config 和 --serve。

然后我再看 shared/runtime 下面的 role_runtime.cpp。
这里面 run_role_main 是所有角色共用的启动入口。
它一开始会通过 runtime_build_role_spec(role) 生成当前角色的说明。
因为我这里传的是 as_server，所以 spec 里面会记录 AS 这个角色对应的名字、程序名、实体 ID，
还有要去 config 里读取哪些 key，比如 AS_ID、AS_PORT、AS_IP、AS_BIND_IP。
这里注意它不是马上拿到真实 IP 和端口，而是先知道 key 名字，
真正的值要等 Config::load() 把 config 文件读进来以后，再通过 config.get_string 或 config.get_u16 获取。

接下来 run_role_main 里面有几个变量，比如 print_config、serve、game_auth_encrypted、ui_port、max_connections 和 config_path。
serve 默认是 false，也就是说程序默认不会直接启动服务。
如果命令行参数里有 --serve，就会把 serve 改成 true。
config_path 如果命令行传了 --config，就通过 config_path = argv[++i] 拿到 --config 后面的路径。
如果没有传 --config，那么参数解析完以后会发现 config_path 是空的，
这时就调用 config_find_default_path() 去默认找 config/course_config.txt 这些路径。

等参数解析完以后，run_role_main 会加载 config 文件。
加载完以后会打印当前角色、实体 ID、监听端口、阶段目标这些信息。
如果 serve 是 true，就会调用 runtime_run_auth_server(role, config, spec, max_connections)。

runtime_run_auth_server 主要负责启动认证服务器。
它会先判断当前角色能不能用 --serve。
Client 不能直接 --serve，V 在这个项目里也不是用这个方式启动。
AS 和 TGS 可以走这里。
然后它会通过 runtime_bind_endpoint(config, spec) 获取监听地址。
对 AS 来说，就是用 spec 里的 AS_BIND_IP 和 AS_PORT 到 config 里取真实值。
拿到监听地址以后，listen_tcp(endpoint) 开始监听。

然后程序进入 while 循环，一直等客户端连接。
accept_tcp(listener, &peer) 这里会阻塞等待连接。
只有真的有连接进来了，才会得到 accepted 这个 socket。
接着它开一个线程，把 accepted、role、spec、config 传给 runtime_handle_server_connection。

runtime_handle_server_connection 是一个分发函数。
它会判断当前 role 是 AS 还是 TGS。
如果 role 是 RoleKind::as_server，就调用 cyber::roles::as::as_process_connection(socket, config)。
所以真正 AS 这个角色干活的地方，就是 src/roles/as/as_service.cpp 里的 as_process_connection。

进入 as_process_connection 以后，第一步是 const Packet request = recv_packet_logged(socket)。
这里 socket 可以理解成这次网络连接。
recv_packet_logged 会从 socket 里读取网络字节，然后按照项目定义的协议解析成 Packet。
Packet 可以理解成信封，里面有 msg_type、src、dst 和 payload。
msg_type 表示这封信是什么类型，src 表示谁发来的，dst 表示发给谁，payload 才是真正的信纸内容。

然后调用 packet_require_msg_type(request, MsgType::as_req)。
这一步检查的是信封上的消息类型是不是 AS_REQ。
如果不是 AS_REQ，就抛异常，说明这个连接发来的不是 AS 应该处理的请求。

接着是 const AsReq as_req = as_parse_req(request.payload)。
这里 payload 本身只是一串字节，AS 不能直接知道里面哪个字节代表 Client，哪个字节代表 TGS，哪个字节代表时间。
所以要用 as_parse_req 按 AS_REQ 的格式把 payload 拆成 AsReq 结构体。
拆完以后就能通过 as_req.idc 知道客户端是谁，
通过 as_req.idtgs 知道客户端想找哪个 TGS，
通过 as_req.ts1 知道客户端发请求的时间。

然后是 const ClientSecret secret = as_find_client_secret(config, as_req.idc)。
这一步是 AS 根据 as_req.idc 去 config 里面找这个客户端的资料。
config 对 AS 来说就像登记表，里面有 C1_ID、C1_PASSWORD、C1_KC 这些信息。
这里真正后面会用到的是 secret.kc，也就是这个 Client 的长期密钥 Kc。
AS 不是直接拿 password 比较，而是用这个 Kc 加密回复。
如果客户端输入密码正确，它自己算出来的 Kc 就和这里的 secret.kc 一样，就能解开 AS_REP。

接下来 AS 生成临时会话密钥。
const std::uint64_t kc_tgs = generate_des_key56()。
这个 kc_tgs 是 Client 后面跟 TGS 通信用的临时钥匙。
然后 const std::uint64_t ts2 = auth_time_now_ms() 获取 AS 当前时间，
这个时间表示 AS 签发票据的时间。

然后 AS 开始准备 Ticket_tgs。
TicketTgsBody ticket_body 里面放了 kc_tgs、客户端 id、客户端地址、TGS id、签发时间 ts2 和有效期。
这里的 ticket_body 还是明文结构体。
但是 Ticket_tgs 是给 TGS 看的，不能让 Client 随便改，
所以后面调用 tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"))。
这里 config.get_u64("KTGS") 是从 config 里拿 AS 和 TGS 共享的长期密钥 KTGS。
加密以后得到 ticket_tgs。
Client 拿到 ticket_tgs 以后打不开，只能原样交给 TGS。
TGS 因为知道 KTGS，所以能打开。

然后 AS 准备回给 Client 的 AS_REP。
const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs}。
这里面有 kc_tgs、TGS 的身份、签发时间、有效期，还有刚才加密好的 ticket_tgs。
rep_body 是 C++ 结构体，方便代码组织字段，但是网络上传输和加密都需要字节，
所以要调用 as_build_rep_body(rep_body)，把它序列化成 Bytes。
这个结果叫 rep_plain，意思是 AS_REP 的明文字节。

然后 AS 调用 as_build_encrypted_packet。
这里传的类型是 MsgType::as_rep，发送者是 EntityId::as，接收者是 as_req.idc，
明文内容是 rep_plain，加密密钥是 secret.kc。
也就是说 AS 用 Client 的长期密钥 Kc 把 AS_REP 明文字节加密，
加密后的内容放进 response.payload 里。
这样只有输入正确密码、算出正确 Kc 的 Client 才能解开。

后面 ProtocolPayloadView view 这一块主要是为了协议监视器和日志显示，
它把 rep_plain 和 response.payload 的明文密文对应关系记录下来，
也把 ticket_tgs 的密文和 ticket_body 的明文关系记录下来。
这不是 AS 认证本身必须的业务逻辑，而是为了验收时能看到报文细节。

最后 send_packet_logged(socket, response, view) 把 AS_REP 发回 Client，
然后 close_socket(socket) 关闭这次连接。
如果中间出现异常，catch 里面也会 close_socket(socket)，然后把异常继续抛给 runtime，
runtime_handle_server_connection 会打印 AS worker failed。

所以总结一下，AS 这段核心逻辑就是：
先从 socket 收到 AS_REQ Packet，
再把 payload 解析成 AsReq，
然后根据 idc 查 config 里的 Client Kc，
生成 Kc_tgs，
用 KTGS 生成给 TGS 看的 Ticket_tgs，
再把 Kc_tgs 和 Ticket_tgs 放进 AS_REP，
最后用 Client 的 Kc 加密 AS_REP 发回去。
```

