# Role Source Layout

Runtime executables keep thin role entry points here:

```text
src/roles/as/main.cpp
src/roles/tgs/main.cpp
src/roles/v/main.cpp
src/roles/client/main.cpp
src/roles/monitor/main.cpp
```

Role-owned behavior lives with the role when it is practical to modify during a live demo. Cross-role protocol, crypto, network, logging, and runtime helpers live under `src/shared`.

```text
src/roles/as/README.md       AS ownership guide
src/roles/tgs/README.md      TGS ownership guide
src/roles/v/README.md        V/game-server ownership guide
src/roles/client/README.md   Client/WebSocket bridge ownership guide
src/roles/monitor/README.md  Protocol monitor ownership guide
src/shared/README.md         Shared source guide
```

Executables remain unchanged:

```text
as_server.exe
tgs_server.exe
v_server.exe
client.exe
monitor.exe
```
