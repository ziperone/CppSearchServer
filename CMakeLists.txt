cmake_minimum_required(VERSION 3.16)

project(CppSearchServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_executable(cpp_search_server
    src/main.cpp
    src/net/Buffer.cpp
    src/net/Channel.cpp
    src/net/EventLoop.cpp
    src/net/Epoller.cpp
    src/net/Socket.cpp
    src/net/TcpConnection.cpp
    src/http/HttpResponse.cpp
)

target_include_directories(cpp_search_server PRIVATE src)

target_compile_options(cpp_search_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
