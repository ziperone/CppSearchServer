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
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/search/DocumentLoader.cpp
    src/search/MarkdownChunker.cpp
)

target_include_directories(cpp_search_server PRIVATE src)

target_compile_options(cpp_search_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

include(CTest)

if(BUILD_TESTING)
    add_executable(markdown_chunker_test
        src/search/MarkdownChunker.cpp
        tests/MarkdownChunkerTest.cpp
    )

    target_include_directories(markdown_chunker_test PRIVATE src)

    target_compile_options(markdown_chunker_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME markdown_chunker_test COMMAND markdown_chunker_test)

    add_executable(document_loader_test
        src/search/DocumentLoader.cpp
        src/search/MarkdownChunker.cpp
        tests/DocumentLoaderTest.cpp
    )

    target_include_directories(document_loader_test PRIVATE src)

    target_compile_options(document_loader_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME document_loader_test
        COMMAND document_loader_test ${CMAKE_SOURCE_DIR}/data/docs
    )
endif()
