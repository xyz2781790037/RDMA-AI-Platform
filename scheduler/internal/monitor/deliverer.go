package monitor

import (
	"context"
	"net"
	"sort"
	"time"

	schedulerlogger "rdma-ai-platform/scheduler/Logger"
	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/storage"
)

// AssignmentDeliverer 定期扫描运行中的任务，并把分配结果投递给对应 worker。
type AssignmentDeliverer struct {
	store       *storage.MemoryStore
	interval    time.Duration
	dialTimeout time.Duration
}

// assignment 是一次待投递任务分配的内部表示。
type assignment struct {
	TaskID  string
	Payload string
	Address string
}

// NewAssignmentDeliverer 创建一个任务投递器，并在未指定间隔时使用默认轮询周期。
func NewAssignmentDeliverer(store *storage.MemoryStore, interval time.Duration) *AssignmentDeliverer {
	if interval <= 0 {
		interval = time.Second
	}

	return &AssignmentDeliverer{
		store:       store,
		interval:    interval,
		dialTimeout: time.Second,
	}
}

// Start 启动后台轮询协程，持续尝试把运行中的任务通知给对应 worker。
func (d *AssignmentDeliverer) Start(ctx context.Context) {
	go func() {
		d.dispatchOnce()

		ticker := time.NewTicker(d.interval)
		defer ticker.Stop()

		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				d.dispatchOnce()
			}
		}
	}()
}

// dispatchOnce 执行一次投递周期，并记录快照或发送过程中的失败。
func (d *AssignmentDeliverer) dispatchOnce() {
	assignments, err := d.snapshotAssignments()
	if err != nil {
		schedulerlogger.Log.Error("snapshot assignments failed", "err", err)
		return
	}

	for _, item := range assignments {
		if err := d.deliver(item); err != nil {
			schedulerlogger.Log.Warn("deliver assignment failed", "task_id", item.TaskID, "addr", item.Address, "err", err)
		}
	}
}

// snapshotAssignments 收集当前可投递的任务分配，并按地址和任务 ID 排序。
func (d *AssignmentDeliverer) snapshotAssignments() ([]assignment, error) {
	assignments := make([]assignment, 0)
	err := d.store.Read(func(state *storage.State) error {
		for _, task := range state.Tasks {
			if task.Status != model.TaskRunning || task.AssignedWorker == "" {
				continue
			}

			node := state.Nodes[task.AssignedWorker]
			if node == nil || node.Address == "" {
				continue
			}

			assignments = append(assignments, assignment{
				TaskID:  task.ID,
				Payload: task.PayloadRef,
				Address: node.Address,
			})
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	sort.Slice(assignments, func(i, j int) bool {
		if assignments[i].Address == assignments[j].Address {
			return assignments[i].TaskID < assignments[j].TaskID
		}
		return assignments[i].Address < assignments[j].Address
	})

	return assignments, nil
}

// deliver 通过 TCP 连接把任务 ID 和载荷发送给指定 worker。
func (d *AssignmentDeliverer) deliver(item assignment) error {
	conn, err := net.DialTimeout("tcp", item.Address, d.dialTimeout)
	if err != nil {
		return err
	}
	defer conn.Close()

	_ = conn.SetDeadline(time.Now().Add(d.dialTimeout))
	_, err = conn.Write([]byte(item.TaskID + "|" + item.Payload + "\n"))
	return err
}
