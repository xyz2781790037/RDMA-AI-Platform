package scheduler

import (
	"sort"
	"time"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/storage"
)

// Scheduler 负责刷新节点状态、选择可用 worker 并分配待执行任务。
type Scheduler struct {
	// store 是调度器共享的内存状态存储。
	store *storage.MemoryStore
	// strategy 定义了多个候选 worker 之间的优先级比较逻辑。
	strategy Strategy
	// offlineAfter 是节点超过该时长未心跳时的离线阈值。
	offlineAfter time.Duration
}

// New 创建一个调度器实例，并注入存储、选点策略和离线判定配置。
func New(store *storage.MemoryStore, strategy Strategy, offlineAfter time.Duration) *Scheduler {
	return &Scheduler{
		store:        store,
		strategy:     strategy,
		offlineAfter: offlineAfter,
	}
}

// DispatchPending 刷新节点状态后，尝试把所有待调度任务分配给可用 worker。
func (s *Scheduler) DispatchPending(now time.Time) error {
	return s.store.Write(func(state *storage.State) error {
		s.refreshNodeStatesLocked(state, now)
		s.dispatchPendingLocked(state, now)
		return nil
	})
}

// RefreshNodeStates 仅刷新当前所有 worker 的在线状态，不执行任务分配。
func (s *Scheduler) RefreshNodeStates(now time.Time) error {
	return s.store.Write(func(state *storage.State) error {
		s.refreshNodeStatesLocked(state, now)
		return nil
	})
}

// Overview 生成一份包含节点和任务列表的调度器状态快照。
func (s *Scheduler) Overview(now time.Time) (model.StatusResponse, error) {
	var response model.StatusResponse

	err := s.store.Write(func(state *storage.State) error {
		s.refreshNodeStatesLocked(state, now)

		nodes := make([]model.Node, 0, len(state.Nodes))
		tasks := make([]model.Task, 0, len(state.Tasks))

		activeWorkers := 0
		pendingTasks := 0
		runningTasks := 0
		completedTasks := 0

		for _, node := range state.Nodes {
			if node.Status != model.NodeOffline {
				activeWorkers++
			}
			nodes = append(nodes, model.CloneNode(node))
		}

		for _, task := range state.Tasks {
			switch task.Status {
			case model.TaskPending:
				pendingTasks++
			case model.TaskRunning:
				runningTasks++
			case model.TaskCompleted:
				completedTasks++
			}
			tasks = append(tasks, model.CloneTask(task))
		}

		sort.Slice(nodes, func(i, j int) bool {
			return nodes[i].ID < nodes[j].ID
		})
		sort.Slice(tasks, func(i, j int) bool {
			if tasks[i].CreatedAt.Equal(tasks[j].CreatedAt) {
				return tasks[i].ID < tasks[j].ID
			}
			return tasks[i].CreatedAt.Before(tasks[j].CreatedAt)
		})

		response = model.StatusResponse{
			GeneratedAt:    now,
			TotalWorkers:   len(nodes),
			ActiveWorkers:  activeWorkers,
			PendingTasks:   pendingTasks,
			RunningTasks:   runningTasks,
			CompletedTasks: completedTasks,
			Workers:        nodes,
			Tasks:          tasks,
		}
		return nil
	})

	return response, err
}

// refreshNodeStatesLocked 在持有 store 写锁时批量刷新所有节点状态。
func (s *Scheduler) refreshNodeStatesLocked(state *storage.State, now time.Time) {
	for _, node := range state.Nodes {
		node.Status = model.DeriveNodeState(node, now, s.offlineAfter)
	}
}
