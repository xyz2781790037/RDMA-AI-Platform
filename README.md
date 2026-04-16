# RDMA-AI-Platform

一个基于 RDMA 语义抽象的分布式 AI 通信平台骨架，包含：

- Go 控制面调度器，负责 Worker 注册、心跳、任务分配和集群概览
- C++ Worker 执行节点，负责任务执行、张量封装和低延迟消息处理
- `rdma/` 核心模块，抽象 Queue Pair、注册内存、完成队列和零拷贝风格传输
- `benchmark/` 性能测试，提供 RDMA 风格路径和 TCP 回环的对比基准
- `proto/` 控制面协议定义，以及完整的架构/设计/基准文档

当前版本默认提供一个可编译、可运行的 in-memory RDMA backend，用来模拟注册内存和完成队列语义，便于在没有 RDMA 网卡和 `libibverbs` 的环境下先完成平台开发。后续可在 `rdma/transport.*` 基础上替换成真实 verbs 实现。

## 目录结构

```text
rdma-ai-platform/
├── README.md
├── Makefile
├── CMakeLists.txt
├── scheduler/
│   ├── cmd/
│   ├── internal/
│   └── main.go
├── worker/
│   ├── main.cpp
│   ├── task/
│   ├── runtime/
│   └── model/
├── rdma/
│   ├── CMakeLists.txt
│   ├── include/rdma/
│   ├── src/
│   ├── examples/
│   ├── benchmark/
│   ├── scripts/
│   └── docs/
├── benchmark/
│   ├── latency_test.cpp
│   ├── throughput_test.cpp
│   └── tcp_compare.cpp
├── proto/
│   └── scheduler.proto
├── docs/
│   ├── architecture.md
│   ├── rdma-design.md
│   └── benchmark.md
└── scripts/
    ├── start_cluster.sh
    └── benchmark.sh
```

## 核心能力

### 1. 控制面调度

- Worker 注册与状态维护
- 心跳续约与离线检测
- 基于负载的简单调度策略
- 任务状态跟踪与集群概览

### 2. 数据面传输

- Queue Pair 生命周期建模
- 注册内存管理
- 完成队列轮询
- 零拷贝风格的共享 payload 传输

### 3. Worker 执行

- 张量元数据封装
- AI 任务描述与执行器
- Worker Runtime 和消息轮询
- 基于 RDMA transport 的任务分发示例

### 4. Benchmark

- RDMA 风格路径的延迟测试
- RDMA 风格路径的吞吐测试
- TCP loopback 延迟/吞吐对比

## 快速开始

### 构建 Go 调度器

```bash
cd scheduler
go build ./...
```

### 构建 C++ Worker 与 Benchmark

```bash
./rdma/scripts/build.sh
```

### 启动调度器

```bash
cd scheduler
go run .
```

默认监听地址是 `:8080`，可通过环境变量 `SCHEDULER_ADDR` 覆盖。

### 启动示例集群

```bash
./scripts/start_cluster.sh
```

### 运行 Benchmark

```bash
./scripts/benchmark.sh
```

## Scheduler API

### `POST /workers/register`

注册一个 Worker，示例请求体：

```json
{
  "id": "worker-a",
  "address": "10.0.0.11:9000",
  "capacity": 2,
  "labels": ["gpu", "rdma"]
}
```

### `POST /workers/heartbeat`

上报 Worker 当前负载和心跳：

```json
{
  "id": "worker-a",
  "running_tasks": 1
}
```

### `POST /tasks/submit`

提交一个 AI 通信任务：

```json
{
  "model": "llama2-13b",
  "operation": "tensor-broadcast",
  "payload_ref": "rdma://tensor/step-1024",
  "metadata": {
    "layer": "12",
    "dtype": "fp16"
  }
}
```

### `GET /cluster/overview`

查看集群中 Worker 和任务状态。

## 文档

- 架构说明：`docs/architecture.md`
- RDMA 设计：`docs/rdma-design.md`
- 基准测试：`docs/benchmark.md`
- RDMA 模块内部文档：`rdma/docs/`

## 后续扩展方向

- 接入真实 `libibverbs` / RoCE / InfiniBand backend
- 把 `proto/scheduler.proto` 生成 gRPC server/client
- 增加模型分片、all-reduce、parameter server 等任务类型
- 接入 Prometheus 指标和 tracing
