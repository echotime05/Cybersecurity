# Phase A: Plaintext Tank Battle Application Layer Design

## Purpose

Phase A builds the playable tank battle application layer before integrating Kerberos, encryption, signatures, or non-repudiation. The goal is to prove the realtime game loop, player input, authoritative V server state, and Web UI rendering work end to end.

The project keeps the existing C++ `Packet` header as the outer transport format, but the application payload is redesigned to follow the semantics of `colyseus/realtime-tanks-demo`: `join`, `move`, `target`, `shoot`, `name`, and `state`.

## Scope

In scope:

- Browser-based Three.js UI derived from `realtime-tanks-demo/web-threejs`.
- Local C++ client process acting as the real game client.
- Plaintext TCP long connection between C++ client and C++ V server.
- Authoritative C++ V server game world based on `realtime-tanks-demo/server/src/rooms/BattleRoom.ts`.
- Full-state broadcasts from V to every connected client.
- Local UI bridge from browser to C++ client.

Out of scope for Phase A:

- AS/TGS authentication flow.
- DES encryption.
- RSA signatures.
- Signed acknowledgements.
- Replay prevention and `seq_no`.
- Incremental state diffing.

Those security features will wrap the same `GameMessage` payloads in later phases.

## Process Architecture

Phase A uses this process layout:

```text
Browser UI <-> Local C++ Client <-> Plaintext TCP <-> C++ V
```

Each player runs a local browser UI and a C++ client:

```text
Host 1:
  browser UI 1
  client.exe --game-plain --ui-port 7001

Host 2:
  browser UI 2
  client.exe --game-plain --ui-port 7002

Host 3:
  browser UI 3
  client.exe --game-plain --ui-port 7003

Host 4:
  browser UI 4
  client.exe --game-plain --ui-port 7004
  v_server.exe --game-plain
```

Responsibilities:

- Browser UI renders the battlefield, captures keyboard and mouse input, plays sound, and displays HUD state.
- C++ client owns the player identity and network connection to V. It converts UI messages into game protocol messages and converts V state snapshots into UI JSON.
- C++ V is the authoritative game server. It owns all tank, bullet, pickable, team, collision, score, respawn, and win-state logic.
- AS, TGS, encryption, and non-repudiation components are not used in Phase A.

## Transport Boundary

The existing outer packet header remains unchanged:

```text
PacketHeader:
  msg_type = MsgType::app
  src = Client1..Client4
  dst = V
  payload_len
  reserved
```

Only `payload` is redesigned. The payload contains a plaintext `GameMessage`:

```text
GameMessage:
  game_msg_type: uint8
  game_payload: bytes
```

All multi-byte numeric fields in `GameMessage` payloads use the same network byte order style as the existing protocol payload builders: big-endian integers. `float32` values are encoded as IEEE-754 binary32 bits and written as big-endian `uint32`. Variable-length byte fields are length-prefixed and must be rejected if the encoded length exceeds the remaining payload.

Suggested game message types:

```cpp
enum class GameMsgType : std::uint8_t {
    join = 1,
    move = 2,
    target = 3,
    shoot = 4,
    name = 5,
    state = 16,
    error = 127
};
```

This intentionally mirrors the Colyseus demo message names:

```ts
room.send("move", { x, y })
room.send("target", angle)
room.send("shoot", shooting)
room.send("name", name)
```

Existing `AppCode::key_down`, `AppCode::key_up`, `AppCode::aim_event`, and `AppCode::fire_event` do not need to drive Phase A. They may remain for compatibility, but the Phase A game protocol should prefer `GameMsgType` inside `MsgType::app`.

## Client-to-V Messages

`join`:

```text
client_id: uint8
name_len: uint8
name: bytes
```

`move`:

```text
x: int8    // -1, 0, 1
y: int8    // -1, 0, 1
```

`target`:

```text
angle: float32
```

`shoot`:

```text
shooting: uint8   // 0 or 1
```

`name`:

```text
name_len: uint8
name: bytes
```

V validates message size, numeric ranges, known client identity, and whether the client has joined before applying the message to the game world.

## V-to-Client State

V broadcasts full `BattleState` snapshots. This follows the demo schema but uses explicit C++ serialization instead of Colyseus schema diffing.

Snapshot shape:

```text
state:
  server_time_ms: uint64
  total_score: uint16
  winner_team: int8
  team_count: uint8
  teams: TeamState[team_count]
  tank_count: uint8
  tanks: TankState[tank_count]
  bullet_count: uint16
  bullets: BulletState[bullet_count]
  pickable_count: uint16
  pickables: PickableState[pickable_count]
```

`TeamState`:

```text
team_id: uint8
score: uint16
tanks: uint8
```

`TankState`:

```text
client_id: uint8
name_len: uint8
name: bytes
team: uint8
x: float32
y: float32
angle: float32
hp: int8
shield: int8
dead: uint8
score: uint16
```

`BulletState`:

```text
id: uint16
owner_client_id: uint8
x: float32
y: float32
special: uint8
```

`PickableState`:

```text
id: uint16
type: uint8   // repair, damage, shield
x: float32
y: float32
```

Phase A uses full snapshots every tick. Four players and a small number of entities keep the packet size low enough that diffing is unnecessary.

## V Game World

The V server game world is a C++ port of the demo's `BattleRoom`.

Recommended modules:

```text
include/cyber/game/game_types.hpp
src/game/game_types.cpp
  TankState
  BulletState
  PickableState
  TeamState
  BattleState
  GameInput

include/cyber/game/game_world.hpp
src/game/game_world.cpp
  Block
  spatial index
  collision helpers

include/cyber/game/game_protocol.hpp
src/game/game_protocol.cpp
  build/parse GameMessage
  build/parse BattleState snapshot

include/cyber/game/battle_room.hpp
src/game/battle_room.cpp
  join
  leave
  handle_move
  handle_target
  handle_shoot
  handle_name
  tick
  snapshot
```

Gameplay logic should match the demo:

- Four teams.
- Pick the weakest team on join.
- Spawn tanks from the demo's team-based spawn logic.
- Use the demo `LEVEL` block layout.
- Use the demo pickable spawn points.
- Tick at 20 FPS, approximately every 50 ms.
- Apply movement from each tank's latest direction.
- Apply target angle and shooting state from each client's latest input.
- Resolve tank-tank, tank-block, tank-pickable, bullet-tank, and bullet-block collisions.
- Handle reload, damage, shield, repair, respawn, score, and winner reset.

Phase A can port these constants directly:

```text
TANK_SPEED
TANK_RANGE
TANK_RADIUS
BULLET_SPEED
BULLET_RADIUS
BULLET_DAMAGE
PICKABLE_RADIUS
RESPAWN_TIME
INVULN_TIME
RELOAD_TIME
RECOVERY_DELAY
RECOVERY_INTERVAL
WIN_SCORE
```

## V Concurrency Model

V runs three kinds of work:

```text
V main thread:
  listen and accept game clients

V client worker thread:
  read GameMessage from one client
  call BattleRoom handlers
  detect disconnect and remove the tank

V game thread:
  every 50 ms:
    BattleRoom.tick()
    snapshot = BattleRoom.snapshot()
    send snapshot to all connected clients
```

`BattleRoom` is shared state and must be protected by a mutex. The connection table also needs a mutex, because worker threads add/remove clients while the game thread broadcasts.

The game thread should not hold the room mutex while sending packets. It should copy/serialize the snapshot first, release the room lock, then send the same bytes to connected clients.

## C++ Client and UI Bridge

The C++ client has three internal responsibilities:

```text
UiBridge:
  listen on ws://127.0.0.1:<ui_port>
  receive UI JSON input
  send state JSON to UI

VConnection:
  maintain plaintext TCP long connection to V
  send GameMessage payloads
  receive state snapshots

ClientController:
  UI input -> GameMessage -> V
  V state -> JSON -> UI
```

UI-to-client JSON:

```json
{ "type": "join", "name": "p1" }
{ "type": "move", "x": 1, "y": 0 }
{ "type": "target", "angle": 135.5 }
{ "type": "shoot", "shooting": true }
{ "type": "name", "name": "p1" }
```

Client-to-UI JSON:

```json
{
  "type": "state",
  "self": 1,
  "state": {
    "serverTimeMs": 0,
    "totalScore": 0,
    "winnerTeam": -1,
    "teams": [],
    "tanks": [],
    "bullets": [],
    "pickables": []
  }
}
```

The local UI bridge is intentionally outside the course security protocol. It is a local presentation bridge, not the trusted network participant. The C++ client remains the trusted player endpoint.

If a C++ WebSocket implementation becomes too large for Phase A, a temporary local Node bridge may be used:

```text
Browser UI <-> Node WebSocket bridge <-> C++ Client localhost TCP/stdin
```

That bridge is a fallback only. The main design keeps the UI bridge in the C++ client.

## Web UI Migration

The Web UI should come from `realtime-tanks-demo/web-threejs`.

Keep:

- Three.js rendering structure.
- Tank model and `TankEntity`.
- Map rendering.
- Sound handling.
- Keyboard and mouse input collection.
- HUD layout.

Replace:

- `Network.ts`, which currently uses Colyseus `Client` and `joinOrCreate("battle")`.
- Colyseus `Callbacks`-based state listeners in `Game.ts`.

New `Network.ts` connects to the local C++ client:

```ts
const socket = new WebSocket("ws://127.0.0.1:7001");
socket.send(JSON.stringify({ type: "move", x, y }));
socket.send(JSON.stringify({ type: "target", angle }));
socket.send(JSON.stringify({ type: "shoot", shooting }));
```

`Game.ts` reconciles full snapshots:

- Create tank entities for new tank ids.
- Update target positions, angles, health, shield, dead state, and score for existing tanks.
- Remove tank entities missing from the snapshot.
- Create/update/remove bullets.
- Create/update/remove pickables.
- Update score HUD and winner overlay.

## Error Handling

Phase A error handling stays minimal but explicit:

- Reject unknown `GameMsgType`.
- Reject malformed payload lengths.
- Reject input from clients that have not joined.
- Clamp movement to `-1, 0, 1`.
- Ignore invalid target angle values.
- Reject names outside the same style as the demo: ASCII letters, digits, hyphen, underscore, length 4 to 8.
- Remove a client from `BattleRoom` on socket disconnect.
- Log connection, join, leave, malformed message, and broadcast errors.

## Testing

C++ self-tests:

- `GameMessage` build/parse roundtrip.
- `BattleState` snapshot build/parse roundtrip.
- `BattleRoom` join assigns teams.
- `move` changes tank position after a tick.
- `shoot` creates a bullet after reload allows it.
- bullet collision reduces HP.
- dead tank respawns after `RESPAWN_TIME`.

Integration test:

```text
start v_server --game-plain
start two mock clients
client1 sends join + move
client2 sends join + shoot
both clients receive state snapshots
state contains both tanks
```

Manual UI test:

- Start V.
- Start one or more clients with UI ports.
- Open Web UI for each client.
- Move, aim, shoot, and observe synchronized state on every UI.

## Acceptance Criteria

Phase A is complete when:

- `v_server.exe --game-plain` starts a plaintext authoritative game server.
- `client.exe --game-plain --ui-port <port>` connects to V and exposes a local UI bridge.
- Four clients can join the same match.
- Each player sees a distinct tank.
- Movement, aim, shooting, collision, HP, death, respawn, pickups, team score, and winner state work.
- V broadcasts full state snapshots roughly every 50 ms.
- Browser UI renders remote players and local player from the V state.
- Disconnecting a client removes its tank from other clients' state.
- Tests cover protocol roundtrip and core game-world behavior.

## Later Security Integration

Later phases wrap the same `GameMessage` payloads. The game world does not change.

```text
Phase A:
  Packet.payload = GameMessage

Phase B:
  Packet.payload = app envelope containing GameMessage

Phase C:
  Packet.payload = DES(Kc_v, GameMessage)

Phase D:
  Packet.payload = DES(Kc_v, seq_no + SignedGameMessage)
```

The secure channel will validate identity, decrypt, verify sequence/signature when needed, then call the same application handlers:

```cpp
handle_move(client_id, x, y);
handle_target(client_id, angle);
handle_shoot(client_id, shooting);
handle_name(client_id, name);
```

This preserves the intended layering: security is a transparent envelope around the application protocol, and `BattleRoom` remains independent of Kerberos and non-repudiation.
