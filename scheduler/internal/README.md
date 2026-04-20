  model

  - model/task.go：定义任务模型、任务状态、提交/完成任务的请求结构，以及任务元数据的拷贝工具函数。
  - model/node.go：定义 worker 节点模型、节点状态、注册/心跳请求结构，以及根据心跳和容量推导节点状态的逻辑。
  - model/response.go：定义对外返回的通用响应结构，主要是健康检查、错误响应和集群总览响应。

  util

  - util/id.go：把自增计数格式化成固定样式的任务 ID，比如 task-000001。

  api

  - api/router.go：HTTP 路由入口，组装 Handler，注册所有 REST 接口，同时提供 JSON 解码、JSON 输出、错误输出这些通用辅助函数。
  - api/register.go：HTTP 的 worker 注册接口处理函数。
  - api/heartbeat.go：HTTP 的 worker 心跳上报接口处理函数。
  - api/submit.go：HTTP 的任务提交和任务完成回报接口处理函数。
  - api/status.go：HTTP 的健康检查、按 ID 查任务、查看集群状态总览接口处理函数。

  registry

  - registry/registry.go：节点注册表，负责保存 worker、处理注册、处理心跳、查询节点、刷新离线状态。

  service

  - service/node_service.go：节点相关业务入口，调用 registry 更新节点信息，再触发调度器尝试分配待执行任务。
  - service/task_service.go：任务相关业务入口，负责校验提交参数、创建任务、完成任务、查询任务、获取集群状态，并在关键动作后触
    发调度。

  monitor

  - monitor/checker.go：后台巡检器，定时刷新节点在线状态，并再次尝试调度挂起任务。
  - monitor/deliverer.go：后台投递器，扫描已经分配到 worker 的运行中任务，通过 TCP 把任务分配消息发给对应 worker。

  storage

  - storage/memory.go：线程安全的内存存储，保存所有节点、任务、轮转顺序、下次选点位置、任务计数器，并通过读写锁保护并发访问。

  scheduler

  - scheduler/strategy.go：定义“如何比较两个 worker 谁更适合接任务”的策略接口，并实现默认的最小负载策略。
  - scheduler/scheduler.go：调度器对外主入口，负责刷新节点状态、分配待调度任务、生成集群状态快照。
  - scheduler/dispatch.go：调度器内部具体分配逻辑，包含扫描待分配任务、选择 worker、绑定任务、推进轮转起点。

  grpcapi

  - grpcapi/server.go：gRPC 服务实现，功能和 HTTP 层对应，但额外支持流式下发任务分配，并负责 proto 结构和内部模型之间的转换。
  - grpcapi/server_test.go：gRPC 服务的主流程测试，覆盖 worker 注册、任务提交、任务分配流、心跳、结果上报、查询总览这一整条链
    路。