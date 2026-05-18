# V Role

Startup:

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

Owned files:

```text
src/roles/v/main.cpp
src/roles/v/tank_game_server.cpp
src/roles/v/battle_room.cpp
src/roles/v/game_world.cpp
```

Shared files usually modified for V behavior:

```text
src/shared/auth/auth_flow.cpp
src/shared/game/game_protocol.cpp
src/shared/game/app_payload_codec.cpp
src/shared/game/game_non_repudiation.cpp
src/shared/protocol/protocol_payloads.cpp
include/cyber/game/tank_game_server.hpp
include/cyber/game/battle_room.hpp
include/cyber/game/game_world.hpp
include/cyber/game/game_protocol.hpp
```

Responsibilities:

```text
Client -> V: MSG_V_AUTH_REQ, MSG_CERT_C2V, MSG_APP.*
V -> Client: MSG_V_AUTH_REP, MSG_CERT_V2C, MSG_APP.GAME_STATE, MSG_APP.APP_ACK
```

V owns the authoritative game world. It authenticates clients, stores client public keys, decrypts and verifies encrypted game payloads, applies move/target/shoot/join commands, advances the world every tick, broadcasts state, and writes V-side ACK evidence.

Common live modification points:

```text
Game input handling:     src/roles/v/tank_game_server.cpp
Tank/world rules:        src/roles/v/battle_room.cpp
Map, bullets, pickups:   src/roles/v/game_world.cpp
Game payload layout:     src/shared/game/game_protocol.cpp
Encryption/signature:    src/shared/game/app_payload_codec.cpp
Non-repudiation ACK:     src/shared/game/game_non_repudiation.cpp
```
