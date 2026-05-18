# Monitor Role

Startup:

```powershell
.\_generated\build-mingw\monitor.exe --ui-port 7010 --events-dir .\_generated\logs\protocol_events
```

Owned files:

```text
src/roles/monitor/main.cpp
src/roles/monitor/protocol_monitor.cpp
```

Shared/UI files usually modified for monitor behavior:

```text
src/shared/protocol/protocol_event.cpp
include/cyber/protocol/protocol_event.hpp
include/cyber/monitor/protocol_monitor.hpp
web-ui/src/ProtocolMonitor.ts
web-ui/src/protocolPayload.ts
```

Responsibilities:

```text
Role processes -> _generated/logs/protocol_events/*.txt
monitor.exe -> Browser Protocol panel over WebSocket
```

Monitor does not participate in authentication, encryption, ACK, or game calculation. It tails local protocol event logs, parses packet metadata and payload views, deduplicates events, and streams structured protocol events to the browser.

Common live modification points:

```text
Event file parsing:       src/shared/protocol/protocol_event.cpp
Monitor WebSocket server: src/roles/monitor/protocol_monitor.cpp
Protocol panel layout:    web-ui/src/ProtocolMonitor.ts
Payload visualization:    web-ui/src/protocolPayload.ts
```
