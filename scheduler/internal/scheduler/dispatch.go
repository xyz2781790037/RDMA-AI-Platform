package scheduler

import (
	"sort"
	"time"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/storage"
)

// dispatchPendingLocked 在持有 store 写锁时按创建顺序调度所有待分配任务。
func (s *Scheduler) dispatchPendingLocked(state *storage.State, now time.Time) {
	pendingTasks := make([]*model.Task, 0)
	for _, task := range state.Tasks {
		if task.Status == model.TaskPending {
			pendingTasks = append(pendingTasks, task)
		}
	}

	sort.Slice(pendingTasks, func(i, j int) bool {
		if pendingTasks[i].CreatedAt.Equal(pendingTasks[j].CreatedAt) {
			return pendingTasks[i].ID < pendingTasks[j].ID
		}
		return pendingTasks[i].CreatedAt.Before(pendingTasks[j].CreatedAt)
	})

	for _, task := range pendingTasks {
		if !s.assignTaskLocked(state, task, now) {
			return
		}
	}
}

// assignTaskLocked 为单个待调度任务选择 worker，并更新任务和节点的运行状态。
func (s *Scheduler) assignTaskLocked(state *storage.State, task *model.Task, now time.Time) bool {
	if task == nil || task.Status != model.TaskPending {
		return false
	}

	node := s.selectNodeLocked(state)
	if node == nil {
		return false
	}

	startedAt := now
	task.AssignedWorker = node.ID
	task.Status = model.TaskRunning
	task.Message = "assigned to worker"
	task.StartedAt = &startedAt
	node.RunningTasks++
	node.Status = model.DeriveNodeState(node, now, s.offlineAfter)

	return true
}

// selectNodeLocked 从可用 worker 中选择一个最合适的节点，并推进轮转起点。
func (s *Scheduler) selectNodeLocked(state *storage.State) *model.Node {
	if len(state.NodeOrder) == 0 {
		return nil
	}

	strategy := s.strategy
	if strategy == nil {
		strategy = LeastLoadedStrategy{}
	}

	start := 0
	if len(state.NodeOrder) > 0 {
		start = state.NextNodeIndex % len(state.NodeOrder)
	}

	bestIndex := -1
	var best *model.Node

	for offset := 0; offset < len(state.NodeOrder); offset++ {
		index := (start + offset) % len(state.NodeOrder)
		node := state.Nodes[state.NodeOrder[index]]
		if node == nil || node.Status == model.NodeOffline {
			continue
		}
		if node.Capacity > 0 && node.RunningTasks >= node.Capacity {
			continue
		}
		if best == nil || strategy.Compare(node, best) < 0 {
			best = node
			bestIndex = index
		}
	}

	if bestIndex == -1 {
		return nil
	}

	state.NextNodeIndex = (bestIndex + 1) % len(state.NodeOrder)
	return state.Nodes[state.NodeOrder[bestIndex]]
}
