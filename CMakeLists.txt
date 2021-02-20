project(main)
cmake_minimum_required (VERSION 2.6)

SET(CMAKE_CXX_COMPILER "g++")
include_directories(./include)
include_directories(./images)
include_directories(./Web)
aux_source_directory(./src dir_current)
#将源文件编译成成名为 main的可执行文件
add_executable(./bin/main ${dir_current})
set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -O3 -pthread")