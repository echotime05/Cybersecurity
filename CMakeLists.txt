cmake_minimum_required(VERSION 3.16)

project(cyber_tank_design LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "This course deployment is Windows-only.")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(cyber_common
    src/common/auth_credentials.cpp
    src/common/auth_flow.cpp
    src/common/config.cpp
    src/common/crypto.cpp
    src/common/log_parser.cpp
    src/common/logger.cpp
    src/common/net_packet.cpp
    src/common/net_socket.cpp
    src/common/packet.cpp
    src/common/protocol_event.cpp
    src/common/protocol_payloads.cpp
    src/common/role_runtime.cpp
    src/game/app_payload_codec.cpp
    src/game/game_protocol.cpp
    src/game/game_non_repudiation.cpp
    src/game/game_world.cpp
    src/game/battle_room.cpp
    src/game/tank_game_server.cpp
    src/game/tank_game_client.cpp
    src/monitor/protocol_monitor.cpp
    src/ui/websocket.cpp
    src/ui/ui_bridge.cpp
)

target_include_directories(cyber_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(cyber_common PUBLIC ws2_32 advapi32)

function(add_role_target target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE cyber_common)
endfunction()

add_role_target(as_server src/roles/as/main.cpp)
add_role_target(tgs_server src/roles/tgs/main.cpp)
add_role_target(v_server src/roles/v/main.cpp)
add_role_target(client src/roles/client/main.cpp)
add_role_target(monitor src/roles/monitor/main.cpp)

add_executable(protocol_selftest tests/protocol_selftest.cpp)
target_link_libraries(protocol_selftest PRIVATE cyber_common)

add_executable(log_selftest tests/log_selftest.cpp)
target_link_libraries(log_selftest PRIVATE cyber_common)

add_executable(net_packet_selftest tests/net_packet_selftest.cpp)
target_link_libraries(net_packet_selftest PRIVATE cyber_common)

add_executable(crypto_selftest tests/crypto_selftest.cpp)
target_link_libraries(crypto_selftest PRIVATE cyber_common)

add_executable(auth_credentials_selftest tests/auth_credentials_selftest.cpp)
target_link_libraries(auth_credentials_selftest PRIVATE cyber_common)

add_executable(auth_payload_selftest tests/auth_payload_selftest.cpp)
target_link_libraries(auth_payload_selftest PRIVATE cyber_common)

add_executable(game_protocol_selftest tests/game_protocol_selftest.cpp)
target_link_libraries(game_protocol_selftest PRIVATE cyber_common)

add_executable(app_payload_codec_selftest tests/app_payload_codec_selftest.cpp)
target_link_libraries(app_payload_codec_selftest PRIVATE cyber_common)

add_executable(protocol_event_selftest tests/protocol_event_selftest.cpp)
target_link_libraries(protocol_event_selftest PRIVATE cyber_common)

add_executable(protocol_monitor_tail_selftest tests/protocol_monitor_tail_selftest.cpp)
target_link_libraries(protocol_monitor_tail_selftest PRIVATE cyber_common)

add_executable(game_world_selftest tests/game_world_selftest.cpp)
target_link_libraries(game_world_selftest PRIVATE cyber_common)

add_executable(battle_room_selftest tests/battle_room_selftest.cpp)
target_link_libraries(battle_room_selftest PRIVATE cyber_common)

add_executable(tank_game_server_flow_selftest tests/tank_game_server_flow_selftest.cpp)
target_link_libraries(tank_game_server_flow_selftest PRIVATE cyber_common)

add_executable(tank_game_client_options_selftest
    tests/tank_game_client_options_selftest.cpp)
target_link_libraries(tank_game_client_options_selftest PRIVATE cyber_common)

add_executable(encrypted_plaintext_rejection_client
    tests/encrypted_plaintext_rejection_client.cpp)
target_link_libraries(encrypted_plaintext_rejection_client PRIVATE cyber_common)

add_executable(websocket_selftest tests/websocket_selftest.cpp)
target_link_libraries(websocket_selftest PRIVATE cyber_common)

add_executable(ui_bridge_selftest tests/ui_bridge_selftest.cpp)
target_link_libraries(ui_bridge_selftest PRIVATE cyber_common)

enable_testing()
add_test(NAME protocol_selftest COMMAND protocol_selftest)
set_tests_properties(protocol_selftest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_test(NAME log_selftest COMMAND log_selftest)
add_test(NAME net_packet_selftest COMMAND net_packet_selftest)
add_test(NAME crypto_selftest COMMAND crypto_selftest)
add_test(NAME auth_credentials_selftest COMMAND auth_credentials_selftest)
add_test(NAME auth_payload_selftest COMMAND auth_payload_selftest)
add_test(NAME game_protocol_selftest COMMAND game_protocol_selftest)
add_test(NAME app_payload_codec_selftest COMMAND app_payload_codec_selftest)
add_test(NAME protocol_event_selftest COMMAND protocol_event_selftest)
add_test(NAME protocol_monitor_tail_selftest COMMAND protocol_monitor_tail_selftest)
add_test(NAME game_world_selftest COMMAND game_world_selftest)
add_test(NAME battle_room_selftest COMMAND battle_room_selftest)
add_test(NAME tank_game_server_flow_selftest COMMAND tank_game_server_flow_selftest)
add_test(NAME tank_game_client_options_selftest
         COMMAND tank_game_client_options_selftest)
add_test(NAME websocket_selftest COMMAND websocket_selftest)
add_test(NAME ui_bridge_selftest COMMAND ui_bridge_selftest)

add_test(
    NAME connect_probe_selftest
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/connect_probe_selftest.ps1
            -BuildDir $<TARGET_FILE_DIR:client>
)

add_test(
    NAME auth_flow_selftest
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_flow_selftest.ps1
            -BuildDir $<TARGET_FILE_DIR:client>
)

add_test(
    NAME auth_encrypted_game_flow_selftest
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/auth_encrypted_game_flow_selftest.ps1
            -BuildDir $<TARGET_FILE_DIR:client>
)
