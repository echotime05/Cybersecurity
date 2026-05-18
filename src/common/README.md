# Common Source Layout

Shared code used by Client, AS, TGS, and V lives here.

```text
auth_flow.cpp          Kerberos normal flow, certificate exchange, APP_ACK non-repudiation
config.cpp             course_config.txt parser
crypto.cpp             DES-style payload encryption, hash, RSA-style signatures, certificates
log_parser.cpp         structured log parser
logger.cpp             async 20ms batched thread-safe line logger
net_packet.cpp         packet send/recv with required PACKET_SEND/PACKET_RECV logs
net_socket.cpp         TCP socket helpers with TCP_NODELAY for low-latency small packets
packet.cpp             packet header serialization and generic MSG_ERROR / MSG_APP helpers
protocol_payloads.cpp  binary payload builders/parsers for Kerberos and APP_ACK
role_runtime.cpp       final role CLI plus AS/TGS server dispatch
```

Keep role-specific `main.cpp` files under `src/roles`. Put reusable protocol, logging, socket, crypto, and testable flow code in this directory.
