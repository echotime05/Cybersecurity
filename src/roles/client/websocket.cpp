#include "cyber/ui/websocket.hpp"

#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <wincrypt.h>

namespace cyber::ui
{
namespace
{
using NativeSocket = SOCKET;

// 调用 Windows CryptoAPI 计算 SHA1，WebSocket 握手 accept key 需要它。
std::array<std::uint8_t, 20> sha1(const std::string& input)
{
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::array<std::uint8_t, 20> digest{};
    DWORD digest_len = static_cast<DWORD>(digest.size());

    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        throw std::runtime_error("CryptAcquireContextA failed");
    }
    if (!CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash))
    {
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptCreateHash failed");
    }
    if (!CryptHashData(hash, reinterpret_cast<const BYTE*>(input.data()),
                       static_cast<DWORD>(input.size()), 0))
    {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptHashData failed");
    }
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &digest_len, 0))
    {
        CryptDestroyHash(hash);
        CryptReleaseContext(provider, 0);
        throw std::runtime_error("CryptGetHashParam failed");
    }

    CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return digest;
}

// 将字节串编码为 Base64，WebSocket 握手响应使用。
std::string base64(const std::uint8_t* data, std::size_t size)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2U) / 3U) * 4U);
    for (std::size_t i = 0; i < size; i += 3U)
    {
        const std::uint32_t b0 = data[i];
        const std::uint32_t b1 = (i + 1U < size) ? data[i + 1U] : 0U;
        const std::uint32_t b2 = (i + 2U < size) ? data[i + 2U] : 0U;
        const std::uint32_t triple = (b0 << 16U) | (b1 << 8U) | b2;
        out.push_back(alphabet[(triple >> 18U) & 0x3FU]);
        out.push_back(alphabet[(triple >> 12U) & 0x3FU]);
        out.push_back((i + 1U < size) ? alphabet[(triple >> 6U) & 0x3FU] : '=');
        out.push_back((i + 2U < size) ? alphabet[triple & 0x3FU] : '=');
    }
    return out;
}

// 循环发送直到指定缓冲区全部写入 socket。
void send_all(SocketHandle socket, const char* data, int len)
{
    int sent = 0;
    while (sent < len)
    {
        const int n = send(static_cast<NativeSocket>(socket), data + sent, len - sent, 0);
        if (n <= 0)
        {
            throw std::runtime_error("websocket send failed");
        }
        sent += n;
    }
}

// 从 WebSocket TCP 连接读取一段原始字节。
Bytes recv_some(SocketHandle socket)
{
    std::array<char, 4096> buffer{};
    const int n = recv(static_cast<NativeSocket>(socket), buffer.data(),
                       static_cast<int>(buffer.size()), 0);
    if (n <= 0)
    {
        throw std::runtime_error("websocket recv failed");
    }
    return Bytes(buffer.begin(), buffer.begin() + n);
}
} // namespace

// 根据浏览器握手 key 生成 Sec-WebSocket-Accept。
std::string websocket_accept_key(const std::string& client_key)
{
    const std::string magic = client_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto digest = sha1(magic);
    return base64(digest.data(), digest.size());
}

// 构造服务端发给浏览器的未掩码文本帧。
Bytes build_ws_text_frame(const std::string& text)
{
    Bytes out;
    out.push_back(0x81);
    if (text.size() <= 125U)
    {
        out.push_back(static_cast<std::uint8_t>(text.size()));
    }
    else if (text.size() <= 65535U)
    {
        out.push_back(126);
        out.push_back(static_cast<std::uint8_t>((text.size() >> 8U) & 0xFFU));
        out.push_back(static_cast<std::uint8_t>(text.size() & 0xFFU));
    }
    else
    {
        throw std::runtime_error("websocket text frame too large");
    }
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

// 解析浏览器发来的掩码文本帧并还原 payload。
WebSocketFrame parse_ws_frame(const Bytes& frame)
{
    if (frame.size() < 2U)
    {
        throw std::runtime_error("websocket frame too short");
    }
    WebSocketFrame parsed;
    parsed.opcode = frame[0] & 0x0FU;
    const bool masked = (frame[1] & 0x80U) != 0;
    std::uint64_t len = frame[1] & 0x7FU;
    std::size_t offset = 2;
    if (len == 126U)
    {
        if (frame.size() < offset + 2U)
        {
            throw std::runtime_error("websocket extended length missing");
        }
        len = (static_cast<std::uint64_t>(frame[offset]) << 8U) | frame[offset + 1U];
        offset += 2U;
    }
    else if (len == 127U)
    {
        throw std::runtime_error("websocket 64-bit lengths are not supported");
    }
    if (!masked)
    {
        throw std::runtime_error("client websocket frame must be masked");
    }
    if (frame.size() < offset + 4U + len)
    {
        throw std::runtime_error("websocket frame payload truncated");
    }
    const std::uint8_t mask[4] = {frame[offset], frame[offset + 1U], frame[offset + 2U],
                                  frame[offset + 3U]};
    offset += 4U;
    parsed.text.reserve(static_cast<std::size_t>(len));
    for (std::size_t i = 0; i < len; ++i)
    {
        parsed.text.push_back(static_cast<char>(frame[offset + i] ^ mask[i % 4U]));
    }
    return parsed;
}

// 完成 HTTP Upgrade 到 WebSocket 的服务端握手。
void perform_websocket_server_handshake(SocketHandle socket)
{
    std::string request;
    std::array<char, 1024> buffer{};
    while (request.find("\r\n\r\n") == std::string::npos)
    {
        const int n = recv(static_cast<NativeSocket>(socket), buffer.data(),
                           static_cast<int>(buffer.size()), 0);
        if (n <= 0)
        {
            throw std::runtime_error("websocket handshake recv failed");
        }
        request.append(buffer.data(), static_cast<std::size_t>(n));
        if (request.size() > 8192U)
        {
            throw std::runtime_error("websocket handshake too large");
        }
    }

    const std::string marker = "Sec-WebSocket-Key:";
    const std::size_t marker_pos = request.find(marker);
    if (marker_pos == std::string::npos)
    {
        throw std::runtime_error("websocket key missing");
    }
    std::size_t key_start = marker_pos + marker.size();
    while (key_start < request.size() && (request[key_start] == ' ' || request[key_start] == '\t'))
    {
        ++key_start;
    }
    const std::size_t key_end = request.find("\r\n", key_start);
    if (key_end == std::string::npos || key_end == key_start)
    {
        throw std::runtime_error("websocket key malformed");
    }
    const std::string client_key = request.substr(key_start, key_end - key_start);

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << websocket_accept_key(client_key) << "\r\n\r\n";
    const std::string text = response.str();
    send_all(socket, text.data(), static_cast<int>(text.size()));
}

// 接收一条 WebSocket 文本消息，遇到关闭帧或非文本帧会抛异常。
std::string recv_ws_text(SocketHandle socket)
{
    const WebSocketFrame frame = parse_ws_frame(recv_some(socket));
    if (frame.opcode == 0x8U)
    {
        throw std::runtime_error("websocket close frame received");
    }
    if (frame.opcode != 0x1U)
    {
        throw std::runtime_error("only websocket text frames are supported");
    }
    return frame.text;
}

// 发送一条 WebSocket 文本消息。
void send_ws_text(SocketHandle socket, const std::string& text)
{
    const Bytes frame = build_ws_text_frame(text);
    send_all(socket, reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
}
} // namespace cyber::ui
