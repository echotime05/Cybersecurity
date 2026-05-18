# AS Role

Startup:

```powershell
.\_generated\build-mingw\as_server.exe --config .\config\course_config.txt --serve
```

Owned files:

```text
src/roles/as/main.cpp
src/roles/as/as_service.cpp
include/cyber/roles/as/as_service.hpp
```

Shared files usually modified for AS behavior:

```text
src/shared/auth/auth_credentials.cpp
src/shared/protocol/kerberos_messages.cpp
src/shared/crypto/crypto.cpp
include/cyber/common/auth_credentials.hpp
include/cyber/protocol/kerberos_messages.hpp
```

Responsibilities:

```text
Client -> AS: MSG_AS_REQ
AS -> Client: MSG_AS_REP or MSG_ERROR
```

AS validates the Client ID and password-derived long-term key, creates `Kc_tgs`, builds `Ticket_tgs`, encrypts the AS reply for the client, and writes protocol events for the UI.

Common live modification points:

```text
Client credential table: src/shared/auth/auth_credentials.cpp
AS packet handling:     src/roles/as/as_service.cpp
AS/TGS payload fields:  src/shared/protocol/kerberos_messages.cpp
Role CLI/startup:       src/shared/runtime/role_runtime.cpp
```
