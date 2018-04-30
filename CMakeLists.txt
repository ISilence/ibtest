cmake_minimum_required(VERSION 3.0)
project(gpu_comp)

set(DIR_ROOT ${PROJECT_SOURCE_DIR})
set(DIR_SRC ${DIR_ROOT}/src)
set(DIR_INC ${DIR_ROOT}/include)
set(DIR_BIN ${DIR_ROOT}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DIR_BIN})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DIR_BIN})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${DIR_BIN})

add_subdirectory(src/compressor)
add_subdirectory(src/ib_pingpong)
#add_subdirectory(benchmark/ib)
#add_subdirectory(benchmark/gpu2gpu)
