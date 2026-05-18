# Shared Source Layout

Shared code used by AS, TGS, V, Client, and Monitor lives here.

```text
auth/auth_credentials.cpp       Client passwords, long-term keys, entity keys
config/config.cpp               course_config.txt parser
crypto/crypto.cpp               DES-style payload encryption, hash, RSA-style signatures, certificates
game/app_payload_codec.cpp      MSG_APP payload codec switch and encrypted/plain views
game/game_protocol.cpp          GameMessage and snapshot payload serialization
game/game_non_repudiation.cpp   Signed game payload and APP_ACK helpers
logging/log_parser.cpp          Low-level bracket log parser used by logger self-tests
logging/logger.cpp              Async line writer used by protocol_events
net/net_packet.cpp              Packet send/recv plus protocol event hooks
net/net_socket.cpp              TCP socket helpers with TCP_NODELAY
protocol/packet.cpp             Fixed 11-byte packet header and generic MSG_ERROR/MSG_APP helpers
protocol/protocol_event.cpp     Protocol monitor event writer/parser
protocol/kerberos_messages.cpp  Kerberos payload builders/parsers
protocol/certificate_messages.cpp  Client/V certificate payload builders/parsers
protocol/app_envelope.cpp       APP_ACK and SignedAppPayload builders/parsers
runtime/role_runtime.cpp        Final role CLI dispatch and AS/TGS server loop
```

When a live modification changes one role only, start in `src/roles/<role>`. When it changes wire format, encryption, signatures, ACK, logging, sockets, or shared CLI behavior, update this directory and the matching headers under `include/cyber`.
