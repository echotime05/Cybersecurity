# TGS Role

Startup:

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

Owned files:

```text
src/roles/tgs/main.cpp
```

Shared files usually modified for TGS behavior:

```text
src/shared/runtime/role_runtime.cpp
src/shared/auth/auth_flow.cpp
src/shared/protocol/protocol_payloads.cpp
src/shared/crypto/crypto.cpp
include/cyber/common/auth_flow.hpp
include/cyber/common/protocol_payloads.hpp
```

Responsibilities:

```text
Client -> TGS: MSG_TGS_REQ
TGS -> Client: MSG_TGS_REP or MSG_ERROR
```

TGS decrypts `Ticket_tgs`, validates `Authenticator_tgs`, checks the requested service V, creates `Kc_v`, builds `Ticket_v`, encrypts the TGS reply, and writes normal/protocol logs.

Common live modification points:

```text
TGS packet handling:      src/shared/auth/auth_flow.cpp
Ticket_v field layout:   src/shared/protocol/protocol_payloads.cpp
Replay/time validation:  src/shared/auth/auth_flow.cpp
Role CLI/startup:        src/shared/runtime/role_runtime.cpp
```
