cmake_minimum_required(VERSION 3.20)
project(test_package CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(sdl_painter REQUIRED)

add_executable(test_package src/main.cpp)
target_link_libraries(test_package sdl_painter::sdl_painter)
