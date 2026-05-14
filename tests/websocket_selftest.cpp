#include "cyber/ui/websocket.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        const std::string accept =
            cyber::ui::websocket_accept_key("dGhlIHNhbXBsZSBub25jZQ==");
        require(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC accept key mismatch");

        const cyber::Bytes server_frame = cyber::ui::build_ws_text_frame("hello");
        require(server_frame.size() == 7U, "server frame size mismatch");
        require(server_frame[0] == 0x81 && server_frame[1] == 0x05,
                "server frame header mismatch");

        const cyber::Bytes masked_client_frame{
            0x81,
            0x85,
            0x37,
            0xFA,
            0x21,
            0x3D,
            static_cast<std::uint8_t>('h' ^ 0x37),
            static_cast<std::uint8_t>('e' ^ 0xFA),
            static_cast<std::uint8_t>('l' ^ 0x21),
            static_cast<std::uint8_t>('l' ^ 0x3D),
            static_cast<std::uint8_t>('o' ^ 0x37)};
        const cyber::ui::WebSocketFrame parsed = cyber::ui::parse_ws_frame(masked_client_frame);
        require(parsed.opcode == 1, "parsed opcode mismatch");
        require(parsed.text == "hello", "parsed text mismatch");

        std::cout << "websocket_selftest: ok\n";
    }
    catch (const std::exception& ex)
    {
        std::cerr << "websocket_selftest failed: " << ex.what() << '\n';
        return 1;
    }
    return 0;
}
