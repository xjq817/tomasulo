cmake_minimum_required(VERSION 3.16)
project(tomasulo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS " -Ofast")

include_directories(.)

add_executable(code main.cpp)
