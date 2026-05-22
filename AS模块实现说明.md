# AS 模块实现说明

这版按两层来读：

1. **先看函数之间怎么传数据**：哪个文件的哪个函数调用下一个函数，传了什么参数，对方用什么参数接住。
2. **再看函数内部怎么实现**：接住这些参数后，函数里面每个变量是怎么来的、干什么用、最后传给谁。

先记住一句话：

```text
AS 阶段不是一个函数从头跑到尾，而是 AS 服务端进程和 Client 进程通过网络配合完成。
AS 先启动监听；Client 登录时连接 AS，发 AS_REQ；AS 处理后返回 AS_REP。
```

## 0. 总览：两条线先分清

### 0.1 AS 服务端启动线

```text
src/roles/as/main.cpp
  main(argc, argv)
    -> run_role_main(RoleKind::as_server, argc, argv)

src/shared/runtime/role_runtime.cpp
  run_role_main(role, argc, argv)
    -> runtime_build_role_spec(role)
    -> Config::load(config_path)
    -> runtime_run_auth_server(role, config, spec, max_connections)
    -> runtime_bind_endpoint(config, spec)
    -> listen_tcp(endpoint)
    -> accept_tcp(listener, &peer)
    -> runtime_handle_server_connection(accepted, role, spec, config)
    -> cyber::roles::as::as_process_connection(socket, config)

src/roles/as/as_service.cpp
  as_process_connection(socket, config)
    -> 等 Client 发 AS_REQ
```

人话：

```text
AS 先独立启动。
main.cpp 只是入口。
role_runtime.cpp 负责读配置、开端口、接连接。
接到连接后，把 socket 和 config 交给 as_service.cpp。
```

### 0.2 Client 登录认证线

```text
网页输入 clientId/password

src/roles/client/tank_game_client.cpp
  TankGameClient::handle_login(command)
    -> auth_derive_client_key(command.client_id, command.password)
    -> client_auth_connect_to_v_socket(config_, command.client_id, kc)

src/roles/client/client_auth_flow.cpp
  client_auth_connect_to_v_socket(config, client_id, kc)
    -> state.client_id = client_id
    -> state.kc = kc
    -> as_build_req({state.client_id, EntityId::tgs, ts1})
    -> auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req)

src/shared/protocol/kerberos_messages.cpp
  as_build_req(value)
    -> 把 idc / idtgs / ts1 写成 AS_REQ payload

网络传输
  Client 把 as_req 发给 AS
  AS 返回 as_rep

src/roles/client/client_auth_flow.cpp
  des_decrypt_payload(as_rep.payload, state.kc)
  as_parse_rep_body(as_rep_plain)
  state.kc_tgs = as_body.kc_tgs
  state.ticket_tgs = as_body.ticket_tgs
```

人话：

```text
Client 登录时先用密码算出 Kc。
然后用 Kc 去跑 AS 阶段。
AS 回来的 AS_REP 如果能用 Kc 解开，就说明密码正确。
解开后拿到 Kc_tgs 和 Ticket_tgs，准备进入 TGS 阶段。
```

## 1. 函数数据接力总表

这张表先只看“谁把什么传给谁”，不看内部实现。

| 步骤 | 当前文件/函数 | 传出的数据 | 下一个文件/函数 | 对方怎么接住 |
| --- | --- | --- | --- | --- |
| 1 | `src/roles/as/main.cpp` / `main(argc, argv)` | `RoleKind::as_server`, `argc`, `argv` | `run_role_main(role, argc, argv)` | `role` 接住 AS 身份，`argc/argv` 接住命令行 |
| 2 | `run_role_main()` | `role` | `runtime_build_role_spec(role)` | 根据 `role` 生成 `spec` |
| 3 | `run_role_main()` | `config_path` | `Config::load(config_path)` | 读取配置，返回 `config` |
| 4 | `run_role_main()` | `role`, `config`, `spec`, `max_connections` | `runtime_run_auth_server(...)` | 用这些参数启动 AS/TGS 服务端 |
| 5 | `runtime_run_auth_server()` | `config`, `spec` | `runtime_bind_endpoint(config, spec)` | 读 `AS_BIND_IP` 和 `AS_PORT`，返回 `endpoint` |
| 6 | `runtime_run_auth_server()` | `endpoint` | `listen_tcp(endpoint)` | 创建监听 socket，返回 `listener` |
| 7 | `runtime_run_auth_server()` | `listener` | `accept_tcp(listener, &peer)` | 接收一个 Client 连接，返回 `accepted` |
| 8 | `runtime_run_auth_server()` | `accepted`, `role`, `spec`, `config` | `runtime_handle_server_connection(...)` | 根据 `role` 分发 |
| 9 | `runtime_handle_server_connection()` | `socket`, `config` | `as_process_connection(socket, config)` | AS 用 socket 收请求，用 config 查密钥 |
| 10 | `tank_game_client.cpp` / `handle_login(command)` | `command.client_id`, `command.password` | `auth_derive_client_key(...)` | 算出 Client 长期密钥 `kc` |
| 11 | `handle_login()` | `config_`, `command.client_id`, `kc` | `client_auth_connect_to_v_socket(config, client_id, kc)` | 进入完整认证流程 |
| 12 | `client_auth_connect_to_v_socket()` | `state.client_id`, `EntityId::tgs`, `ts1` | `as_build_req(...)` | 生成 AS_REQ payload |
| 13 | `client_auth_connect_to_v_socket()` | `config`, `"AS_IP"`, `"AS_PORT"` | `config_build_endpoint(...)` | 得到 AS 连接地址 |
| 14 | `client_auth_connect_to_v_socket()` | `endpoint`, `as_req` | `auth_exchange_packet(endpoint, as_req)` | 连接 AS，发送 AS_REQ，接收 AS_REP |
| 15 | `as_process_connection()` | `request.payload` | `as_parse_req(request.payload)` | 解析出 `as_req.idc`, `as_req.idtgs`, `as_req.ts1` |
| 16 | `as_process_connection()` | `config`, `as_req.idc` | `as_find_client_secret(config, as_req.idc)` | 找到 `secret.kc` |
| 17 | `as_process_connection()` | `ticket_body`, `KTGS` | `tgs_ticket_encrypt(...)` | 得到加密的 `ticket_tgs` |
| 18 | `as_process_connection()` | `rep_body` | `as_build_rep_body(rep_body)` | 得到 `rep_plain` |
| 19 | `as_process_connection()` | `rep_plain`, `secret.kc` | `as_build_encrypted_packet(...)` | 得到最终 `response` |
| 20 | `client_auth_flow.cpp` | `as_rep.payload`, `state.kc` | `des_decrypt_payload(...)` | 解出 `as_rep_plain` |
| 21 | `client_auth_flow.cpp` | `as_rep_plain` | `as_parse_rep_body(as_rep_plain)` | 得到 `as_body` |
| 22 | `client_auth_flow.cpp` | `as_body.kc_tgs`, `as_body.ticket_tgs` | `state.kc_tgs`, `state.ticket_tgs` | 保存 AS 阶段结果 |

## 2. AS 服务端启动：函数先讲作用，再讲内部

### 2.1 `main()`：把 AS 身份交给统一启动函数

文件：

```text
src/roles/as/main.cpp
```

函数作用：

```text
main() 是 as_server.exe 的入口。
它不处理网络，也不处理 AS_REQ。
它只把 RoleKind::as_server 传给 run_role_main()。
```

代码：

```cpp
int main(int argc, char** argv)
{
    return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
}
```

传参关系：

| 传出去的值 | 接收方参数 | 用途 |
| --- | --- | --- |
| `RoleKind::as_server` | `role` | 告诉启动框架当前角色是 AS |
| `argc` | `argc` | 命令行参数数量 |
| `argv` | `argv` | 命令行参数内容 |

下一步看：

```text
src/shared/runtime/role_runtime.cpp
run_role_main(role, argc, argv)
```

### 2.2 `run_role_main()`：统一启动总控

文件：

```text
src/shared/runtime/role_runtime.cpp
```

函数作用：

```text
run_role_main() 根据 role 和命令行参数决定怎么启动。
对于 AS 来说，它会读取配置，然后在 --serve 模式下启动监听服务。
```

函数开头这些变量为什么要建：

```cpp
const RoleSpec spec = runtime_build_role_spec(role);
bool print_config = false;
bool serve = false;
bool game_auth_encrypted = false;
std::uint16_t ui_port = 0;
int max_connections = 0;
std::filesystem::path config_path;
```

| 变量 | 为什么要建 | 初始值为什么这样 | 后面怎么用 |
| --- | --- | --- | --- |
| `spec` | 保存当前角色需要用的配置键名 | 由 `role` 立刻生成 | 后面读端口、打印 usage、启动服务都要用 |
| `print_config` | 记录用户是否只想打印配置 | 默认 `false`，不打印 | 遇到 `--print-config` 变 `true` |
| `serve` | 记录是否作为 AS/TGS 服务端运行 | 默认 `false`，避免误启动 | 遇到 `--serve` 变 `true` |
| `game_auth_encrypted` | 记录是否启动 V/Client 的最终游戏模式 | 默认 `false` | AS 不用它，但统一框架要兼容 V/Client |
| `ui_port` | Client WebSocket 端口 | 默认 `0` 表示未设置 | Client 模式必须设置 |
| `max_connections` | 限制最多处理多少连接 | 默认 `0` 表示无限处理 | 测试时可传 `--max-connections N` |
| `config_path` | 保存配置文件路径 | 默认空，表示用户没指定 | 后面如果为空就找默认配置 |

这段的人话：

```text
run_role_main 先准备一堆开关。
这些开关一开始都是默认状态。
后面扫描 argv，看到用户传了什么参数，就把对应开关打开。
```

参数解析怎么传递：

| 命令行参数 | 改哪个变量 | 后面影响 |
| --- | --- | --- |
| `--print-config` | `print_config = true` | 后面只打印配置并退出 |
| `--serve` | `serve = true` | 后面调用 `runtime_run_auth_server()` |
| `--max-connections N` | `max_connections = N` | 服务端处理 N 个连接后退出 |
| `--game-auth-encrypted` | `game_auth_encrypted = true` | V/Client 模式使用 |
| `--ui-port PORT` | `ui_port = PORT` | Client 模式使用 |
| `--config PATH` | `config_path = PATH` | `Config::load(config_path)` 读取这个文件 |

关键调用：

```cpp
const Config config = Config::load(config_path);
...
if (serve)
{
    runtime_run_auth_server(role, config, spec, max_connections);
}
```

传参关系：

| 传出去的值 | 接收方参数 | 用途 |
| --- | --- | --- |
| `role` | `role` | 继续告诉下一个函数当前是 AS |
| `config` | `config` | 后面监听端口、查密钥都靠它 |
| `spec` | `spec` | 后面知道要读 `AS_BIND_IP` / `AS_PORT` |
| `max_connections` | `max_connections` | 控制处理连接数量 |

下一步看：

```text
runtime_run_auth_server(role, config, spec, max_connections)
```

### 2.3 `runtime_build_role_spec()`：把 role 翻译成配置键名

函数作用：

```text
role 只是一个枚举，信息太少。
runtime_build_role_spec() 把它翻译成更完整的 RoleSpec。
```

AS 分支：

```cpp
case RoleKind::as_server:
    return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
            "handle AS_REQ and return AS_REP"};
```

这里返回的 `spec` 后面这样被使用：

| `spec` 字段 | 谁用它 | 干什么 |
| --- | --- | --- |
| `spec.name` | 打印和报错 | 显示 `"AS"` |
| `spec.binary` | `runtime_print_usage()` | 显示 `"as_server"` |
| `spec.id_key` | `config.get_entity_id(spec.id_key)` | 读取 `AS_ID` |
| `spec.port_key` | `config.get_u16(spec.port_key)` | 读取 `AS_PORT` |
| `spec.bind_ip_key` | `runtime_bind_endpoint()` | 读取 `AS_BIND_IP` |

### 2.4 `runtime_run_auth_server()`：真正监听

函数作用：

```text
根据 role/config/spec 启动 AS 或 TGS 的短连接认证服务。
对于 AS，就是监听 AS_BIND_IP:AS_PORT，等 Client 连接。
```

关键代码：

```cpp
SocketRuntime runtime;
const TcpEndpoint endpoint = runtime_bind_endpoint(config, spec);
SocketHandle listener = listen_tcp(endpoint);
...
SocketHandle accepted = accept_tcp(listener, &peer);
std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
```

变量细讲：

| 变量 | 为什么要建 | 来源 | 后面给谁 |
| --- | --- | --- | --- |
| `runtime` | 初始化 Windows socket 环境 | `SocketRuntime runtime` | 让后续 socket API 可用 |
| `endpoint` | 保存 AS 监听地址 | `runtime_bind_endpoint(config, spec)` | 给 `listen_tcp()` |
| `listener` | 保存监听 socket | `listen_tcp(endpoint)` | 给 `accept_tcp()` |
| `accepted_count` | 记录已处理连接数 | 本函数内部 | 配合 `max_connections` |
| `peer` | 保存对方地址文本 | `accept_tcp(listener, &peer)` 写入 | 可用于调试 |
| `accepted` | 保存某个 Client 的连接 socket | `accept_tcp()` 返回 | 给 `runtime_handle_server_connection()` |
| `worker` | 每个连接一个线程 | `std::thread(...)` | 执行连接处理函数 |

调用下一步：

```cpp
runtime_handle_server_connection(accepted, role, spec, config)
```

传参关系：

| 传出去的值 | 接收方参数 | 用途 |
| --- | --- | --- |
| `accepted` | `socket` | 当前 Client 连接 |
| `role` | `role` | 判断调用 AS 还是 TGS |
| `spec` | `spec` | 出错时打印角色名 |
| `config` | `config` | AS 后面查密钥 |

### 2.5 `runtime_handle_server_connection()`：把 socket 分发给 AS

函数作用：

```text
这个函数不处理 AS 协议。
它只根据 role 判断：这个连接应该交给 AS 还是 TGS。
```

AS 分支：

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
```

传参关系：

| 传出去的值 | 接收方参数 | 用途 |
| --- | --- | --- |
| `socket` | `socket` | AS 从这个连接里读 AS_REQ，再写 AS_REP |
| `config` | `config` | AS 查 `C1_KC`、`KTGS` |

下一步看：

```text
src/roles/as/as_service.cpp
as_process_connection(socket, config)
```

## 3. Client 登录线：函数先讲作用，再讲内部

### 3.1 `TankGameClient::handle_login()`：把 UI 登录变成认证流程

文件：

```text
src/roles/client/tank_game_client.cpp
```

函数作用：

```text
接收 UI 发来的 client_id 和 password。
先把 password 算成 Kc。
再调用 client_auth_connect_to_v_socket() 跑完整认证流程。
```

关键代码：

```cpp
const std::uint64_t kc = auth_derive_client_key(command.client_id, command.password);
cyber::roles::client::VAuthenticatedSocket auth =
    cyber::roles::client::client_auth_connect_to_v_socket(config_, command.client_id, kc);
```

数据传递：

| 当前变量 | 来源 | 传给谁 | 对方怎么接 |
| --- | --- | --- | --- |
| `command.client_id` | UI 登录命令 | `auth_derive_client_key()` | 参数 `client_id` |
| `command.password` | UI 登录命令 | `auth_derive_client_key()` | 参数 `password` |
| `kc` | `auth_derive_client_key()` 返回 | `client_auth_connect_to_v_socket()` | 参数 `kc` |
| `config_` | Client 对象保存的配置 | `client_auth_connect_to_v_socket()` | 参数 `config` |
| `command.client_id` | UI 登录命令 | `client_auth_connect_to_v_socket()` | 参数 `client_id` |

人话：

```text
handle_login 是 UI 和认证代码之间的桥。
UI 给的是密码，认证流程真正用的是 Kc。
所以这里先把密码变成 Kc，再把 config、client_id、kc 一起传下去。
```

### 3.2 `auth_derive_client_key()`：把密码变成 Kc

文件：

```text
src/shared/auth/auth_credentials.cpp
```

函数作用：

```text
根据 client_id 和 password 计算 Client 长期密钥 Kc。
```

关键变量：

| 变量 | 为什么要建 | 后面怎么用 |
| --- | --- | --- |
| `client_id` | 区分是 Client1 还是 Client2 | 拼进 `material` |
| `password` | 用户输入的密码 | 拼进 `material` |
| `material` | 把固定前缀、client_id、password 拼起来 | 转 bytes 后 hash |
| `bytes` | hash 函数处理字节，不直接处理 string | 传给 `hash64()` |
| `key` | hash 后得到的 56 位密钥 | 返回给 `handle_login()` |

人话：

```text
Client 不把密码明文发给 AS。
它本地把密码算成 Kc。
AS_REP 后面能不能用 Kc 解开，就证明密码对不对。
```

### 3.3 `client_auth_connect_to_v_socket()`：进入 AS 阶段

文件：

```text
src/roles/client/client_auth_flow.cpp
```

函数作用：

```text
跑完整 Kerberos 认证流程。
这里先讲 AS 阶段：构造 AS_REQ，发给 AS，收 AS_REP，解出 Kc_tgs 和 Ticket_tgs。
```

函数接收：

```cpp
client_auth_connect_to_v_socket(const Config& config, EntityId client_id, std::uint64_t kc)
```

| 参数 | 上一层传来的是什么 | 函数内部保存到哪里 |
| --- | --- | --- |
| `config` | `config_` | 后面读取 `AS_IP`、`AS_PORT` |
| `client_id` | `command.client_id` | `state.client_id` |
| `kc` | `auth_derive_client_key()` 算出的密钥 | `state.kc` |

初始化：

```cpp
AuthClientState state;
state.client_id = client_id;
state.adc = kDefaultAdc;
state.kc = kc;
```

变量细讲：

| 变量 | 为什么建 |
| --- | --- |
| `state` | 存认证全过程的状态，AS/TGS/V 都要往里面放东西 |
| `state.client_id` | 后面构造 AS_REQ 时要说明“我是谁” |
| `state.adc` | 后续认证器会用到的 Client 地址 |
| `state.kc` | 解 AS_REP 用 |

### 3.4 `as_build_req()`：生成 AS_REQ payload

调用位置：

```cpp
const std::uint64_t ts1 = auth_time_now_ms();
const Packet as_req =
    make_packet(MsgType::as_req, state.client_id, EntityId::as,
                as_build_req({state.client_id, EntityId::tgs, ts1}));
```

先讲函数作用：

```text
as_build_req() 只负责生成 AS_REQ 的 payload。
make_packet() 再把 payload 包成完整网络包。
```

数据传递：

| 数据 | 来源 | 传给谁 |
| --- | --- | --- |
| `state.client_id` | 前面 `client_id` 保存来的 | `AsReq.idc` |
| `EntityId::tgs` | 固定目标 | `AsReq.idtgs` |
| `ts1` | 当前时间 | `AsReq.ts1` |

`as_build_req()` 实现在：

```text
src/shared/protocol/kerberos_messages.cpp
```

它把这三个字段写成 10 字节：

```text
idc 1字节 + idtgs 1字节 + ts1 8字节
```

### 3.5 `auth_exchange_packet()`：把 AS_REQ 发出去

调用：

```cpp
const Packet as_rep =
    auth_exchange_packet(config_build_endpoint(config, "AS_IP", "AS_PORT"), as_req);
```

先讲函数作用：

```text
config_build_endpoint() 负责找到 AS 地址。
auth_exchange_packet() 负责连接 AS、发送 as_req、接收 as_rep、关闭连接。
```

数据传递：

| 当前数据 | 传给谁 | 对方怎么用 |
| --- | --- | --- |
| `config` + `"AS_IP"` + `"AS_PORT"` | `config_build_endpoint()` | 读配置得到 AS 地址 |
| 返回的 `endpoint` | `auth_exchange_packet()` | `connect_tcp(endpoint)` 连接 AS |
| `as_req` | `auth_exchange_packet()` | `send_packet_logged()` 发给 AS |
| AS 返回包 | `as_rep` | 后面解密 |

## 4. AS 处理线：函数先讲作用，再讲内部

### 4.1 `as_process_connection()`：处理一个 Client 的 AS 请求

文件：

```text
src/roles/as/as_service.cpp
```

函数作用：

```text
从 socket 收 AS_REQ。
解析 Client 身份。
查 Client 长期密钥。
生成 Kc_tgs 和 Ticket_tgs。
返回用 Kc 加密的 AS_REP。
```

函数接收：

```cpp
as_process_connection(SocketHandle socket, const Config& config)
```

| 参数 | 谁传来的 | 用途 |
| --- | --- | --- |
| `socket` | `runtime_handle_server_connection()` | 收 AS_REQ、发 AS_REP |
| `config` | `runtime_handle_server_connection()` | 查 Client 密钥和 `KTGS` |

### 4.2 接收并检查 AS_REQ

```cpp
const Packet request = recv_packet_logged(socket);
packet_require_msg_type(request, MsgType::as_req);
```

函数作用：

| 函数 | 作用 |
| --- | --- |
| `recv_packet_logged(socket)` | 从当前连接读出 Client 发来的完整包 |
| `packet_require_msg_type(...)` | 确认这个包确实是 AS_REQ |

变量：

| 变量 | 为什么建 |
| --- | --- |
| `request` | 保存 Client 发来的包，后面要读 `request.payload` |

### 4.3 `as_parse_req()`：把 payload 读回字段

```cpp
const AsReq as_req = as_parse_req(request.payload);
```

函数作用：

```text
把 Client 之前用 as_build_req() 写成的 10 字节 payload 读回来。
```

`as_req` 里得到：

| 字段 | 来源 | 用途 |
| --- | --- | --- |
| `as_req.idc` | AS_REQ 第 1 字节 | 查 Client 密钥 |
| `as_req.idtgs` | AS_REQ 第 2 字节 | 表示目标是 TGS |
| `as_req.ts1` | AS_REQ 后 8 字节 | 表示请求时间 |

### 4.4 `as_find_client_secret()`：查 Client 长期密钥

```cpp
const ClientSecret secret = as_find_client_secret(config, as_req.idc);
```

函数作用：

```text
拿 as_req.idc 去配置文件的 Client 列表里找对应 Client。
```

数据传递：

| 数据 | 来源 | 作用 |
| --- | --- | --- |
| `config` | runtime 传入 | 里面有 `C1_ID`、`C1_KC` 等 |
| `as_req.idc` | Client 发来的身份 | 用来匹配哪个 Client |
| `secret.kc` | 查到的长期密钥 | 后面加密 AS_REP |

### 4.5 生成 `kc_tgs` 和 `ticket_tgs`

```cpp
const std::uint64_t kc_tgs = generate_des_key56();
const std::uint64_t ts2 = auth_time_now_ms();
const TicketTgsBody ticket_body{
    kc_tgs, as_req.idc, kDefaultAdc, EntityId::tgs, ts2, kDefaultLifetimeMs};
const Bytes ticket_tgs = tgs_ticket_encrypt(ticket_body, config.get_u64("KTGS"));
```

先讲函数/变量作用：

| 名字 | 作用 |
| --- | --- |
| `generate_des_key56()` | 生成 Client 和 TGS 后续共用的新钥匙 |
| `auth_time_now_ms()` | 得到 AS 当前签发时间 |
| `ticket_body` | Ticket 的明文内容 |
| `tgs_ticket_encrypt()` | 用 `KTGS` 把 Ticket 明文加密 |
| `ticket_tgs` | 加密后的 Ticket，给 Client 转交给 TGS |

为什么要 `ticket_body` 再 `ticket_tgs`：

```text
ticket_body 是写信内容。
ticket_tgs 是把信用 KTGS 锁起来后的密文。
Client 拿到密文但不能改；TGS 有 KTGS，能打开。
```

### 4.6 生成 AS_REP

```cpp
const AsRepBody rep_body{kc_tgs, EntityId::tgs, ts2, kDefaultLifetimeMs, ticket_tgs};
const Bytes rep_plain = as_build_rep_body(rep_body);
const Packet response =
    as_build_encrypted_packet(MsgType::as_rep, EntityId::as, as_req.idc,
                              rep_plain, secret.kc);
```

先讲函数/变量作用：

| 名字 | 作用 |
| --- | --- |
| `rep_body` | AS_REP 的明文结构 |
| `as_build_rep_body()` | 把 `rep_body` 写成 bytes |
| `rep_plain` | AS_REP 明文字节 |
| `as_build_encrypted_packet()` | 用 `secret.kc` 加密 `rep_plain` 并包成 Packet |
| `response` | 最终要发给 Client 的 AS_REP |

为什么用 `secret.kc`：

```text
AS 查配置得到的 secret.kc 是正确长期密钥。
Client 如果密码正确，本地算出的 state.kc 会和 secret.kc 一样。
所以 Client 能解开 AS_REP。
```

### 4.7 发回 Client

```cpp
send_packet_logged(socket, response, view);
close_socket(socket);
```

函数作用：

| 函数 | 作用 |
| --- | --- |
| `send_packet_logged()` | 把 AS_REP 发回 Client，同时写协议事件 |
| `close_socket()` | AS 阶段是一问一答，发完关闭连接 |

## 5. Client 接收 AS_REP：函数先讲作用，再讲内部

文件：

```text
src/roles/client/client_auth_flow.cpp
```

AS 返回的包在 Client 这里叫：

```cpp
const Packet as_rep = ...
```

### 5.1 检查类型

```cpp
packet_require_msg_type(as_rep, MsgType::as_rep);
```

作用：

```text
确认收到的是 AS_REP，不是错误类型或其他阶段的包。
```

### 5.2 解密 AS_REP

```cpp
const Bytes as_rep_plain = des_decrypt_payload(as_rep.payload, state.kc);
const AsRepBody as_body = as_parse_rep_body(as_rep_plain);
```

函数/变量作用：

| 名字 | 作用 |
| --- | --- |
| `as_rep.payload` | AS 发回来的密文 |
| `state.kc` | Client 根据密码算出的长期密钥 |
| `des_decrypt_payload()` | 用 `state.kc` 解密 |
| `as_rep_plain` | 解密后的 AS_REP 明文字节 |
| `as_parse_rep_body()` | 把明文字节解析成结构体 |
| `as_body` | 解析后的 AS_REP 内容 |

### 5.3 保存 AS 阶段结果

```cpp
state.kc_tgs = as_body.kc_tgs;
state.ticket_tgs = as_body.ticket_tgs;
```

作用：

| 保存到 | 来源 | 后面干什么 |
| --- | --- | --- |
| `state.kc_tgs` | `as_body.kc_tgs` | 下一阶段和 TGS 通信用 |
| `state.ticket_tgs` | `as_body.ticket_tgs` | 下一阶段发给 TGS |

人话：

```text
AS 阶段最终产物就是这两个东西：
Kc_tgs 和 Ticket_tgs。
拿到它们，Client 才能继续 TGS 阶段。
```
