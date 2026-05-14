#pragma once

#include "cyber/common/net_socket.hpp"
#include "cyber/common/packet.hpp"

#include <cstdint>
#include <string>

namespace cyber::ui
{
struct WebSocketFrame
{
    std::uint8_t opcode = 0;
    std::string text;
};

std::string websocket_accept_key(const std::string& client_key);
Bytes build_ws_text_frame(const std::string& text);
WebSocketFrame parse_ws_frame(const Bytes& frame);
void perform_websocket_server_handshake(SocketHandle socket);
std::string recv_ws_text(SocketHandle socket);
void send_ws_text(SocketHandle socket, const std::string& text);
} // namespace cyber::ui
