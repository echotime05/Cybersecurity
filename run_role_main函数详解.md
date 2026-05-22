# run_role_main 函数详解

对应文件：

```text
src/shared/runtime/role_runtime.cpp
函数：run_role_main(RoleKind role, int argc, char** argv)
```

这个函数是 AS、TGS、V、Client 四个角色共用的启动入口。  
你可以把它理解成“总启动器”：

```text
main.cpp 只告诉它：我要启动哪个角色。
run_role_main() 负责：
  1. 看当前角色是谁
  2. 解析命令行参数
  3. 读取 config
  4. 打印启动信息
  5. 根据参数选择真正启动哪种运行模式
```

## 1. 它是怎么被调用的

以 AS 为例：

```text
src/roles/as/main.cpp
```

里面调用：

```cpp
return cyber::run_role_main(cyber::RoleKind::as_server, argc, argv);
```

人话：

```text
AS 的 main.cpp 自己不处理 AS_REQ。
它只是把 RoleKind::as_server 交给 run_role_main()。
run_role_main() 看到 role 是 as_server，后面才知道自己要按 AS 的规则启动。
```

其他角色也是同一个套路：

```text
as_server  -> RoleKind::as_server
tgs_server -> RoleKind::tgs_server
v_server   -> RoleKind::v_server
client     -> RoleKind::client
```

所以这个函数不是 AS 独有的，而是四个角色共用。

## 2. 函数参数是什么意思

函数签名：

```cpp
int run_role_main(RoleKind role, int argc, char** argv)
```

### role

`role` 表示当前要启动哪个角色。

比如 AS 调用时传的是：

```cpp
RoleKind::as_server
```

人话：

```text
role 就是身份标签。
它告诉公共 runtime：这次运行的是 AS，还是 TGS，还是 V，还是 Client。
```

### argc

`argc` 是命令行参数个数。

比如你运行：

```powershell
.\as_server.exe --config .\config\course_config.txt --serve
```

大概可以理解成：

```text
argv[0] = .\as_server.exe
argv[1] = --config
argv[2] = .\config\course_config.txt
argv[3] = --serve
argc = 4
```

### argv

`argv` 是命令行参数内容。

函数后面的 `for` 循环就是一个个读 `argv[i]`，判断用户传了哪些参数。

## 3. 第一步：根据 role 生成 RoleSpec

代码：

```cpp
const RoleSpec spec = runtime_build_role_spec(role);
```

`RoleSpec` 可以理解成“角色说明书”。

它里面有：

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

如果 role 是 AS，`runtime_build_role_spec()` 返回：

```cpp
return {"AS", "as_server", EntityId::as, "AS_ID", "AS_PORT", "AS_IP", "AS_BIND_IP",
        "handle AS_REQ and return AS_REP"};
```

人话解释：

```text
name = "AS"
  -> 打印日志时显示的角色名。

binary = "as_server"
  -> 打印 usage 时显示程序名。

id = EntityId::as
  -> AS 这个实体的枚举 ID。

id_key = "AS_ID"
  -> 去 config 里读 AS_ID。

port_key = "AS_PORT"
  -> 去 config 里读 AS_PORT。

connect_ip_key = "AS_IP"
  -> 别人连接 AS 时用的 IP key。

bind_ip_key = "AS_BIND_IP"
  -> AS 自己监听时绑定的 IP key。

stage_goal = "handle AS_REQ and return AS_REP"
  -> 打印给人看的阶段目标。
```

重点注意：

```text
RoleSpec 里放的 "AS_PORT"、"AS_IP" 不是端口和 IP 的真实值。
它只是告诉代码：后面要去 config 里用这些 key 取值。
```

也就是：

```text
RoleSpec 是说明书。
Config 才是真正的数据表。
```

## 4. 第二步：初始化几个控制变量

代码：

```cpp
bool print_config = false;
bool serve = false;
bool game_auth_encrypted = false;
std::uint16_t ui_port = 0;
int max_connections = 0;
std::filesystem::path config_path;
```

这些变量都是先给默认值，后面根据命令行参数再改。

### print_config

```cpp
bool print_config = false;
```

作用：

```text
是否只打印配置，不真正启动服务。
```

如果命令行有：

```powershell
--print-config
```

它就变成：

```cpp
print_config = true;
```

后面会进入：

```cpp
if (print_config)
{
    config_print_deployment(config, spec);
    return 0;
}
```

人话：

```text
用户只是想看当前配置，不想真的启动 AS/TGS/V/Client。
打印完就返回 0，程序结束。
```

### serve

```cpp
bool serve = false;
```

作用：

```text
是否以认证服务器模式启动。
```

AS 和 TGS 启动时常用：

```powershell
--serve
```

解析到这个参数后：

```cpp
serve = true;
```

后面会进入：

```cpp
if (serve)
{
    runtime_run_auth_server(role, config, spec, max_connections);
}
```

人话：

```text
serve 为 true，才会真正开始监听端口，等待 Client 连接。
```

### game_auth_encrypted

```cpp
bool game_auth_encrypted = false;
```

作用：

```text
是否启动最终的加密认证游戏模式。
```

这个主要给：

```text
v_server
client
```

使用。

命令行：

```powershell
--game-auth-encrypted
```

解析到后：

```cpp
game_auth_encrypted = true;
```

后面如果 role 是 `v_server`，会启动 `TankGameServer`。  
如果 role 是 `client`，会启动 `TankGameClient`。

注意：

```text
AS 和 TGS 不用 --game-auth-encrypted。
AS/TGS 用 --serve。
V/Client 用 --game-auth-encrypted。
```

### ui_port

```cpp
std::uint16_t ui_port = 0;
```

作用：

```text
Client 的网页 UI/WebSocket 端口。
```

Client 启动时会用：

```powershell
--ui-port 7001
```

解析到后：

```cpp
ui_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
```

如果启动 Client 的 `--game-auth-encrypted` 模式，但是没有传 `--ui-port`，后面会报错：

```cpp
throw std::runtime_error("client --game-auth-encrypted requires --ui-port");
```

人话：

```text
Client 需要一个端口给网页连上来。
所以 Client 的加密游戏模式必须带 --ui-port。
```

### max_connections

```cpp
int max_connections = 0;
```

作用：

```text
认证服务器最多处理几个连接。
```

命令行：

```powershell
--max-connections 1
```

解析后：

```cpp
max_connections = std::stoi(argv[++i]);
```

如果小于等于 0，函数直接返回错误：

```cpp
if (max_connections <= 0)
{
    std::cerr << "--max-connections must be positive\n";
    return 2;
}
```

默认值是 0，意思是：

```text
不限制连接数，一直服务。
```

但是如果用户主动写了 `--max-connections`，就必须是正数。

### config_path

```cpp
std::filesystem::path config_path;
```

作用：

```text
记录 config 文件路径。
```

如果命令行有：

```powershell
--config .\config\course_config.txt
```

就会执行：

```cpp
config_path = argv[++i];
```

如果用户没传 `--config`，它会一直是空路径。  
后面会自动找默认配置文件。

## 5. 第三步：循环解析命令行参数

代码：

```cpp
for (int i = 1; i < argc; ++i)
{
    const std::string arg = argv[i];
    ...
}
```

为什么 `i` 从 1 开始？

```text
argv[0] 是程序自己，比如 as_server.exe。
真正的用户参数从 argv[1] 开始。
```

每次循环：

```cpp
const std::string arg = argv[i];
```

就是把当前参数拿出来，后面用 `if/else if` 判断它是什么。

## 6. 参数一：--print-config

代码：

```cpp
if (arg == "--print-config")
{
    print_config = true;
}
```

人话：

```text
如果用户传了 --print-config，就设置标记。
现在不马上打印，因为 config 还没有加载。
等后面 Config::load() 完成，才能真正打印配置。
```

## 7. 参数二：--serve

代码：

```cpp
else if (arg == "--serve")
{
    serve = true;
}
```

人话：

```text
用户要求以认证服务器方式启动。
AS/TGS 一般会用这个参数。
```

比如 AS：

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

## 8. 参数三：--max-connections

代码：

```cpp
else if (arg == "--max-connections")
{
    if (i + 1 >= argc)
    {
        std::cerr << "--max-connections requires a number\n";
        return 2;
    }
    max_connections = std::stoi(argv[++i]);
    if (max_connections <= 0)
    {
        std::cerr << "--max-connections must be positive\n";
        return 2;
    }
}
```

这个参数后面必须跟一个数字。  
比如：

```powershell
--max-connections 1
```

### 为什么要判断 `i + 1 >= argc`

如果用户只写：

```powershell
--max-connections
```

后面没有数字，那么 `argv[++i]` 就会越界。  
所以代码先检查后面还有没有参数。

### `argv[++i]` 是什么意思

比如：

```text
argv[3] = --max-connections
argv[4] = 1
```

当前 `i` 是 3。  
`++i` 先把 `i` 变成 4，再读取 `argv[4]`。

所以：

```cpp
max_connections = std::stoi(argv[++i]);
```

意思是：

```text
跳到 --max-connections 后面的那个参数，把它转成整数。
```

### 返回 2 是什么意思

在这个函数里：

```text
return 0 代表正常结束。
return 1 代表运行时异常。
return 2 代表命令行参数写错。
```

`--max-connections` 缺少数字，属于参数写错，所以返回 2。

## 9. 参数四：--game-auth-encrypted

代码：

```cpp
else if (arg == "--game-auth-encrypted")
{
    game_auth_encrypted = true;
}
```

人话：

```text
启动最终加密认证游戏模式。
```

它主要给 V 和 Client 用。

V 启动示例：

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

Client 启动示例：

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

## 10. 参数五：--ui-port

代码：

```cpp
else if (arg == "--ui-port")
{
    if (i + 1 >= argc)
    {
        std::cerr << "--ui-port requires a port\n";
        return 2;
    }
    ui_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
}
```

这个参数也必须跟一个数字。

比如：

```powershell
--ui-port 7001
```

人话：

```text
告诉 Client 的 UI 桥接服务开在哪个端口。
网页会通过这个端口和 client.exe 通信。
```

注意：

```text
代码这里只把 stoi 的结果强转成 uint16_t。
它没有额外检查端口是否超过 65535。
```

所以验收时如果老师问严谨性，可以说：

```text
当前代码检查了有没有传端口，但没有显式检查端口范围。
```

## 11. 参数六：--config

代码：

```cpp
else if (arg == "--config")
{
    if (i + 1 >= argc)
    {
        std::cerr << "--config requires a path\n";
        return 2;
    }
    config_path = argv[++i];
}
```

命令行示例：

```powershell
--config .\config\course_config.txt
```

人话：

```text
告诉程序配置文件在哪里。
```

如果用户写了 `--config` 但是后面没跟路径：

```powershell
.\as_server.exe --config --serve
```

这时 `--serve` 会被当成路径吗？

当前代码只检查后面有没有参数，不检查后面的参数是不是另一个选项。  
所以从代码角度看，`--serve` 会被拿来当作 config_path。  
后面 `Config::load("--serve")` 会失败。

正常写法应该是：

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

## 12. 参数七：--help 或 -h

代码：

```cpp
else if (arg == "--help" || arg == "-h")
{
    runtime_print_usage(spec);
    return 0;
}
```

人话：

```text
打印使用方法，然后正常退出。
```

因为只是查看帮助，不是错误，所以返回 0。

`runtime_print_usage(spec)` 会打印：

```text
Usage: as_server [--config PATH] [--print-config] [--serve] [--max-connections N] [--game-auth-encrypted] [--ui-port PORT]
```

如果是 Client，就会显示：

```text
Usage: client ...
```

这里用的是 `spec.binary`。

## 13. 未知参数

代码：

```cpp
else
{
    std::cerr << "unknown argument: " << arg << '\n';
    runtime_print_usage(spec);
    return 2;
}
```

人话：

```text
用户传了代码不认识的参数。
打印错误和用法，然后返回 2。
```

比如写错：

```powershell
--server
```

代码只认识：

```powershell
--serve
```

所以会进入 unknown argument。

## 14. 第四步：如果没传 config，就找默认路径

代码：

```cpp
if (config_path.empty())
{
    config_path = config_find_default_path();
}
```

如果用户没有写：

```powershell
--config ...
```

`config_path` 还是空的。  
这时调用：

```cpp
config_find_default_path()
```

它会按顺序找：

```text
config/course_config.txt
../config/course_config.txt
../../config/course_config.txt
```

哪个先存在，就用哪个。  
如果都不存在，就返回第一个：

```text
config/course_config.txt
```

后面再由 `Config::load(config_path)` 去真正打开。  
如果文件不存在，`Config::load()` 会抛异常。

人话：

```text
传了 --config 就用用户指定的。
没传 --config 就自动找默认配置。
```

## 15. 第五步：进入 try，开始真正加载和运行

代码：

```cpp
try
{
    const Config config = Config::load(config_path);
    ...
}
catch (const std::exception& ex)
{
    std::cerr << spec.name << " failed: " << ex.what() << '\n';
    return 1;
}
```

从这里开始，属于真正运行阶段。  
如果里面任何地方抛出 `std::exception`，都会被 catch 捕获。

错误时打印：

```text
AS failed: ...
```

或者：

```text
Client failed: ...
```

因为用的是：

```cpp
spec.name
```

然后返回 1。

## 16. 加载 config

代码：

```cpp
const Config config = Config::load(config_path);
```

人话：

```text
读取 config 文件，把里面的 key=value 加载到 Config 对象里。
```

比如：

```text
AS_PORT=9001
AS_BIND_IP=0.0.0.0
C1_KC=...
KTGS=...
```

后面都通过 `config.get_string()`、`config.get_u16()`、`config.get_u64()` 来取。

## 17. 设置协议日志根目录

代码：

```cpp
set_protocol_event_log_root(log_root_from_config(config));
```

人话：

```text
告诉协议日志系统：后面收发包日志写到哪个目录下面。
```

这会影响：

```text
send_packet_logged()
recv_packet_logged()
write_protocol_event()
```

也就是说，AS/TGS/Client/V 后面记录协议事件时，会用这里设置的日志目录。

## 18. 打印基础启动信息

代码：

```cpp
std::cout << spec.name << " role loaded config: " << config_path.string() << '\n';
```

人话：

```text
打印当前角色加载了哪个配置文件。
```

比如：

```text
AS role loaded config: .\config\course_config.txt
```

接着：

```cpp
std::cout << "entity id: 0x" << std::hex
          << static_cast<int>(config.get_entity_id(spec.id_key)) << std::dec << '\n';
```

这里根据 `spec.id_key` 去 config 里取实体 ID。

AS 的 `spec.id_key` 是：

```text
AS_ID
```

所以 AS 会读：

```text
config.get_entity_id("AS_ID")
```

人话：

```text
打印当前角色在协议里的实体 ID。
```

`std::hex` 表示按十六进制打印，`std::dec` 表示打印完切回十进制。

## 19. 打印端口或本地 Client ID

代码：

```cpp
if (spec.port_key != nullptr)
{
    std::cout << "listen port: " << config.get_u16(spec.port_key) << '\n';
}
else
{
    std::cout << "local client id: " << to_string(config.get_entity_id("LOCAL_CLIENT_ID"))
              << '\n';
}
```

这里分两种角色。

### 服务器角色

AS/TGS/V 的 `spec.port_key` 都不是空。

比如 AS：

```text
spec.port_key = "AS_PORT"
```

所以会打印：

```text
listen port: 9001
```

### Client 角色

Client 的 RoleSpec 是：

```cpp
return {"Client", "client", EntityId::unknown, "LOCAL_CLIENT_ID", nullptr, nullptr,
        nullptr,
        "run AS, TGS, V_AUTH, certificate exchange, then game events"};
```

Client 没有监听端口 key，所以：

```text
spec.port_key = nullptr
```

于是进入 `else`：

```cpp
config.get_entity_id("LOCAL_CLIENT_ID")
```

打印本地 Client ID。

人话：

```text
服务器打印监听端口。
Client 打印本地客户端身份。
```

## 20. 打印 stage goal

代码：

```cpp
std::cout << "stage goal: " << spec.stage_goal << '\n';
```

人话：

```text
打印当前角色的阶段目标。
```

AS 的目标是：

```text
handle AS_REQ and return AS_REP
```

这个只是给人看的说明，不影响逻辑。

## 21. 分支一：只打印配置

代码：

```cpp
if (print_config)
{
    config_print_deployment(config, spec);
    return 0;
}
```

如果前面解析到了：

```powershell
--print-config
```

就进入这里。

`config_print_deployment()` 会打印：

```text
当前角色
本地 Client ID
AS connect/listen 地址
TGS connect/listen 地址
V connect/listen 地址
当前服务器监听地址
```

然后：

```cpp
return 0;
```

人话：

```text
用户只是看配置。
打印完就正常退出，不启动服务。
```

注意：

```text
即使同时传了 --serve 和 --print-config，也会先走 print_config，然后 return 0。
也就是说 --print-config 优先级更高。
```

## 22. 分支二：启动加密认证游戏模式

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

如果命令行有：

```powershell
--game-auth-encrypted
```

就进入这个分支。

### role 是 v_server

代码：

```cpp
SocketRuntime runtime;
cyber::game::TankGameServer server(runtime_bind_endpoint(config, spec), config, true);
server.run();
return 0;
```

人话：

```text
启动 V 游戏服务器。
runtime_bind_endpoint(config, spec) 用 V_BIND_IP 和 V_PORT 得到监听地址。
true 表示开启认证加密模式。
server.run() 开始运行。
```

### role 是 client

代码：

```cpp
if (ui_port == 0)
{
    throw std::runtime_error("client --game-auth-encrypted requires --ui-port");
}
cyber::game::TankGameClient client(config, ui_port);
client.run();
return 0;
```

人话：

```text
启动 Client 游戏客户端。
它需要 ui_port，因为网页 UI 要连到这个端口。
如果没有传 --ui-port，就抛异常。
```

### role 是 AS 或 TGS

如果 AS/TGS 也传了：

```powershell
--game-auth-encrypted
```

就会走到：

```cpp
throw std::runtime_error(
    "--game-auth-encrypted is only supported by v_server and client");
```

人话：

```text
AS/TGS 不支持这个模式。
AS/TGS 应该用 --serve。
```

## 23. 分支三：启动 AS/TGS 认证服务

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

如果前面解析到了：

```powershell
--serve
```

就进入：

```cpp
runtime_run_auth_server(role, config, spec, max_connections);
```

人话：

```text
启动认证服务器。
AS 会监听 AS_BIND_IP:AS_PORT。
TGS 会监听 TGS_BIND_IP:TGS_PORT。
收到连接后，后面会转到 as_process_connection() 或 tgs_process_connection()。
```

AS 启动命令：

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

TGS 启动命令：

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

如果还传了：

```powershell
--max-connections 1
```

`runtime_run_auth_server()` 会把这个值用于控制最多处理几个连接。

## 24. 没选任何模式会怎样

如果用户既没传：

```powershell
--serve
```

也没传：

```powershell
--game-auth-encrypted
```

也不是：

```powershell
--print-config
```

就会进入：

```cpp
runtime_print_usage(spec);
throw std::runtime_error("no runtime mode selected");
```

人话：

```text
程序不知道你到底想干嘛。
所以打印用法，然后抛异常。
```

异常会被外层 catch 捕获，打印：

```text
AS failed: no runtime mode selected
```

然后返回 1。

## 25. catch：统一处理运行错误

代码：

```cpp
catch (const std::exception& ex)
{
    std::cerr << spec.name << " failed: " << ex.what() << '\n';
    return 1;
}
```

这里捕获 try 里面抛出的异常。

比如：

```text
配置文件不存在
缺少 config key
端口被占用
Client 没有传 --ui-port
AS/TGS 用错 --game-auth-encrypted
没有选择运行模式
```

都会走到这里。

人话：

```text
统一打印“哪个角色失败了”和失败原因。
```

比如：

```text
AS failed: failed to open config: config/course_config.txt
```

返回：

```cpp
return 1;
```

表示运行失败。

## 26. 最后 return 0

代码：

```cpp
return 0;
```

如果 `try` 里面顺利执行完，并且没有在中途 return，就会走到这里。

不过实际情况里：

```text
--print-config 分支会 return 0。
--game-auth-encrypted 的 V/Client 分支会 return 0。
--serve 分支一般会进入长期运行，除非 max_connections 让它结束。
```

所以最后这个 `return 0` 是兜底的正常结束返回。

## 27. run_role_main 的整体流程图

```text
run_role_main(role, argc, argv)
  |
  v
runtime_build_role_spec(role)
  |
  v
初始化 print_config / serve / game_auth_encrypted / ui_port / max_connections / config_path
  |
  v
循环解析 argv
  |
  +-- --print-config        -> print_config = true
  +-- --serve               -> serve = true
  +-- --max-connections N   -> max_connections = N
  +-- --game-auth-encrypted -> game_auth_encrypted = true
  +-- --ui-port PORT        -> ui_port = PORT
  +-- --config PATH         -> config_path = PATH
  +-- --help / -h           -> 打印 usage，return 0
  +-- 未知参数              -> 打印 usage，return 2
  |
  v
如果 config_path 为空 -> config_find_default_path()
  |
  v
Config::load(config_path)
  |
  v
set_protocol_event_log_root(...)
  |
  v
打印角色、entity id、端口或 local client id、stage goal
  |
  v
if print_config
  -> 打印部署配置，return 0
  |
  v
if game_auth_encrypted
  -> role 是 V：启动 TankGameServer
  -> role 是 Client：检查 ui_port，启动 TankGameClient
  -> role 是 AS/TGS：抛异常
  |
  v
if serve
  -> runtime_run_auth_server(role, config, spec, max_connections)
else
  -> 打印 usage，抛 no runtime mode selected
```

## 28. 按角色记启动方式

### AS

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

对应 `run_role_main()` 里的分支：

```text
role = as_server
serve = true
runtime_run_auth_server()
```

### TGS

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

对应分支：

```text
role = tgs_server
serve = true
runtime_run_auth_server()
```

### V

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

对应分支：

```text
role = v_server
game_auth_encrypted = true
TankGameServer::run()
```

### Client

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

对应分支：

```text
role = client
game_auth_encrypted = true
ui_port = 7001
TankGameClient::run()
```

## 29. 验收时可以这样讲

```text
run_role_main 是四个角色共用的启动函数。
它先根据传进来的 RoleKind 生成 RoleSpec，也就是当前角色的说明书。
RoleSpec 里记录这个角色的名字、程序名、实体 ID，以及要从 config 读取哪些 key。

然后它初始化几个控制变量，比如 serve、print_config、game_auth_encrypted、ui_port、max_connections 和 config_path。
接着用 for 循环解析命令行参数。
比如 --serve 会让 serve 变成 true，--config 后面的值会放进 config_path，
--ui-port 后面的数字会放进 ui_port。

参数解析完以后，如果没有传 config 路径，它就调用 config_find_default_path() 找默认配置。
然后通过 Config::load(config_path) 加载配置文件，并设置协议日志目录。
接着它打印当前角色、实体 ID、监听端口或本地 Client ID，还有阶段目标。

后面它根据用户选择的运行模式分支。
如果是 --print-config，就只打印配置然后退出。
如果是 --game-auth-encrypted，只有 V 和 Client 支持：
V 会启动 TankGameServer，Client 会检查 ui_port 后启动 TankGameClient。
如果是 --serve，就调用 runtime_run_auth_server，AS/TGS 会在这里开始监听端口。
如果什么模式都没选，就打印 usage 并报 no runtime mode selected。

所以这个函数本身不处理 AS_REQ 或 TGS_REQ。
它负责把程序启动到正确模式。
真正 AS 的业务处理是在 as_process_connection()。
```

