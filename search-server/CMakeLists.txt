cmake_minimum_required(VERSION 2.4)

project(ss8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17")
set(CMAKE_CXX_STANDARD 17)

find_package(TBB REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
target_link_libraries(Threads::Threads)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories(${PROJECT_SOURCE_DIR})

add_executable(ss8
    main.cpp
    document.cpp
    read_input_functions.cpp
    request_queue.cpp
    search_server.cpp
    string_processing.cpp
    remove_duplicates.cpp
    test_example_functions.cpp
    process_queries.cpp
)

target_link_libraries(ss8 PUBLIC TBB::tbb)
