cmake_minimum_required(VERSION 3.16)
project(SoftHeap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(softheap
    main.cpp
)

target_include_directories(softheap PRIVATE ${CMAKE_SOURCE_DIR})
