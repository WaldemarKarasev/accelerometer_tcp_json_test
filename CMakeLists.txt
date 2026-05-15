cmake_minimum_required(VERSION 3.16)

project(accelerometer_distributed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# boost
find_package(Boost REQUIRED COMPONENTS system)

# nlohmann
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

add_library(accel_common
    src/common/json_protocol.cpp
    src/common/tcp_client.cpp
)

target_include_directories(accel_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(accel_common
    PUBLIC
        Boost::system
        nlohmann_json::nlohmann_json
)

add_executable(server
    src/server/main.cpp
    src/server/tcp_server.cpp
    src/server/client_session.cpp
    src/server/server_state.cpp
)

target_link_libraries(server
    PRIVATE
        accel_common
)

add_executable(node_a
    src/node_a/main.cpp
)

target_link_libraries(node_a
    PRIVATE
        accel_common
)

add_executable(node_b
    src/node_b/main.cpp
)

target_link_libraries(node_b
    PRIVATE
        accel_common
)