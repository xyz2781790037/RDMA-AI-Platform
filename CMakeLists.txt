cmake_minimum_required(VERSION 3.20)
project(rdma_ai_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(rdma)
add_subdirectory(worker)

add_executable(latency_test rdma/benchmark/latency_bench.cpp)
target_link_libraries(latency_test PRIVATE rdma_core)

add_executable(throughput_test rdma/benchmark/throughput_bench.cpp)
target_link_libraries(throughput_test PRIVATE rdma_core)

add_executable(tcp_compare benchmark/tcp_compare.cpp)
