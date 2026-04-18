# RDMA 设计说明

## 目标

本项目希望给 AI 推理/训练场景提供一个低延迟、低 CPU 开销的数据交换层，重点支持：

- 张量块广播
- 激活值分发
- 模型分片拉取
- Worker 间状态同步

## 核心抽象

### Queue Pair

`rdma::QueuePair` 负责建模连接状态：

- `reset`
- `init`
- `ready_to_receive`
- `ready_to_send`
- `error`

这样做的目的是让真实 verbs 迁移时，接口层不需要大改。

### 注册内存

`rdma::MemoryRegistry` 负责生成一个注册后的内存句柄：

- 每块内存都有唯一 key
- payload 使用 `shared_ptr<std::vector<std::byte>>`
- 发送时直接共享同一块底层存储

这就是当前版本“零拷贝风格”语义的来源。

### 完成队列

`rdma::CompletionQueue` 模拟 CQ：

- 发送成功后投递 completion
- 业务侧通过 `Poll` 获取结果
- 后续可以把状态码映射到真正的 WC status

### Transport

`rdma::Transport` 当前是 in-memory broker，但接口语义已经接近真实 RDMA：

- `Connect`
- `PostSend`
- `PollReceive`

未来替换成 verbs backend 时，业务层无需改动任务模型和 Worker Runtime。

## 与真实 RDMA 的差距

当前版本没有直接做以下内容：

- Memory Region 注册到 NIC
- Queue Pair 到设备上下文的绑定
- Completion Channel / Event FD
- RDMA Read / Write / Send verbs
- RKey / LKey 协商

这些能力建议在后续版本中新增 `verbs_transport.cpp` 和 `verbs_memory.cpp` 实现。
