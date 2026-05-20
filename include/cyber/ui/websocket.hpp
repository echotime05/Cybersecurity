#pragma once

#include "cyber/shared/net_socket.hpp"
#include "cyber/protocol/packet.hpp"

#include <cstdint>
#include <string>

namespace cyber::ui
{
// 解析后的 WebSocket 帧，当前 UI 只使用文本帧。
struct WebSocketFrame
{
    std::uint8_t opcode = 0;
    std::string text;
};

// 根据浏览器发来的 Sec-WebSocket-Key 生成握手响应 key。
std::string websocket_accept_key(const std::string& client_key);
// 将文本 JSON 封装成服务器到浏览器的 WebSocket 文本帧。
Bytes build_ws_text_frame(const std::string& text);
// 解析浏览器发来的 WebSocket 帧并取出文本内容。
WebSocketFrame parse_ws_frame(const Bytes& frame);
// 在已有 TCP socket 上完成 WebSocket 服务端握手。
void perform_websocket_server_handshake(SocketHandle socket);
// 接收一条 WebSocket 文本消息。
std::string recv_ws_text(SocketHandle socket);
// 发送一条 WebSocket 文本消息。
void send_ws_text(SocketHandle socket, const std::string& text);
} // namespace cyber::ui
