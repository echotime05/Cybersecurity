# Role Source Layout

Each runtime role has its own entry directory:

```text
src/roles/client/main.cpp
src/roles/as/main.cpp
src/roles/tgs/main.cpp
src/roles/v/main.cpp
```

Role directories should stay thin. They own only role startup and should call shared code from `src/common` through headers in `include/cyber/common`.

Current executables remain unchanged:

```text
client.exe
as_server.exe
tgs_server.exe
v_server.exe
```

Ownership guide:

- Client owner: `src/roles/client` plus Client-facing flow changes in `src/common/auth_flow.cpp`.
- AS owner: `src/roles/as` plus AS handler code in `src/common/auth_flow.cpp`.
- TGS owner: `src/roles/tgs` plus TGS handler code in `src/common/auth_flow.cpp`.
- V owner: `src/roles/v` plus V handler/session code in `src/common/auth_flow.cpp`.
- Shared protocol owner: `include/cyber/common`, `src/common`, and `tests`.
