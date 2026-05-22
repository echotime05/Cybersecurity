# TGS Role

启动方式

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

所属文件:

```text
src/roles/tgs/main.cpp
src/roles/tgs/tgs_service.cpp
include/cyber/roles/tgs/tgs_service.hpp
```

为 TGS 行为而通常需要修改的共享文件：

```text
src/shared/protocol/kerberos_messages.cpp
src/shared/crypto/crypto.cpp
include/cyber/protocol/kerberos_messages.hpp
```

职责：

```text
Client -> TGS: MSG_TGS_REQ
TGS -> Client: MSG_TGS_REP or MSG_ERROR
```

TGS 解密 Ticket_tgs，验证 Authenticator_tgs，检查所请求的服务 V，生成 Kc_v，构建 Ticket_v，加密 TGS 回复，并将协议事件写入 UI。

常见的实机演示修改点：

```text
TGS 数据包处理：      src/roles/tgs/tgs_service.cpp
Ticket_v 字段布局：   src/shared/protocol/kerberos_messages.cpp
重放/时间验证：       src/roles/tgs/tgs_service.cpp
角色命令行/启动：     src/shared/runtime/role_runtime.cpp
```
