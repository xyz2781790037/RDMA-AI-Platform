package storage

import (
	"sync"

	"rdma-ai-platform/scheduler/internal/model"
)

// State 是调度器当前运行时的内存状态快照。
type State struct {
	// Nodes 以 worker ID 为键保存所有已注册节点。
	Nodes map[string]*model.Node
	// Tasks 以任务 ID 为键保存所有任务。
	Tasks map[string]*model.Task
	// NodeOrder 保存节点注册顺序，用于轮转选择 worker。
	NodeOrder []string
	// NextNodeIndex 记录下一次选点时优先开始扫描的位置。
	NextNodeIndex int
	// TaskCounter 是递增的任务编号计数器，用于生成任务 ID。
	TaskCounter uint64
}

// MemoryStore 是一个带读写锁保护的内存状态存储。
type MemoryStore struct {
	// mu 用于保护 state 的并发读写。
	mu sync.RWMutex
	// state 保存调度器共享的全部内存状态。
	state State
}

// NewMemoryStore 创建并初始化一个空的内存存储。
func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		state: State{
			Nodes: make(map[string]*model.Node),
			Tasks: make(map[string]*model.Task),
		},
	}
}

// Read 在读锁保护下执行回调，供调用方安全读取当前状态。
func (s *MemoryStore) Read(fn func(*State) error) error {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return fn(&s.state)
}

// Write 在写锁保护下执行回调，供调用方安全修改当前状态。
func (s *MemoryStore) Write(fn func(*State) error) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	return fn(&s.state)
}
