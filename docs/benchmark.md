# Benchmark 说明

## 目标

Benchmark 用来回答两个问题：

1. RDMA 风格路径在当前抽象层下的延迟和吞吐大致是多少
2. 与本机 TCP loopback 相比，接口层的额外开销有多少

## 用例

### `benchmark/latency_test.cpp`

发送固定大小的 payload，统计平均单向发送延迟。

### `benchmark/throughput_test.cpp`

复用同一块注册内存，持续发送消息，统计 GB/s。

### `benchmark/tcp_compare.cpp`

基于 Linux socket 做本机回环 ping-pong，统计平均 RTT 和近似吞吐。

## 运行方法

```bash
./scripts/benchmark.sh
```

## 结果解读

当前 RDMA benchmark 测的是抽象层和零拷贝风格数据路径，不代表真实硬件 RDMA 最终指标。它主要用于：

- 对比不同实现的框架开销
- 验证 worker/runtime/transport 路径是否退化
- 为后续 verbs backend 提供基线
