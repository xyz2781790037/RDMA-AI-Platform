# Queue Pair State Machine

当前 `QueuePair` 保留了一个最小但清晰的状态机：

1. `reset`
2. `init`
3. `ready_to_receive`
4. `ready_to_send`
5. `error`

允许的正常迁移是：

- `reset -> init`
- `init -> ready_to_receive`
- `ready_to_receive -> ready_to_send`

任意阶段都允许进入 `error`。

`Connection::Establish()` 会顺序执行这三步 bring-up，所以业务层只需要关心“链路是否就绪”，不用每次手动推进状态。
