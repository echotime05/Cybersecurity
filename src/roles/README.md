# Role Source Layout

Each runtime executable has its own thin entry directory:

```text
src/roles/client/main.cpp
src/roles/as/main.cpp
src/roles/tgs/main.cpp
src/roles/v/main.cpp
src/roles/monitor/main.cpp
```

Role directories should stay thin. They own only role startup and should call shared code from `src/common` through headers in `include/cyber/common`.

Current executables remain unchanged:

```text
client.exe
as_server.exe
tgs_server.exe
v_server.exe
monitor.exe
```

Ownership guide:

- Client owner: `src/roles/client`, `src/game/tank_game_client.cpp`, `src/ui`, and Client-facing authentication in `src/common/auth_flow.cpp`.
- AS owner: `src/roles/as` plus AS request handling in `src/common/auth_flow.cpp`.
- TGS owner: `src/roles/tgs` plus TGS request handling in `src/common/auth_flow.cpp`.
- V owner: `src/roles/v`, `src/game/tank_game_server.cpp`, `src/game/battle_room.cpp`, `src/game/game_world.cpp`, and V authentication helpers in `src/common/auth_flow.cpp`.
- Monitor owner: `src/roles/monitor`, `src/monitor`, and protocol event parsing in `src/common/protocol_event.cpp`.
- Shared protocol owner: `include/cyber/common`, `src/common`, `include/cyber/game`, `src/game`, and `tests`.
