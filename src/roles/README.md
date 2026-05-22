# 角色源代码布局

运行时可执行程序在此处保留精简的角色入口点：

```text
src/roles/as/main.cpp
src/roles/tgs/main.cpp
src/roles/v/main.cpp
src/roles/client/main.cpp
src/roles/monitor/main.cpp
```

角色自身的行为代码放在各自角色目录下，以便在实机演示时能够方便地进行修改。跨角色的协议、加密、网络、日志记录以及运行时辅助功能则位于 src/shared 目录下。

```text
include/cyber/roles/as/        AS 公共角色头文件
include/cyber/roles/tgs/       TGS 公共角色头文件
include/cyber/roles/v/         V 公共角色头文件
include/cyber/roles/client/    客户端公共角色头文件
src/roles/as/README.md       AS 代码归属指南
src/roles/tgs/README.md      TGS 代码归属指南
src/roles/v/README.md        V/游戏服务器代码归属指南
src/roles/client/README.md   客户端/WebSocket桥接代码归属指南
src/roles/monitor/README.md  协议监控器代码归属指南
src/shared/README.md         共享源代码指南
```

可执行程序名称保持不变：

```text
as_server.exe
tgs_server.exe
v_server.exe
client.exe
monitor.exe
```
