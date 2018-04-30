cmake_minimum_required(VERSION 3.0)
project(gpu2gpu)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCL REQUIRED)

include_directories(../../include)
add_executable(gpu2gpu
    main.cpp
        ../../src/timer.hpp
    ../../include/cl.hpp
    ../../include/compressor.hpp
    ../../include/bdi_compressor.hpp
)
target_link_libraries(gpu2gpu OpenCL::OpenCL compressor)
