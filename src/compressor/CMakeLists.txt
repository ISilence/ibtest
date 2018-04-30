cmake_minimum_required(VERSION 3.0)
project(compressor)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenCL REQUIRED)


add_library(compressor
    ${DIR_INC}/cl.hpp
    ${DIR_INC}/compressor.hpp
    cl.cpp

    ${DIR_INC}/bdi_compressor.hpp
    bdi_compressor.cpp
)

add_custom_command(
        TARGET compressor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${DIR_SRC}/compressor/kernel.cl
        ${DIR_BIN}/kernel.cl
        )

configure_file(kernel.cl ${DIR_BIN}/kernel.cl COPYONLY)

include_directories(${DIR_INC})
target_link_libraries(compressor OpenCL::OpenCL)
