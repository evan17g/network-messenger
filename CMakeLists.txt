cmake_minimum_required(VERSION 3.16)

project(network_messenger)

add_executable(network_client
    src/client.cpp
)

add_executable(network_server
    src/server.cpp
)

target_include_directories(network_client
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(network_server
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)