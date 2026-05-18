# V Role

Startup:

```powershell
.\_generated\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

Owned files:

```text
src/roles/v/main.cpp
src/roles/v/v_auth_service.cpp
src/roles/v/tank_game_server.cpp
src/roles/v/battle_room.cpp
src/roles/v/game_world.cpp
include/cyber/roles/v/v_auth_service.hpp
include/cyber/roles/v/tank_game_server.hpp
```

Shared files usually modified for V behavior:

```text
src/shared/game/game_protocol.cpp
src/shared/game/app_payload_codec.cpp
src/shared/game/game_non_repudiation.cpp
src/shared/protocol/kerberos_messages.cpp
src/shared/protocol/certificate_messages.cpp
src/shared/protocol/app_envelope.cpp
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
V role header:           include/cyber/roles/v/tank_game_server.hpp
Tank/world rules:        src/roles/v/battle_room.cpp
Map, bullets, pickups:   src/roles/v/game_world.cpp
Game payload layout:     src/shared/game/game_protocol.cpp
Encryption/signature:    src/shared/game/app_payload_codec.cpp
Non-repudiation ACK:     src/shared/game/game_non_repudiation.cpp
```
