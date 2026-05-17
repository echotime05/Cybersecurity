# Cyber Tank Battle

This project is a Windows C++17 Kerberos-style tank battle demo with a browser
UI. The supported deployment mode is:

```text
Browser UI <-> local C++ client WebSocket bridge <-> AS/TGS/V over TCP
Protocol panel <-> local monitor.exe WebSocket bridge <-> local packet logs
```

The browser never connects to V directly. It connects to the local `client.exe`
bridge on `ws://127.0.0.1:<ui-port>`. The C++ client performs AS, TGS, and V
authentication, then forwards game input to V. V owns the authoritative game
state and broadcasts snapshots back to clients.

The optional protocol panel connects only to local `monitor.exe` on
`ws://127.0.0.1:<monitor-port>`. `monitor.exe` tails local text logs under
`logs/protocol_events/`, so each physical machine displays only packets sent
and received by processes running on that machine.

## Supported Runtime

The public runtime uses Kerberos authentication, encrypted game payloads, and
application-level non-repudiation:

```text
as_server.exe --serve
tgs_server.exe --serve
v_server.exe --game-auth-encrypted
client.exe --game-auth-encrypted --ui-port 7001
monitor.exe --ui-port 7010
```

Additional non-deployment modes are kept only for internal tests. Do not use
them for normal deployment.

## Security Model

`--game-auth-encrypted` runs the complete application chain:

1. `client.exe` obtains `Kc_tgs` from AS.
2. `client.exe` obtains `Kc_v` and `Ticket_v` from TGS.
3. `client.exe` authenticates to V and completes the client/V certificate
   exchange.
4. Client-to-V and V-to-client game `MSG_APP` payloads are signed and encrypted
   with `Kc_v`.
5. Every non-ACK game packet is acknowledged by the receiver with a signed and
   encrypted `APP_ACK`. ACK packets are evidence only and are not acknowledged
   again.

The browser UI connection is local development traffic between the browser and
`client.exe`; it is not encrypted. Security is applied on the C++ client-to-V
game channel.

## Prerequisites

- Windows
- CMake 3.16 or newer
- Ninja and a C++17 compiler available on `PATH`
- Node.js and npm available on `PATH`

## Build

From the repository root:

```powershell
cmake -S . -B build-mingw -G Ninja
cmake --build build-mingw
```

Run the C++ and integration tests:

```powershell
ctest --test-dir build-mingw --output-on-failure
```

Run the Web UI protocol parser selftest and production build:

```powershell
cd web-ui
npm run verify
cd ..
```

## Single-Machine Demo

The default `config/course_config.txt` is a single-machine config. It binds AS,
TGS, and V to localhost and uses `Client1` as the local client.

Start the C++ backend:

```powershell
.\scripts\run_local.ps1
```

or:

```cmd
run_local.bat
```

Start the Web UI in another terminal:

```powershell
.\scripts\run_web.ps1
```

or:

```cmd
run_web.bat
```

Open:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

Stop the local C++ backend:

```powershell
.\scripts\stop_local.ps1
```

or:

```cmd
stop_local.bat
```

## Manual Startup

Use these commands if you want to start each role yourself. Run each command
from the repository root.

Terminal 1:

```powershell
.\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

Terminal 2:

```powershell
.\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

Terminal 3:

```powershell
.\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

Terminal 4:

```powershell
.\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
```

Terminal 5:

```powershell
.\build-mingw\monitor.exe --ui-port 7010 --events-dir .\logs\protocol_events
```

Terminal 6:

```powershell
.\scripts\run_web.ps1
```

## Login Passwords

```text
Client1: 123456
Client2: admin123
Client3: hehe12345
Client4: &wxh@147
```

## Four-Host Deployment

Each host needs the same AS, TGS, and V addresses in its config file. The
templates under `config/lan/` are examples. Edit the IP addresses for your LAN,
then copy the matching file to `config/course_config.txt` on each host.

Recommended role layout:

```text
Host 1: Client1
Host 2: AS + Client2
Host 3: TGS + Client3
Host 4: V + Client4
```

On Host 2:

```powershell
Copy-Item .\config\lan\host2_as_client2.txt .\config\course_config.txt -Force
.\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

On Host 3:

```powershell
Copy-Item .\config\lan\host3_tgs_client3.txt .\config\course_config.txt -Force
.\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

On Host 4:

```powershell
Copy-Item .\config\lan\host4_v_client4.txt .\config\course_config.txt -Force
.\build-mingw\v_server.exe --config .\config\course_config.txt --game-auth-encrypted
```

On each player host, including Host 1 if it is client-only:

```powershell
.\build-mingw\client.exe --config .\config\course_config.txt --game-auth-encrypted --ui-port 7001
.\build-mingw\monitor.exe --ui-port 7010 --events-dir .\logs\protocol_events
.\scripts\run_web.ps1
```

Open the same local URL on each player host:

```text
http://127.0.0.1:5173/?client=ws://127.0.0.1:7001&monitor=ws://127.0.0.1:7010
```

Start `monitor.exe` on AS/TGS/V-only hosts too if you want a browser on that
host to inspect that host's local packet send/receive events.

## Configuration Rules

- AS, TGS, V, and each client must agree on `AS_IP`, `TGS_IP`, `V_IP`, and the
  corresponding ports.
- `*_IP` values are the addresses clients connect to.
- `*_BIND_IP` values are the addresses servers listen on. Use `127.0.0.1` for a
  local demo and `0.0.0.0` for LAN servers.
- Every local browser still connects to its own local client bridge at
  `ws://127.0.0.1:7001`.

## Logs

Runtime logs are written under `logs/`:

```text
logs/as.log
logs/tgs.log
logs/v_game.log
logs/client_game.log
logs/v_ack.log
logs/client_ack.log
logs/protocol_events/*.txt
logs/runtime/*.out
logs/runtime/*.err
```

`logs/v_ack.log` and `logs/client_ack.log` are the non-repudiation evidence
logs. Each verified ACK is recorded as `APP_NON_REPUDIATION_ACK packet_hex=...`,
where `packet_hex` is the complete ACK packet including header and encrypted
signed payload.

`logs/protocol_events/*.txt` are local protocol monitor logs. Each line keeps
the existing text-log style and includes structured packet fields such as
timestamp, direction, endpoint, message name, fixed header fields, and payload
hex. `monitor.exe` tails these files and sends `protocolEvent` messages to the
browser Protocol panel.

## Project Layout

```text
src/roles/       Role entry points for AS, TGS, V, and Client
src/common/      Shared protocol, auth, crypto, network, config, and logging code
src/game/        Tank battle protocol and authoritative game state
src/ui/          Local WebSocket bridge used by client.exe
include/cyber/   Public C++ headers
config/          Default and LAN config files
tests/           C++ and PowerShell selftests
web-ui/          Browser UI
scripts/         Portable startup helpers
```

## Troubleshooting

If the browser says `ERR_CONNECTION_REFUSED` for `127.0.0.1:5173`, the Web UI
dev server is not running. Start it with `.\scripts\run_web.ps1`.

If login stays on `Authenticating`, check that AS, TGS, V, and `client.exe` are
all running and using the same config file. Also check that V was started with
`--game-auth-encrypted`.

If `client.exe` cannot listen on port 7001, stop the old client process:

```powershell
.\scripts\stop_local.ps1
```
