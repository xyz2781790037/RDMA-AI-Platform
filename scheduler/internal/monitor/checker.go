package monitor

import (
	"context"
	"time"

	core "rdma-ai-platform/scheduler/internal/scheduler"
)

// Checker 定期刷新节点在线状态，并触发待调度任务重新分配。
type Checker struct {
	scheduler *core.Scheduler
	interval  time.Duration
}

// NewChecker 创建一个后台巡检器，并在未指定间隔时使用默认轮询周期。
func NewChecker(scheduler *core.Scheduler, interval time.Duration) *Checker {
	if interval <= 0 {
		interval = 5 * time.Second
	}

	return &Checker{
		scheduler: scheduler,
		interval:  interval,
	}
}

// Start 启动后台协程，按固定周期刷新节点状态并重新尝试调度。
func (c *Checker) Start(ctx context.Context) {
	go func() {
		ticker := time.NewTicker(c.interval)
		defer ticker.Stop()

		for {
			select {
			case <-ctx.Done():
				return
			case now := <-ticker.C:
				timestamp := now.UTC()
				_ = c.scheduler.RefreshNodeStates(timestamp)
				_ = c.scheduler.DispatchPending(timestamp)
			}
		}
	}()
}
