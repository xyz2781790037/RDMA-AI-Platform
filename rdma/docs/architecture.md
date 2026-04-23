# RDMA Module Architecture

这个目录实现的是一个可编译的 RDMA 风格数据面骨架，当前 backend 仍然是 in-memory broker，但接口已经拆成接近真实 RDMA 的层次：

- `Device` / `Context` / `ProtectionDomain`：保留 verbs 风格资源层
- `MemoryRegion` / `BufferPool`：管理注册内存和复用缓冲区
- `CompletionQueue` / `EventChannel`：模拟完成通知和轮询
- `QueuePair` / `Connection` / `Handshake`：描述链路建立和状态推进
- `TransportRdma`：真正承担消息投递，后续可替换成 verbs backend
- `RdmaClient` / `RdmaServer` / `WorkerSession`：给上层业务提供更直接的任务接口

当前版本的重点是把目录、抽象层次和最小可运行路径补齐，方便后续逐步换成真实 NIC/verbs 实现。
