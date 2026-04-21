cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(rdma_worker LANGUAGES CXX)
endif()

find_package(Threads REQUIRED)

add_executable(
  worker_node
  main.cpp
  src/worker_node.cpp
  src/task_queue.cpp
  src/executor.cpp
  src/scheduler_client.cpp
  src/transport_tcp.cpp
  src/logger.cpp
  runtime/thread_pool.cpp
  runtime/timer.cpp
  mock/fake_model.cpp
)

target_include_directories(
  worker_node
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/mock
)

set_target_properties(
  worker_node
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_options(worker_node PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(worker_node PRIVATE Threads::Threads)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config/worker.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/config/worker.yaml
  COPYONLY
)
