cmake_minimum_required(VERSION 2.4)

project(ss4)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17")
set(CMAKE_CXX_STANDARD 17)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(ss4 main.cpp document.cpp read_input_functions.cpp request_queue.cpp search_server.cpp string_processing.cpp)
