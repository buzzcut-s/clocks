cmake_minimum_required(VERSION 3.21.0)

project(
  clocks
  VERSION 0.1.0
  LANGUAGES C)

set(CMAKE_C_STANDARD 99)

add_subdirectory(lib)
add_subdirectory(src)
