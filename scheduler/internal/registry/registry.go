package registry

import (
	"errors"
	"fmt"
	"time"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/storage"
)

// Registry 负责维护已注册 worker 的信息、心跳状态和离线判定。
type Registry struct {
	// store 是底层内存存储，保存所有节点状态。
	store *storage.MemoryStore
	// offlineAfter 是节点在多长时间未上报心跳后会被视为离线。
	offlineAfter time.Duration
}

// New 创建一个节点注册表实例，用于管理 worker 注册和心跳信息。
func New(store *storage.MemoryStore, offlineAfter time.Duration) *Registry {
	return &Registry{
		store:        store,
		offlineAfter: offlineAfter,
	}
}

// Register 注册新 worker，或更新已有 worker 的静态信息并刷新其状态。
func (r *Registry) Register(req model.RegisterNodeRequest, now time.Time) (model.Node, error) {
	if req.ID == "" {
		return model.Node{}, errors.New("worker id is required")
	}
	if req.Capacity <= 0 {
		req.Capacity = 1
	}

	var result model.Node
	err := r.store.Write(func(state *storage.State) error {
		node, exists := state.Nodes[req.ID]
		if !exists {
			node = &model.Node{ID: req.ID}
			state.Nodes[req.ID] = node
			state.NodeOrder = append(state.NodeOrder, req.ID)
		}

		node.Address = req.Address
		node.Capacity = req.Capacity
		node.Labels = append([]string(nil), req.Labels...)
		node.LastHeartbeat = now
		node.Status = model.DeriveNodeState(node, now, r.offlineAfter)
		result = model.CloneNode(node)
		return nil
	})

	return result, err
}

// Heartbeat 更新指定 worker 的运行中任务数、最近心跳时间和推导状态。
func (r *Registry) Heartbeat(req model.HeartbeatRequest, now time.Time) (model.Node, error) {
	if req.ID == "" {
		return model.Node{}, errors.New("worker id is required")
	}

	var result model.Node
	err := r.store.Write(func(state *storage.State) error {
		node, exists := state.Nodes[req.ID]
		if !exists {
			return fmt.Errorf("worker %q not found", req.ID)
		}

		if req.RunningTasks < 0 {
			req.RunningTasks = 0
		}

		node.RunningTasks = req.RunningTasks
		node.LastHeartbeat = now
		node.Status = model.DeriveNodeState(node, now, r.offlineAfter)
		result = model.CloneNode(node)
		return nil
	})

	return result, err
}

// Get 按 worker ID 查询当前节点快照，不存在时返回 false。
func (r *Registry) Get(id string) (model.Node, bool) {
	var (
		result model.Node
		found  bool
	)

	_ = r.store.Read(func(state *storage.State) error {
		node, exists := state.Nodes[id]
		if !exists {
			return nil
		}

		result = model.CloneNode(node)
		found = true
		return nil
	})

	return result, found
}

// Refresh 遍历所有已注册 worker，并按当前时间重新计算它们的在线状态。
func (r *Registry) Refresh(now time.Time) error {
	return r.store.Write(func(state *storage.State) error {
		for _, node := range state.Nodes {
			node.Status = model.DeriveNodeState(node, now, r.offlineAfter)
		}
		return nil
	})
}
