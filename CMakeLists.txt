cmake_minimum_required(VERSION 3.18)

project(json_parser LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# tinyjson library
add_library(tinyjson tinyjson.cpp tinyjson.hpp)

target_compile_options(
  tinyjson
  PRIVATE -Wall
          -Wextra
          -Weffc++
          -Werror=uninitialized
          -Werror=return-type
          -Wconversion
          -Wsign-compare
          -Werror=unused-result
          -Werror=suggest-override
          -Wzero-as-null-pointer-constant
          -Wmissing-declarations
          -Wold-style-cast
          -Werror=vla
          -Wnon-virtual-dtor)

add_executable(json_parser main.cpp)
target_link_libraries(json_parser PRIVATE tinyjson)
