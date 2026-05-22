# role_runtime.cpp 整体讲解

对应文件：

```text
src/shared/runtime/role_runtime.cpp
```

这份文档先讲这个文件整体是干什么的，再细讲每个功能块怎么实现。  
你可以把它理解成“AS/TGS/V/Client 四个程序共用的启动和调度中心”。

## 0. 这个文件整体实现什么功能

`role_runtime.cpp` 不是 AS 自己的业务逻辑，也不是 TGS 自己的业务逻辑。  
它是四个角色共用的运行框架。

一句话：

```text
role_runtime.cpp 负责把一个角色程序启动起来，
根据命令行参数和 config 决定它是监听端口、启动游戏服务器、启动客户端，
并且在 AS/TGS 这种认证服务器模式下负责多线程接收连接。
```

比如 AS 的真正业务在：

```text
src/roles/as/as_service.cpp
as_process_connection()
```

但是 AS 怎么读配置、怎么监听端口、怎么接收连接、怎么开线程去调用 `as_process_connection()`，这些是在：

```text
src/shared/runtime/role_runtime.cpp
```

里做的。

## 1. 这个文件可以分成六个功能块

按功能看，它大概分成这几块：

```text
1. 角色说明
   RoleSpec
   runtime_build_role_spec()

2. 命令行和配置辅助
   runtime_print_usage()
   config_find_default_path()
   config_print_endpoint()
   config_print_deployment()

3. 网络地址辅助
   net_format_endpoint()
   runtime_bind_endpoint()

4. AS/TGS 单个连接分发
   runtime_handle_server_connection()

5. AS/TGS 服务器监听和多线程
   runtime_run_auth_server()

6. 总入口
   run_role_main()
```

其中最重要的是三个函数：

```text
run_role_main()
  -> 总入口，解析参数、加载 config、选择运行模式。

runtime_run_auth_server()
  -> AS/TGS 的监听循环和多线程实现。

runtime_handle_server_connection()
  -> 每个 worker 线程里，根据 role 调 AS 或 TGS 的处理函数。
```

## 2. 整体运行路线

以 AS 为例，启动命令是：

```powershell
.\as_server.exe --config .\config\course_config.txt --serve
```

整体调用链是：

```text
src/roles/as/main.cpp
  -> run_role_main(RoleKind::as_server, argc, argv)
  -> runtime_build_role_spec(RoleKind::as_server)
  -> 解析 --config / --serve 等参数
  -> Config::load(config_path)
  -> if (serve) runtime_run_auth_server(...)
  -> listen_tcp(AS_BIND_IP:AS_PORT)
  -> accept_tcp()
  -> std::thread worker(runtime_handle_server_connection, ...)
  -> runtime_handle_server_connection()
  -> as_process_connection()
```

人话：

```text
main.cpp 只是告诉 runtime：我是 AS。
role_runtime.cpp 负责把 AS 真的启动成一个服务。
当 Client 连上 AS 时，role_runtime.cpp 创建线程。
线程里再去调用 AS 自己的 as_process_connection()。
```

## 3. 第一块：RoleSpec 是角色说明书

代码：

```cpp
struct RoleSpec
{
    const char* name;
    const char* binary;
    EntityId id;
    const char* id_key;
    const char* port_key;
    const char* connect_ip_key;
    const char* bind_ip_key;
    const char* stage_goal;
};
```

`RoleSpec` 可以理解成“当前角色的说明书”。  
它不是业务数据本身，而是告诉 runtime：

```text
当前角色叫什么？
程序名叫什么？
实体 ID 是什么？
要去 config 里读哪些 key？
这个角色的阶段目标是什么？
```

每个字段的人话解释：

| 字段 | 人话解释 |
|---|---|
| `name` | 角色显示名，比如 `"AS"` |
| `binary` | 可执行程序名，比如 `"as_server"` |
| `id` | 角色枚举 ID，比如 `EntityId::as` |
| `id_key` | config 里实体 ID 的 key，比如 `"AS_ID"` |
| `port_key` | config 里端口的 key，比如 `"AS_PORT"` |
| `connect_ip_key` | 别人连接这个角色时用的 IP key，比如 `"AS_IP"` |
| `bind_ip_key` | 这个角色自己监听时绑定的 IP key，比如 `"AS_BIND_IP"` |
| `stage_goal` | 打印给人看的角色目标 |

注意一个非常重要的点：

```text
RoleSpec 里存的是 config key，不是真实 IP 和端口。
```

比如 AS 的 `port_key` 是：

```text
"AS_PORT"
```

这不是端口号。  
真正的端口号要后面通过：

```cpp
config.get_u16(spec.port_key)
```

去 config 文件里读取。

## 4. runtime_build_role_spec()：根据 role 生成说明书

函数：

```cpp
RoleSpec runtime_build_role_spec(RoleKind role)
```

它根据传进来的 `role` 返回对应的 `RoleSpec`。

AS 分支：

```cpp
case RoleKind::as_server:
    return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
            "handle AS_REQ and return AS_REP"};
```

人话：

```text
如果当前角色是 AS，就告诉 runtime：
  角色名叫 AS
  程序名叫 as_server
  实体 ID 是 EntityId::as
  实体 ID 从 config 的 AS_ID 读
  端口从 AS_PORT 读
  连接 IP 从 AS_IP 读
  监听 IP 从 AS_BIND_IP 读
  目标是处理 AS_REQ 并返回 AS_REP
```

TGS 分支类似：

```text
TGS_ID
TGS_PORT
TGS_IP
TGS_BIND_IP
```

V 分支类似：

```text
V_ID
V_PORT
V_IP
V_BIND_IP
```

Client 比较特殊：

```cpp
case RoleKind::client:
    return {"Client", "client", EntityId::unknown, "LOCAL_CLIENT_ID", nullptr, nullptr,
            nullptr,
            "run AS, TGS, V_AUTH, certificate exchange, then game events"};
```

Client 没有自己的认证服务器监听端口，所以：

```text
port_key = nullptr
connect_ip_key = nullptr
bind_ip_key = nullptr
```

这就是后面为什么代码会通过 `spec.port_key != nullptr` 判断当前角色是不是服务器类角色。

## 5. runtime_print_usage()：打印命令行用法

函数：

```cpp
void runtime_print_usage(const RoleSpec& spec)
```

代码：

```cpp
std::cout << "Usage: " << spec.binary
          << " [--config PATH] [--print-config] [--serve]"
             " [--max-connections N]"
             " [--game-auth-encrypted] [--ui-port PORT]\n";
```

人话：

```text
当用户传了 --help，或者参数写错时，打印这个程序应该怎么启动。
```

它用的是：

```cpp
spec.binary
```

所以 AS 打印的是：

```text
Usage: as_server ...
```

Client 打印的是：

```text
Usage: client ...
```

## 6. config_find_default_path()：找默认配置文件

函数：

```cpp
std::filesystem::path config_find_default_path()
```

代码里候选路径是：

```cpp
const std::vector<std::filesystem::path> candidates = {
    "config/course_config.txt",
    "../config/course_config.txt",
    "../../config/course_config.txt"};
```

逻辑：

```text
按顺序检查这几个路径。
谁存在，就返回谁。
如果都不存在，就返回第一个 config/course_config.txt。
```

它在 `run_role_main()` 里这样用：

```cpp
if (config_path.empty())
{
    config_path = config_find_default_path();
}
```

人话：

```text
如果命令行没传 --config，就自动帮你找默认 config。
```

注意：

```text
这个函数只负责找路径。
真正打开文件的是 Config::load(config_path)。
```

## 7. config_print_endpoint()：打印某个角色的连接和监听地址

函数：

```cpp
void config_print_endpoint(const Config& config, const char* name, const char* ip_key,
                           const char* bind_ip_key, const char* port_key)
```

代码：

```cpp
std::cout << name << " connect: " << config.get_string(ip_key) << ':'
          << config.get_u16(port_key) << '\n';
std::cout << name << " listen:  " << config.get_string(bind_ip_key) << ':'
          << config.get_u16(port_key) << '\n';
```

它打印两种地址：

```text
connect
  -> 别人连接这个角色时用的地址，比如 AS_IP:AS_PORT。

listen
  -> 这个角色自己监听时绑定的地址，比如 AS_BIND_IP:AS_PORT。
```

比如：

```text
AS_IP=172.27.17.5
AS_BIND_IP=0.0.0.0
AS_PORT=9001
```

打印出来就是：

```text
AS connect: 172.27.17.5:9001
AS listen:  0.0.0.0:9001
```

人话：

```text
AS_BIND_IP 是服务端自己绑在哪个网卡上。
AS_IP 是别人连接 AS 时应该填哪个地址。
```

## 8. config_print_deployment()：打印整体部署配置

函数：

```cpp
void config_print_deployment(const Config& config, const RoleSpec& spec)
```

它主要给 `--print-config` 用。

调用位置：

```cpp
if (print_config)
{
    config_print_deployment(config, spec);
    return 0;
}
```

它会打印：

```text
当前角色
本地 Client ID
AS 的 connect/listen 地址
TGS 的 connect/listen 地址
V 的 connect/listen 地址
如果当前角色是服务器，再打印 this server listen
```

代码：

```cpp
config_print_endpoint(config, "AS", "AS_IP", "AS_BIND_IP", "AS_PORT");
config_print_endpoint(config, "TGS", "TGS_IP", "TGS_BIND_IP", "TGS_PORT");
config_print_endpoint(config, "V", "V_IP", "V_BIND_IP", "V_PORT");
```

人话：

```text
这个函数就是帮你检查配置有没有读对。
它不启动服务，只打印。
```

## 9. net_format_endpoint()：把地址转成字符串

函数：

```cpp
std::string net_format_endpoint(const TcpEndpoint& endpoint)
```

代码：

```cpp
return endpoint.ip + ":" + std::to_string(endpoint.port);
```

人话：

```text
把 TcpEndpoint 里的 ip 和 port 拼成 "ip:port" 字符串，方便打印日志。
```

比如：

```text
endpoint.ip = "0.0.0.0"
endpoint.port = 9001
```

返回：

```text
0.0.0.0:9001
```

## 10. runtime_bind_endpoint()：获得当前服务器真正监听地址

函数：

```cpp
TcpEndpoint runtime_bind_endpoint(const Config& config, const RoleSpec& spec)
```

代码：

```cpp
if (spec.bind_ip_key == nullptr || spec.port_key == nullptr)
{
    throw std::runtime_error("role does not have a server endpoint");
}
return {config.get_string(spec.bind_ip_key), config.get_u16(spec.port_key)};
```

它做两件事。

第一，检查当前角色有没有服务器监听地址：

```text
AS/TGS/V 有 bind_ip_key 和 port_key。
Client 没有。
```

所以如果 Client 调这个函数，就会报：

```text
role does not have a server endpoint
```

第二，从 config 里取真实监听地址。

以 AS 为例：

```text
spec.bind_ip_key = "AS_BIND_IP"
spec.port_key = "AS_PORT"
```

所以它实际执行的是：

```cpp
return {config.get_string("AS_BIND_IP"), config.get_u16("AS_PORT")};
```

人话：

```text
RoleSpec 告诉它该读哪些 key。
Config 给出这些 key 的真实值。
最后拼成 TcpEndpoint，给 listen_tcp() 使用。
```

## 11. runtime_handle_server_connection()：worker 线程里的分发函数

函数：

```cpp
void runtime_handle_server_connection(SocketHandle socket, RoleKind role, RoleSpec spec,
                                      Config config)
```

这个函数处理“已经 accept 到的一个连接”。  
注意，它不是监听函数，它是单个连接的处理函数。

代码主逻辑：

```cpp
if (role == RoleKind::as_server)
{
    cyber::roles::as::as_process_connection(socket, config);
}
else if (role == RoleKind::tgs_server)
{
    cyber::roles::tgs::tgs_process_connection(socket, config);
}
else
{
    (void)config;
    close_socket(socket);
    throw std::runtime_error("unsupported auth server role");
}
```

人话：

```text
如果当前是 AS，就把这个 socket 交给 as_process_connection()。
如果当前是 TGS，就把这个 socket 交给 tgs_process_connection()。
如果不是 AS/TGS，就关闭连接并报错。
```

为什么这里只处理 AS/TGS？

```text
因为 runtime_run_auth_server() 这个模式是认证服务器模式。
AS 和 TGS 是一请求一响应的认证服务。
V 在最终游戏模式里不是走这里，而是由 TankGameServer 管理长连接。
Client 也不是服务器。
```

异常处理：

```cpp
catch (const std::exception& ex)
{
    std::cerr << spec.name << " worker failed: " << ex.what() << '\n';
    close_socket(socket);
}
```

人话：

```text
如果 AS/TGS 处理连接时出错，比如报文格式不对、密钥找不到，
worker 线程不会让整个服务直接崩掉。
它会打印 worker failed，并关闭这个 socket。
```

这就是为什么某个 Client 请求失败，不一定导致 AS 进程退出。

## 12. runtime_run_auth_server()：AS/TGS 服务器和多线程核心

函数：

```cpp
void runtime_run_auth_server(RoleKind role, const Config& config, const RoleSpec& spec,
                             int max_connections)
```

这是 AS/TGS 多线程实现的核心函数。

### 12.1 先判断角色能不能 --serve

代码：

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

人话：

```text
Client 不能用 --serve。
V 在最终游戏模式里也不用 --serve。
AS/TGS 才走这个认证服务器监听模式。
```

### 12.2 初始化 socket 运行环境

代码：

```cpp
SocketRuntime runtime;
```

在 Windows 上使用 socket 前，需要做 WSAStartup 之类的初始化。  
这个对象的构造和析构负责这类网络运行时准备和清理。

人话：

```text
这是 Windows 网络库的启动开关。
先有它，后面 listen_tcp、accept_tcp 才能正常工作。
```

### 12.3 获取监听地址

代码：

```cpp
const TcpEndpoint endpoint = runtime_bind_endpoint(config, spec);
```

以 AS 为例，它会从 config 里读：

```text
AS_BIND_IP
AS_PORT
```

得到：

```text
0.0.0.0:9001
```

### 12.4 开始监听

代码：

```cpp
SocketHandle listener = listen_tcp(endpoint);
```

人话：

```text
创建一个监听 socket。
AS 开始在指定 IP 和端口上等 Client 连接。
```

后面打印：

```cpp
std::cout << spec.name << " listening on " << net_format_endpoint(endpoint) << std::endl;
```

比如：

```text
AS listening on 0.0.0.0:9001
```

### 12.5 多线程循环

核心代码：

```cpp
int accepted_count = 0;
while (true)
{
    std::string peer;
    SocketHandle accepted = accept_tcp(listener, &peer);
    ++accepted_count;

    std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
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
}
```

逐句讲。

```cpp
int accepted_count = 0;
```

记录已经接收了多少个连接。  
主要给 `--max-connections` 测试模式用。

```cpp
while (true)
```

一直循环监听连接。

```cpp
std::string peer;
SocketHandle accepted = accept_tcp(listener, &peer);
```

`accept_tcp()` 会阻塞等待客户端连接。  
没有 Client 来时，AS 就停在这里等。  
有 Client 连上来后，返回一个新的 socket，叫 `accepted`。

注意：

```text
listener 是负责监听的 socket。
accepted 是这次和某个 Client 通信的 socket。
```

```cpp
++accepted_count;
```

连接数量加一。

```cpp
std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
```

这是多线程的关键。

人话：

```text
每收到一个连接，就创建一个新线程 worker。
这个线程会执行 runtime_handle_server_connection()。
accepted、role、spec、config 会作为参数传给这个函数。
```

如果当前是 AS，那么 worker 线程里面最终会调用：

```cpp
as_process_connection(socket, config);
```

所以 AS 的多线程模式是：

```text
主线程：
  负责 listen 和 accept。

worker 子线程：
  负责处理单个连接里的 AS_REQ，并返回 AS_REP。
```

### 12.6 detach：正常长期服务模式

代码：

```cpp
else
{
    worker.detach();
}
```

当：

```text
max_connections == 0
```

也就是正常启动：

```powershell
as_server.exe --config .\config\course_config.txt --serve
```

会走 `detach()`。

人话：

```text
worker 线程自己独立运行。
主线程不等它处理完，马上回去继续 accept 下一个连接。
```

所以正常模式下 AS 可以同时处理多个 Client 请求。

### 12.7 join：测试模式

代码：

```cpp
if (max_connections > 0)
{
    worker.join();
    if (accepted_count >= max_connections)
    {
        break;
    }
}
```

如果启动时加了：

```powershell
--max-connections 1
```

那么 `max_connections > 0`。  
代码会调用：

```cpp
worker.join();
```

人话：

```text
主线程等待 worker 处理完这个连接。
处理完以后，如果连接数达到 max_connections，就 break 退出监听循环。
```

这个模式常用于测试：

```text
处理固定数量连接后自动退出。
```

它不是正常长期服务模式。

### 12.8 关闭 listener

循环结束后：

```cpp
close_socket(listener);
```

如果中途异常：

```cpp
catch (...)
{
    close_socket(listener);
    throw;
}
```

人话：

```text
无论正常结束还是异常结束，都尽量关闭监听 socket。
```

## 13. run_role_main()：这个文件的总入口

函数：

```cpp
int run_role_main(RoleKind role, int argc, char** argv)
```

这是暴露给各角色 `main.cpp` 调用的函数。  
它负责完整启动流程。

### 13.1 生成角色说明书

代码：

```cpp
const RoleSpec spec = runtime_build_role_spec(role);
```

人话：

```text
根据 role 知道当前是 AS、TGS、V 还是 Client。
并拿到这个角色对应的 config key 和显示信息。
```

### 13.2 初始化命令行控制变量

代码：

```cpp
bool print_config = false;
bool serve = false;
bool game_auth_encrypted = false;
std::uint16_t ui_port = 0;
int max_connections = 0;
std::filesystem::path config_path;
```

这些都是命令行参数解析出来的状态。

| 变量 | 作用 |
|---|---|
| `print_config` | 是否只打印配置 |
| `serve` | 是否用认证服务器模式启动 |
| `game_auth_encrypted` | 是否用最终加密游戏模式启动 |
| `ui_port` | Client 的 WebSocket UI 端口 |
| `max_connections` | AS/TGS 最多处理多少个连接 |
| `config_path` | config 文件路径 |

### 13.3 解析 argv

代码：

```cpp
for (int i = 1; i < argc; ++i)
{
    const std::string arg = argv[i];
    ...
}
```

为什么从 1 开始？

```text
argv[0] 是程序名。
真正的参数从 argv[1] 开始。
```

支持的参数：

| 参数 | 效果 |
|---|---|
| `--print-config` | `print_config = true` |
| `--serve` | `serve = true` |
| `--max-connections N` | `max_connections = N` |
| `--game-auth-encrypted` | `game_auth_encrypted = true` |
| `--ui-port PORT` | `ui_port = PORT` |
| `--config PATH` | `config_path = PATH` |
| `--help` / `-h` | 打印 usage 并返回 0 |

参数错误时：

```text
缺少 N、缺少 PORT、缺少 PATH、未知参数
```

会返回：

```cpp
return 2;
```

这里可以理解为：

```text
2 表示命令行参数写错。
```

### 13.4 找 config 路径

代码：

```cpp
if (config_path.empty())
{
    config_path = config_find_default_path();
}
```

人话：

```text
如果用户没传 --config，就找默认 config/course_config.txt。
```

### 13.5 加载配置和设置日志目录

代码：

```cpp
const Config config = Config::load(config_path);
set_protocol_event_log_root(log_root_from_config(config));
```

人话：

```text
读取 config 文件。
然后根据 config 设置协议事件日志目录。
```

后面 `send_packet_logged()`、`recv_packet_logged()` 写协议日志时，会用这个日志根目录。

### 13.6 打印角色信息

代码会打印：

```text
当前角色加载了哪个 config
entity id
监听端口，或者 local client id
stage goal
```

服务器角色有 `spec.port_key`：

```cpp
if (spec.port_key != nullptr)
{
    std::cout << "listen port: " << config.get_u16(spec.port_key) << '\n';
}
```

Client 没有 `port_key`：

```cpp
else
{
    std::cout << "local client id: " << to_string(config.get_entity_id("LOCAL_CLIENT_ID"))
              << '\n';
}
```

人话：

```text
AS/TGS/V 打印监听端口。
Client 打印本地客户端身份。
```

### 13.7 print_config 分支

代码：

```cpp
if (print_config)
{
    config_print_deployment(config, spec);
    return 0;
}
```

如果命令行传了：

```powershell
--print-config
```

它只打印配置，不启动服务。

注意：

```text
如果同时传 --print-config 和 --serve，会优先 print_config，然后 return 0。
```

### 13.8 game_auth_encrypted 分支

代码：

```cpp
if (game_auth_encrypted)
{
    if (role == RoleKind::v_server)
    {
        SocketRuntime runtime;
        cyber::game::TankGameServer server(runtime_bind_endpoint(config, spec), config,
                                            true);
        server.run();
        return 0;
    }
    if (role == RoleKind::client)
    {
        if (ui_port == 0)
        {
            throw std::runtime_error("client --game-auth-encrypted requires --ui-port");
        }
        cyber::game::TankGameClient client(config, ui_port);
        client.run();
        return 0;
    }
    throw std::runtime_error(
        "--game-auth-encrypted is only supported by v_server and client");
}
```

这个分支给最终游戏模式用。

V：

```powershell
v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

启动：

```text
TankGameServer
```

Client：

```powershell
client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

启动：

```text
TankGameClient
```

AS/TGS 如果传 `--game-auth-encrypted`，会报错。  
因为 AS/TGS 应该走 `--serve`。

### 13.9 serve 分支

代码：

```cpp
if (serve)
{
    runtime_run_auth_server(role, config, spec, max_connections);
}
else
{
    runtime_print_usage(spec);
    throw std::runtime_error("no runtime mode selected");
}
```

如果命令行传了：

```powershell
--serve
```

就启动认证服务器模式。

AS/TGS 进入：

```text
runtime_run_auth_server()
```

然后开始：

```text
监听端口 -> accept 连接 -> 创建 worker 线程 -> 分发到 AS/TGS 处理函数
```

如果没有传任何运行模式：

```text
没有 --serve
没有 --game-auth-encrypted
没有 --print-config
```

就打印 usage，然后报：

```text
no runtime mode selected
```

### 13.10 catch 统一处理运行错误

代码：

```cpp
catch (const std::exception& ex)
{
    std::cerr << spec.name << " failed: " << ex.what() << '\n';
    return 1;
}
```

人话：

```text
try 里面只要抛出异常，就统一打印 “角色 failed: 原因”。
```

比如：

```text
AS failed: failed to open config: ...
Client failed: client --game-auth-encrypted requires --ui-port
```

返回：

```cpp
return 1;
```

表示运行失败。

## 14. 多线程到底在哪里实现

多线程只在这个地方：

```cpp
std::thread worker(runtime_handle_server_connection, accepted, role, spec, config);
```

它位于：

```text
runtime_run_auth_server()
```

完整关系：

```text
runtime_run_auth_server()
  主线程 listen_tcp()
  主线程 accept_tcp()
  每 accept 到一个连接，就创建 std::thread worker

worker 线程运行 runtime_handle_server_connection()
  如果 role 是 AS，调用 as_process_connection()
  如果 role 是 TGS，调用 tgs_process_connection()
```

所以 AS 的多线程不是 `as_service.cpp` 自己开的。  
AS 的多线程是公共 runtime 帮它开的。

人话总结：

```text
AS 自己只关心怎么处理一个连接。
role_runtime.cpp 负责让 AS 能同时处理多个连接。
```

## 15. AS 正常模式和测试模式的区别

正常模式：

```powershell
as_server.exe --config .\config\course_config.txt --serve
```

`max_connections` 默认是 0。  
代码走：

```cpp
worker.detach();
```

效果：

```text
主线程不等 worker。
worker 自己处理 AS_REQ。
主线程继续 accept 下一个连接。
所以可以并发处理多个连接。
```

测试模式：

```powershell
as_server.exe --config .\config\course_config.txt --serve --max-connections 1
```

`max_connections > 0`。  
代码走：

```cpp
worker.join();
```

效果：

```text
主线程等待 worker 处理完。
处理够指定连接数后退出。
适合测试，不适合长期运行。
```

## 16. 这个文件和 AS/TGS/V/Client 的关系

### AS

```text
role_runtime.cpp
  -> 启动 AS
  -> 监听 AS_BIND_IP:AS_PORT
  -> 多线程接收连接
  -> 调 as_process_connection()
```

AS 业务逻辑在：

```text
src/roles/as/as_service.cpp
```

### TGS

```text
role_runtime.cpp
  -> 启动 TGS
  -> 监听 TGS_BIND_IP:TGS_PORT
  -> 多线程接收连接
  -> 调 tgs_process_connection()
```

TGS 业务逻辑在：

```text
src/roles/tgs/tgs_service.cpp
```

### V

V 不通过 `runtime_run_auth_server()` 处理最终游戏连接。  
V 通过：

```text
--game-auth-encrypted
TankGameServer server(...)
server.run()
```

启动。

V 业务逻辑在：

```text
src/roles/v/tank_game_server.cpp
src/roles/v/v_auth_service.cpp
```

### Client

Client 也不走 `runtime_run_auth_server()`。  
Client 通过：

```text
--game-auth-encrypted --ui-port 7001
TankGameClient client(...)
client.run()
```

启动。

Client 业务逻辑在：

```text
src/roles/client/tank_game_client.cpp
src/roles/client/client_auth_flow.cpp
src/roles/client/ui_bridge.cpp
```

## 17. 验收时可以这样讲这个文件

```text
role_runtime.cpp 是四个角色共用的运行框架。
它本身不实现 AS_REQ 具体怎么生成票据，也不实现 TGS 怎么验证票据。
它主要负责把不同角色按照命令行参数启动起来。

首先它通过 runtime_build_role_spec() 根据 RoleKind 生成 RoleSpec。
RoleSpec 里面保存角色名、程序名、实体 ID，以及要从 config 里读取哪些 key。
比如 AS 对应 AS_ID、AS_PORT、AS_IP、AS_BIND_IP。

然后 run_role_main() 解析命令行参数。
比如 --config 决定配置文件路径，--serve 表示启动 AS/TGS 这种认证服务器，
--game-auth-encrypted 表示启动 V 或 Client 的最终游戏模式，
--ui-port 是 Client WebSocket bridge 的端口。

配置加载完以后，它根据模式分支。
如果是 --serve，就调用 runtime_run_auth_server()。
这个函数负责 AS/TGS 的监听和多线程。
它先用 runtime_bind_endpoint() 从 config 里拿监听 IP 和端口，
然后 listen_tcp() 开始监听。
每次 accept_tcp() 收到一个连接，就创建 std::thread worker。
worker 线程执行 runtime_handle_server_connection()。
如果当前 role 是 AS，就调用 as_process_connection()；
如果当前 role 是 TGS，就调用 tgs_process_connection()。

所以 AS 的多线程不是写在 as_service.cpp 里，
而是 role_runtime.cpp 里统一实现的。
as_service.cpp 只负责处理一个连接里的 AS_REQ。
role_runtime.cpp 负责让它能被多个 worker 线程并发调用。
```

