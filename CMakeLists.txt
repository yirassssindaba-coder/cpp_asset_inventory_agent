cmake_minimum_required(VERSION 3.16)
project(asset_inventory_agent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(asset_agent
    src/agent_main.cpp
    src/inventory.cpp
    src/http_client.cpp
    src/mini_json.cpp
    src/logger.cpp
    src/platform.cpp
)

add_executable(asset_server
    src/server_main.cpp
    src/http_server.cpp
    src/file_store.cpp
    src/inventory.cpp
    src/platform.cpp
    src/mini_json.cpp
    src/logger.cpp
)
if (WIN32)
  target_compile_definitions(asset_agent PRIVATE _WIN32_WINNT=0x0601)
  target_compile_definitions(asset_server PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(asset_agent ws2_32)
  target_link_libraries(asset_server ws2_32)
endif()
