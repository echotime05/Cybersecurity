# Client Role

Startup:

```powershell
.\_generated\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

Owned files:

```text
src/roles/client/main.cpp
src/roles/client/client_auth_flow.cpp
src/roles/client/tank_game_client.cpp
src/roles/client/ui_bridge.cpp
src/roles/client/websocket.cpp
include/cyber/roles/client/client_auth_flow.hpp
include/cyber/roles/client/tank_game_client.hpp
web-ui/src/Game.ts
web-ui/src/Network.ts
web-ui/src/ProtocolMonitor.ts
web-ui/src/protocolPayload.ts
```

Shared files usually modified for Client behavior:

```text
src/shared/game/game_protocol.cpp
src/shared/game/app_payload_codec.cpp
src/shared/game/game_non_repudiation.cpp
src/shared/protocol/kerberos_messages.cpp
src/shared/protocol/certificate_messages.cpp
src/shared/protocol/app_envelope.cpp
include/cyber/ui/ui_bridge.hpp
include/cyber/ui/websocket.hpp
```

Responsibilities:

```text
Browser -> Client: login, join, move, target, shoot
Client -> AS/TGS/V: Kerberos and game packets
V -> Client: encrypted state and ACK packets
Client -> Browser: authenticated state, game state, protocol status
```

Client is the browser-facing local process. It receives UI commands through WebSocket, runs the AS/TGS/V authentication sequence, keeps the V TCP connection, signs/encrypts game payloads, verifies/decrypts V state, sends ACK evidence, and forwards render-ready JSON to the browser.

Common live modification points:

```text
Login/game command flow: src/roles/client/tank_game_client.cpp
AS/TGS/V auth sequence: src/roles/client/client_auth_flow.cpp
Client role header:     include/cyber/roles/client/tank_game_client.hpp
Browser bridge JSON:    src/roles/client/ui_bridge.cpp
WebSocket framing:      src/roles/client/websocket.cpp
Game UI behavior:       web-ui/src/Game.ts
Network UI client:      web-ui/src/Network.ts
Protocol panel:         web-ui/src/ProtocolMonitor.ts
```
