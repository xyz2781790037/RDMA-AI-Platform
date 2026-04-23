# Tuning Notes

当前模块虽然还是内存内 backend，但已经暴露出几个后续调优位点：

- `CompletionQueueConfig.depth`：控制 CQ 队列深度
- `TransportConfig.mailbox_capacity`：控制每个节点 inbox 的积压上限
- `BufferPoolConfig.buffers`：控制预注册缓冲区数量
- `BufferPoolConfig.buffer_size`：控制每个缓冲区大小
- `QueuePairConfig.max_send_wr/max_recv_wr`：保留与真实 verbs 接近的 WR 配置

如果后续接入真实 RDMA，可以优先把这些参数映射到：

- CQ 深度
- QP WR 大小
- MR 注册批次
- 发送队列批量 doorbell 策略
- 完成轮询线程绑核
