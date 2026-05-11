cmake_minimum_required(VERSION 3.16)

project(cyber_tank_design LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(cyber_common
    src/common/config.cpp
    src/common/packet.cpp
    src/common/role_runtime.cpp
)

target_include_directories(cyber_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

function(add_role_target target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE cyber_common)
endfunction()

add_role_target(as_server src/as_server/main.cpp)
add_role_target(tgs_server src/tgs_server/main.cpp)
add_role_target(v_server src/v_server/main.cpp)
add_role_target(client src/client/main.cpp)

add_executable(protocol_selftest tests/protocol_selftest.cpp)
target_link_libraries(protocol_selftest PRIVATE cyber_common)

enable_testing()
add_test(NAME protocol_selftest COMMAND protocol_selftest)
set_tests_properties(protocol_selftest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
