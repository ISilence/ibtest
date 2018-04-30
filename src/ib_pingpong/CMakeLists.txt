cmake_minimum_required(VERSION 3.0)
project(def_pingpong)

set(CMAKE_CXX_STANDARD 17)

include_directories(${DIR_INC})
include_directories(/home/sil/dev/rdma-core/include/)
link_directories(/home/sil/dev/rdma-core/lib/)

add_executable(def_pingpong
        ../ib_init.c
        ../timer.hpp
        pingpong.cpp
        ${DIR_INC}/ib_init.hpp
        ${DIR_INC}/testing.hpp
)
target_link_libraries(def_pingpong compressor ibverbs)


add_executable(cl_pingpong
        ../ib_init.c
        ../timer.hpp
        cl_pingpong.cpp
        ${DIR_INC}/compressor.hpp
        ${DIR_INC}/ib_init.hpp
        ${DIR_INC}/testing.hpp
        )
target_link_libraries(cl_pingpong compressor ibverbs)


add_executable(comp_pingpong
        ../ib_init.c
        ../timer.hpp
        compression_pingpong.cpp
        ${DIR_INC}/compressor.hpp
        ${DIR_INC}/ib_init.hpp
        ${DIR_INC}/testing.hpp
        )
target_link_libraries(comp_pingpong compressor ibverbs)


add_executable(server
        ../ib_init.c
        ../timer.hpp
        server.cpp
        ${DIR_INC}/compressor.hpp
        ${DIR_INC}/ib_init.hpp
        ${DIR_INC}/testing.hpp
        )
target_link_libraries(server compressor ibverbs)
