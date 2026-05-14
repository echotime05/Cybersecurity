# Encrypted Game Payloads Design

## Purpose

Add an authenticated encrypted tank-game mode after the existing Kerberos-gated UI work.

This phase encrypts only the application-layer game payloads exchanged between the C++ client and V server. It does not encrypt the browser-to-local-client WebSocket bridge, does not change the packet header, and does not add signatures, non-repudiation, replay detection, or a security diagnostics UI.

## Scope

In scope:

- A new independent command mode for encrypted authenticated gameplay.
- DES payload encryption for `MsgType::app` game messages between C++ client and V.
- Reuse of the Kerberos `Kc_v` session key produced during V authentication.
- No changes to the Three.js/Web UI transport or rendering flow.
- Tests proving encrypted gameplay works and plaintext app payloads are not accepted in encrypted mode.

Out of scope:

- Encrypting the local browser WebSocket bridge.
- Changing the fixed packet header.
- Encrypting AS/TGS/V Kerberos protocol messages beyond the existing authentication encryption.
- RSA signatures, signed ACKs, non-repudiation, or replay protection.
- Dedicated Web UI states for decryption failures.
- Packet inspection screens or security log visualization in the tank UI.

## Command Surface

Keep the current plaintext authenticated game mode:

```powershell
.\build-mingw\v_server.exe --game-auth-plain
.\build-mingw\client.exe --game-auth-plain --ui-port 7001
```

Add a separate encrypted mode:

```powershell
.\build-mingw\v_server.exe --game-auth-encrypted
.\build-mingw\client.exe --game-auth-encrypted --ui-port 7001
```

The encrypted mode uses the same login and join UI as `--game-auth-plain`. The difference is only the network representation of game `MsgType::app` payloads on the C++ client-to-V socket.

## Data Flow

The browser remains connected only to the local C++ client:

```text
Web UI
  -- plaintext local JSON/WebSocket -->
C++ Client
  -- MsgType::app + E_Kc_v(GameMessage) -->
V Server
  -- decrypt, then existing BattleRoom logic -->
V Server
  -- MsgType::app + E_Kc_v(GameState) -->
C++ Client
  -- decrypt, then plaintext local JSON/WebSocket -->
Web UI
```

The fixed packet header is unchanged:

```text
msg_type = MsgType::app
src      = client or V
dst      = V or client
payload  = encrypted or plaintext GameMessage, depending on mode
```

The logical application payload remains the existing `GameMessage` layout:

```text
GameMessage {
  uint8 type
  bytes payload
}
```

In encrypted mode, the network payload is:

```text
DES_Kc_v(build_game_message(message))
```

In plaintext mode, it remains:

```text
build_game_message(message)
```

## Architecture

Use one implementation path with a small app-payload codec switch.

`AuthPlainGameClient` should be extended with a configuration flag:

```text
encrypt_app_payloads: bool
```

`PlainGameServer` should be extended with a matching flag:

```text
encrypt_app_payloads: bool
```

The class names can remain unchanged for this phase. The command mode name provides the externally visible distinction, while the shared implementation keeps the plaintext and encrypted paths close enough for regression testing.

Add small helper functions in the game or common layer:

```text
encode_app_payload(plain, kc_v, encrypted) -> wire_payload
decode_app_payload(wire_payload, kc_v, encrypted) -> plain
```

The helpers should:

- Return the input unchanged when `encrypted` is false.
- Use `des_encrypt_payload()` and `des_decrypt_payload()` when `encrypted` is true.
- Avoid knowing about sockets, UI state, or battle-room behavior.

## Client Behavior

The authenticated client already receives a `VAuthenticatedSocket` from:

```text
authenticate_client_to_v_socket()
```

That object contains:

```text
state.kc_v
socket
```

In encrypted mode, the client stores `state.kc_v` after successful login.

When sending a game command:

```text
plain = build_game_message({type, payload})
wire  = encode_app_payload(plain, kc_v, encrypt_app_payloads)
send MsgType::app with wire payload
```

When receiving state from V:

```text
wire  = packet.payload
plain = decode_app_payload(wire, kc_v, encrypt_app_payloads)
message = parse_game_message(plain)
```

The Web UI still receives the same state JSON from the local client. No browser-side decryption is added.

## Server Behavior

`PlainGameServer` already authenticates a socket before accepting game traffic in `require_auth` mode.

In encrypted mode, `authenticate_socket()` must expose both:

```text
client_id
kc_v
```

The server can obtain `kc_v` from the existing V auth session table after `process_v_auth_request()` stores the session.

The connection table should keep:

```text
ClientConnection {
  socket
  client_id
  kc_v
}
```

When receiving client game traffic:

```text
verify packet.msg_type == MsgType::app
verify packet.src == authenticated client_id
plain = decode_app_payload(packet.payload, kc_v, encrypt_app_payloads)
message = parse_game_message(plain)
handle existing join/move/target/shoot/name logic
```

When broadcasting state:

```text
plain = build_game_message({state, build_state(snapshot)})
for each connected client:
  wire = encode_app_payload(plain, connection.kc_v, encrypt_app_payloads)
  send MsgType::app to that client
```

Each client must be encrypted with its own `Kc_v`, not a shared game key.

## Error Handling

Decryption failures do not get a dedicated Web UI state in this phase.

Server behavior:

- If a client app payload cannot be decrypted or parsed in encrypted mode, V writes a log entry and closes that client connection or ignores the invalid packet.
- V must not pass malformed decrypted data into `BattleRoom`.

Client behavior:

- If a V app payload cannot be decrypted or parsed in encrypted mode, the client writes a log entry and stops processing that V connection.
- The Web UI may continue to show the existing disconnected or stale-state behavior.

UI behavior:

- Do not show low-level messages such as "decrypt failed", "bad cipher", or key information.
- Keep the tank UI focused on login, joining, and rendering.

## Compatibility

`--game-auth-plain` remains the control path. It should continue to send and receive plaintext `MsgType::app` game payloads.

`--game-auth-encrypted` is expected to be incompatible with plaintext app payloads after V authentication. A client or test that sends plaintext `GameMessage` bytes on an encrypted game connection should not successfully join or receive usable state.

The packet header remains readable in logs and by tooling. Payload summaries should avoid exposing decrypted application content when the packet was sent in encrypted mode.

## Testing

Add or extend tests for:

- `encode_app_payload()` / `decode_app_payload()` plaintext roundtrip.
- `encode_app_payload()` / `decode_app_payload()` encrypted roundtrip using `Kc_v`.
- Encrypted output differs from the original `GameMessage` bytes.
- Existing `--game-auth-plain` flow still passes.
- New `--game-auth-encrypted` flow completes login, join, and receives at least one state update.
- V encrypted mode rejects or fails to process a plaintext `MsgType::app` join after V authentication.
- Broadcast encryption uses each client's own `Kc_v`, not a shared server key.

Manual verification should cover:

- Start AS, TGS, V with `--game-auth-encrypted`.
- Start client with `--game-auth-encrypted --ui-port 7001`.
- Login with a real client password.
- Join the game.
- Confirm the browser gameplay behaves the same as the authenticated plaintext mode.
- Confirm logs show app payloads are sent in encrypted mode without printing keys or plaintext game payloads.
