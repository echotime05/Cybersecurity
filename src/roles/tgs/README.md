# TGS Role

Startup:

```powershell
.\_generated\build-mingw\tgs_server.exe --config .\config\course_config.txt --serve
```

Owned files:

```text
src/roles/tgs/main.cpp
src/roles/tgs/tgs_service.cpp
include/cyber/roles/tgs/tgs_service.hpp
```

Shared files usually modified for TGS behavior:

```text
src/shared/protocol/kerberos_messages.cpp
src/shared/crypto/crypto.cpp
include/cyber/protocol/kerberos_messages.hpp
```

Responsibilities:

```text
Client -> TGS: MSG_TGS_REQ
TGS -> Client: MSG_TGS_REP or MSG_ERROR
```

TGS decrypts `Ticket_tgs`, validates `Authenticator_tgs`, checks the requested service V, creates `Kc_v`, builds `Ticket_v`, encrypts the TGS reply, and writes protocol events for the UI.

Common live modification points:

```text
TGS packet handling:      src/roles/tgs/tgs_service.cpp
Ticket_v field layout:   src/shared/protocol/kerberos_messages.cpp
Replay/time validation:  src/roles/tgs/tgs_service.cpp
Role CLI/startup:        src/shared/runtime/role_runtime.cpp
```
