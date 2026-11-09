cmake_minimum_required(VERSION 3.20)
project(lab04_station LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(Threads REQUIRED)

# Boost.Interprocess — только заголовки. Как установить Boost — в README.md.
find_package(Boost 1.80 REQUIRED CONFIG)

add_executable(station main.cpp station.cpp journal.cpp spawn.cpp check.cpp)
target_link_libraries(station PRIVATE Boost::headers Threads::Threads)

# shm_open на Linux со старой glibc живёт в librt.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(station PRIVATE rt)
endif()
