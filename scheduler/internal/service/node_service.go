package service

import (
	"fmt"
	"time"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/registry"
	core "rdma-ai-platform/scheduler/internal/scheduler"
)

// NodeService 负责 worker 的注册、心跳处理以及触发调度。
type NodeService struct {
	registry  *registry.Registry
	scheduler *core.Scheduler
}

// NewNodeService 创建一个节点服务实例。
func NewNodeService(registry *registry.Registry, scheduler *core.Scheduler) *NodeService {
	return &NodeService{
		registry:  registry,
		scheduler: scheduler,
	}
}

// Register 注册或更新 worker 信息，并在节点可用时触发待调度任务分配。
func (s *NodeService) Register(req model.RegisterNodeRequest) (model.Node, error) {
	now := time.Now().UTC()
	if _, err := s.registry.Register(req, now); err != nil {
		return model.Node{}, err
	}
	if err := s.scheduler.DispatchPending(now); err != nil {
		return model.Node{}, err
	}

	node, ok := s.registry.Get(req.ID)
	if !ok {
		return model.Node{}, fmt.Errorf("worker %q not found", req.ID)
	}
	return node, nil
}

// Heartbeat 更新 worker 运行状态，并在状态刷新后尝试继续调度。
func (s *NodeService) Heartbeat(req model.HeartbeatRequest) (model.Node, error) {
	now := time.Now().UTC()
	if _, err := s.registry.Heartbeat(req, now); err != nil {
		return model.Node{}, err
	}
	if err := s.scheduler.DispatchPending(now); err != nil {
		return model.Node{}, err
	}

	node, ok := s.registry.Get(req.ID)
	if !ok {
		return model.Node{}, fmt.Errorf("worker %q not found", req.ID)
	}
	return node, nil
}
