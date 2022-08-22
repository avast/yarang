cmake_minimum_required(VERSION 3.11)
project(yarang
    LANGUAGES CXX C
    VERSION 0.0.1
)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(YARANG_TESTS "Build yarang tests" OFF)

find_package(Threads REQUIRED)
find_package(HyperScan 5.3.0 REQUIRED)
find_package(Yaramod 3.8.1 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
if(YARANG_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
