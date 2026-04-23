cmake_minimum_required(VERSION 3.20)

project(rdma_module LANGUAGES CXX)

find_package(Threads REQUIRED)

add_library(
  rdma_core
  src/logger.cpp
  src/config.cpp
  src/device.cpp
  src/context.cpp
  src/protection_domain.cpp
  src/completion_queue.cpp
  src/event_channel.cpp
  src/memory_region.cpp
  src/buffer_pool.cpp
  src/queue_pair.cpp
  src/connection.cpp
  src/handshake.cpp
  src/rdma_server.cpp
  src/rdma_client.cpp
  src/transport_rdma.cpp
  src/poller.cpp
  src/worker_session.cpp
  src/metrics.cpp
)

target_include_directories(
  rdma_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rdma_core PUBLIC Threads::Threads)

target_compile_features(rdma_core PUBLIC cxx_std_20)
target_compile_options(rdma_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(rdma_pingpong examples/pingpong.cpp)
target_link_libraries(rdma_pingpong PRIVATE rdma_core)

add_executable(rdma_send_recv_demo examples/send_recv_demo.cpp)
target_link_libraries(rdma_send_recv_demo PRIVATE rdma_core)

add_executable(rdma_write_demo examples/write_demo.cpp)
target_link_libraries(rdma_write_demo PRIVATE rdma_core)

add_executable(rdma_worker_transport_demo examples/worker_transport_demo.cpp)
target_link_libraries(rdma_worker_transport_demo PRIVATE rdma_core)

add_executable(rdma_latency_bench benchmark/latency_bench.cpp)
target_link_libraries(rdma_latency_bench PRIVATE rdma_core)

add_executable(rdma_throughput_bench benchmark/throughput_bench.cpp)
target_link_libraries(rdma_throughput_bench PRIVATE rdma_core)

add_executable(rdma_multi_conn_bench benchmark/multi_conn_bench.cpp)
target_link_libraries(rdma_multi_conn_bench PRIVATE rdma_core)
