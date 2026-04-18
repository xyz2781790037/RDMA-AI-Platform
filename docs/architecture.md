# 架构设计

RDMA-AI-Platform 分成控制面和数据面两层：

## 控制面

控制面由 Go 调度器负责，主要职责：

- Worker 注册与标签管理
- 心跳保活与离线检测
- 任务提交、分配、完成和查询
- 暴露 HTTP API，并保留 `proto/scheduler.proto` 作为 gRPC 契约

调度器当前采用基于负载的简化策略：

1. 过滤掉离线 Worker
2. 在活跃 Worker 中选择 `running_tasks` 最少的节点
3. 当负载相同时按轮询顺序打散

## 数据面

数据面由 C++ Worker 和 `rdma/` 核心模块组成：

- `memory.*` 管理注册内存和共享 payload
- `qp.*` 建模 Queue Pair 状态迁移
- `completion.*` 管理工作请求完成事件
- `transport.*` 提供零拷贝风格传输语义

当前默认 backend 是 in-memory transport，用来模拟真实 RDMA 的关键行为：

- payload 以 `shared_ptr` 形式跨模块传递
- 发送侧不复制大块数据
- 接收侧通过完成队列和消息轮询处理数据

## 执行路径

一次典型任务的执行流：

1. Client 向 Scheduler 提交任务
2. Scheduler 选择目标 Worker 并更新任务状态
3. Worker Runtime 根据目标节点建立 Queue Pair
4. Task Executor 将张量序列化后注册到内存区
5. Transport 以共享 payload 方式投递消息
6. 接收端 Worker 轮询收到消息并驱动后续计算

## 为什么先做抽象层

真实 RDMA 依赖硬件、驱动和 verbs API，直接写会让工程初始化成本很高。这个版本先把：

- 调度接口
- 数据结构
- Worker 生命周期
- 基准测试入口

全部固定下来，后续只需要在 transport 和 memory 层替换成真实实现即可。
