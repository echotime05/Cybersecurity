# Kerberos-Gated Tank UI Design

## Purpose

Add a real login gate before the existing tank battle Web UI. The browser asks the user to choose a client identity and enter that client's password. The local C++ client derives the client's long-term key from the password, runs the existing Kerberos flow, and only allows the player to join the tank game after V authentication succeeds.

This phase does not add application-layer game-message encryption. Kerberos tickets, authenticators, and replies still use the existing authentication encryption because that is part of the Kerberos flow, but tank game messages remain plaintext `MsgType::app` payloads after login.

## Scope

In scope:

- A browser login screen with `Client1` through `Client4`, password input, and a login button.
- Password-based client key derivation in the C++ client.
- A Kerberos-authenticated client state before game join.
- A simple join screen after successful authentication.
- A join button that starts the existing plaintext tank game session.
- Fixed display names derived from client identity, such as `Client1`.

Out of scope:

- Showing AS/TGS/V step-by-step logs in the game UI.
- Showing tickets, session keys, encrypted payloads, or packet details in the game UI.
- User-entered nicknames.
- Application-layer game-message encryption.
- Non-repudiation and signed ACKs for the gameplay stream.
- Replay/tamper demo controls.
- Replacing the current Web UI framework.

## UI Flow

The browser starts at the login screen:

```text
Login
  Client ID: Client1 / Client2 / Client3 / Client4
  Password: ********
  [Login]
```

After successful Kerberos authentication, the browser shows the join screen:

```text
Join Game
  Client ID: Client4
  V Server: 172.27.39.248:9003
  [Join Game]
```

After joining, the existing Three.js game screen becomes active:

```text
Game
  battlefield
  health and shield bars
  leaderboard
  existing keyboard and mouse controls
```

The game screen does not receive movement, aiming, or shooting input before join succeeds.

## Process Architecture

The process layout remains:

```text
Browser UI <-> Local C++ Client <-> C++ AS/TGS/V
```

The browser only connects to the local C++ client WebSocket bridge. The browser never talks directly to AS, TGS, or V.

Responsibilities:

- Browser UI collects login input, shows coarse login failure/success, shows the join screen, and renders the game.
- Local C++ client owns password handling, key derivation, Kerberos authentication, the V connection, and game join.
- AS uses its configured long-term client key to encrypt `AS_REP`.
- TGS and V continue to use the existing Kerberos payloads and handlers.
- V remains authoritative for tank world state after join.

## Password And Key Handling

The password is sent only from the browser to the local C++ client over `ws://127.0.0.1:<ui-port>`.

The password is not sent to AS, TGS, or V.

The C++ client derives its long-term key with a deterministic function:

```text
material = "client-kc-v1:" + decimal(client_id) + ":" + password
Kc = hash64(ascii(material)) & 0x00FFFFFFFFFFFFFF
if Kc == 0, use 0x01010101010101
```

`hash64` is the existing project hash helper. This is a deterministic course-demo derivation, not a production password KDF. It keeps this phase compatible with the current 56-bit DES-style Kerberos helpers. The configured `C1_KC` through `C4_KC` values must be updated to match the same derivation for the configured course passwords.

Login succeeds only if the derived client key can decrypt and parse the `AS_REP`, and the client then completes `TGS_REQ / TGS_REP` and `V_AUTH_REQ / V_AUTH_REP`.

## WebSocket Bridge Commands

The existing UI bridge is extended with coarse authentication and join commands:

```json
{"type":"login","clientId":4,"password":"..."}
{"type":"join"}
```

The bridge sends coarse status messages back to the browser:

```json
{"type":"loginState","status":"idle"}
{"type":"loginState","status":"authenticating"}
{"type":"loginState","status":"authenticated","clientId":4,"vServer":"172.27.39.248:9003"}
{"type":"loginState","status":"failed","message":"Invalid client id or password"}
{"type":"joinState","status":"joined"}
```

The browser continues to receive the existing game `state` messages only after join succeeds.

## Client Runtime Behavior

The authenticated plaintext game mode defers the V game join until the user clicks `Join Game`.

Add this command surface:

```powershell
.\build-mingw\client.exe --game-auth-plain --ui-port 7001
```

`--game-plain` remains available for the existing no-login development path.

Client state machine:

```text
Disconnected UI
  -> UI connected
  -> Authenticating
  -> Authenticated
  -> Joined
  -> In game
```

If authentication fails, the client clears any partial auth state and lets the browser retry login.

## Error Handling

The game UI only reports coarse errors:

- invalid client id or password
- AS/TGS/V unreachable
- authentication failed
- join failed
- local bridge disconnected

Detailed AS/TGS/V packet progress and diagnostics belong to the logging UI, not this game UI.

## Testing

Add focused tests for:

- deterministic password-to-key derivation
- wrong password failing to parse `AS_REP`
- successful Kerberos auth with derived key
- UI bridge parsing `login` and `join` commands
- UI bridge login state JSON generation
- authenticated client refusing `move`, `target`, and `shoot` before join
- successful authenticated join followed by plaintext game state delivery

Manual verification should cover:

- browser shows login first
- wrong password stays on login screen with a coarse error
- correct password reaches the join screen
- join enters the existing tank game
- game controls remain disabled before join and enabled after join

## Compatibility

The existing packet header remains unchanged. The later application-layer encryption and non-repudiation work should wrap the same game messages instead of changing this UI flow.

The logging UI can independently observe AS/TGS/V steps through existing logs without requiring the tank Web UI to display them.
