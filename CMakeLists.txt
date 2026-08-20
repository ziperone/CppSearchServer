cmake_minimum_required(VERSION 3.16)

project(CppSearchServer LANGUAGES CXX)

find_package(Threads REQUIRED)

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
    src/net/TimerQueue.cpp
    src/net/EventLoopThread.cpp
    src/net/EventLoopGroup.cc
    src/net/TcpConnection.cpp
    src/concurrency/WorkerPool.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/search/DocumentLoader.cpp
    src/search/InvertedIndex.cpp
    src/search/LruCache.cc
    src/search/MarkdownChunker.cpp
    src/search/SearchApplication.cpp
    src/search/SearchService.cpp
    src/search/Tokenizer.cpp
)

target_include_directories(cpp_search_server PRIVATE src)

target_compile_options(cpp_search_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(cpp_search_server PRIVATE Threads::Threads)

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

    add_executable(tokenizer_test
        src/search/Tokenizer.cpp
        tests/TokenizerTest.cpp
    )

    target_include_directories(tokenizer_test PRIVATE src)

    target_compile_options(tokenizer_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME tokenizer_test COMMAND tokenizer_test)

    add_executable(inverted_index_test
        src/search/InvertedIndex.cpp
        src/search/Tokenizer.cpp
        tests/InvertedIndexTest.cpp
    )

    target_include_directories(inverted_index_test PRIVATE src)

    target_compile_options(inverted_index_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME inverted_index_test COMMAND inverted_index_test)

    add_executable(lru_cache_test
        src/search/LruCache.cc
        tests/LruCacheTest.cpp
    )

    target_include_directories(lru_cache_test PRIVATE src)

    target_compile_options(lru_cache_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(lru_cache_test PRIVATE Threads::Threads)

    add_test(NAME lru_cache_test COMMAND lru_cache_test)

    add_executable(search_service_test
        src/search/InvertedIndex.cpp
        src/search/SearchService.cpp
        src/search/Tokenizer.cpp
        tests/SearchServiceTest.cpp
    )

    target_include_directories(search_service_test PRIVATE src)

    target_compile_options(search_service_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME search_service_test COMMAND search_service_test)

    add_executable(search_application_test
        src/http/HttpRequest.cpp
        src/http/HttpResponse.cpp
        src/search/DocumentLoader.cpp
        src/search/InvertedIndex.cpp
        src/search/LruCache.cc
        src/search/MarkdownChunker.cpp
        src/search/SearchApplication.cpp
        src/search/SearchService.cpp
        src/search/Tokenizer.cpp
        tests/SearchApplicationTest.cpp
    )

    target_include_directories(search_application_test PRIVATE src)

    target_compile_options(search_application_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(
        NAME search_application_test
        COMMAND search_application_test ${CMAKE_SOURCE_DIR}/data/docs
    )

    add_executable(http_request_test
        src/http/HttpRequest.cpp
        tests/HttpRequestTest.cpp
    )

    target_include_directories(http_request_test PRIVATE src)

    target_compile_options(http_request_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME http_request_test COMMAND http_request_test)

    add_executable(timer_queue_test
        src/net/TimerQueue.cpp
        tests/TimerQueueTest.cpp
    )

    target_include_directories(timer_queue_test PRIVATE src)

    target_compile_options(timer_queue_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME timer_queue_test COMMAND timer_queue_test)

    add_executable(event_loop_timer_test
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/TimerQueue.cpp
        tests/EventLoopTimerTest.cpp
    )

    target_include_directories(event_loop_timer_test PRIVATE src)

    target_compile_options(event_loop_timer_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME event_loop_timer_test COMMAND event_loop_timer_test)

    add_executable(event_loop_task_queue_test
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/TimerQueue.cpp
        tests/EventLoopTaskQueueTest.cpp
    )

    target_include_directories(event_loop_task_queue_test PRIVATE src)

    target_compile_options(event_loop_task_queue_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME event_loop_task_queue_test COMMAND event_loop_task_queue_test)

    add_executable(event_loop_thread_test
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/EventLoopThread.cpp
        src/net/TimerQueue.cpp
        tests/EventLoopThreadTest.cpp
    )

    target_include_directories(event_loop_thread_test PRIVATE src)

    target_compile_options(event_loop_thread_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(event_loop_thread_test PRIVATE Threads::Threads)

    add_test(NAME event_loop_thread_test COMMAND event_loop_thread_test)

    add_executable(event_loop_group_test
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/EventLoopThread.cpp
        src/net/EventLoopGroup.cc
        src/net/TimerQueue.cpp
        tests/EventLoopGroupTest.cpp
    )

    target_include_directories(event_loop_group_test PRIVATE src)

    target_compile_options(event_loop_group_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(event_loop_group_test PRIVATE Threads::Threads)

    add_test(NAME event_loop_group_test COMMAND event_loop_group_test)

    add_executable(worker_pool_test
        src/concurrency/WorkerPool.cpp
        tests/WorkerPoolTest.cpp
    )

    target_include_directories(worker_pool_test PRIVATE src)

    target_compile_options(worker_pool_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(worker_pool_test PRIVATE Threads::Threads)

    add_test(NAME worker_pool_test COMMAND worker_pool_test)

    add_executable(tcp_connection_idle_timeout_test
        src/net/Buffer.cpp
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/Socket.cpp
        src/net/TimerQueue.cpp
        src/net/TcpConnection.cpp
        tests/TcpConnectionIdleTimeoutTest.cpp
    )

    target_include_directories(tcp_connection_idle_timeout_test PRIVATE src)

    target_compile_options(tcp_connection_idle_timeout_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME tcp_connection_idle_timeout_test COMMAND tcp_connection_idle_timeout_test)

    add_executable(tcp_connection_idle_refresh_test
        src/net/Buffer.cpp
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/Socket.cpp
        src/net/TimerQueue.cpp
        src/net/TcpConnection.cpp
        tests/TcpConnectionIdleRefreshTest.cpp
    )

    target_include_directories(tcp_connection_idle_refresh_test PRIVATE src)

    target_compile_options(tcp_connection_idle_refresh_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME tcp_connection_idle_refresh_test COMMAND tcp_connection_idle_refresh_test)

    add_executable(tcp_connection_async_request_test
        src/concurrency/WorkerPool.cpp
        src/net/Buffer.cpp
        src/net/Channel.cpp
        src/net/Epoller.cpp
        src/net/EventLoop.cpp
        src/net/Socket.cpp
        src/net/TimerQueue.cpp
        src/net/TcpConnection.cpp
        tests/TcpConnectionAsyncRequestTest.cpp
    )

    target_include_directories(tcp_connection_async_request_test PRIVATE src)

    target_compile_options(tcp_connection_async_request_test PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    target_link_libraries(tcp_connection_async_request_test PRIVATE Threads::Threads)

    add_test(NAME tcp_connection_async_request_test COMMAND tcp_connection_async_request_test)
endif()
