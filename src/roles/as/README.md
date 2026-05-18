# AS Role

Startup:

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

Owned files:

```text
src/roles/as/main.cpp
```

Shared files usually modified for AS behavior:

```text
src/shared/runtime/role_runtime.cpp
src/shared/auth/auth_flow.cpp
src/shared/auth/auth_credentials.cpp
src/shared/protocol/protocol_payloads.cpp
src/shared/crypto/crypto.cpp
include/cyber/common/auth_flow.hpp
include/cyber/common/auth_credentials.hpp
include/cyber/common/protocol_payloads.hpp
```

Responsibilities:

```text
Client -> AS: MSG_AS_REQ
AS -> Client: MSG_AS_REP or MSG_ERROR
```

AS validates the Client ID and password-derived long-term key, creates `Kc_tgs`, builds `Ticket_tgs`, encrypts the AS reply for the client, and writes normal/protocol logs.

Common live modification points:

```text
Client credential table: src/shared/auth/auth_credentials.cpp
AS packet handling:     src/shared/auth/auth_flow.cpp
AS/TGS payload fields:  src/shared/protocol/protocol_payloads.cpp
Role CLI/startup:       src/shared/runtime/role_runtime.cpp
```
