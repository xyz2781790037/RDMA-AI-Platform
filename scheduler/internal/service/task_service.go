package service

import (
	"errors"
	"fmt"
	"time"

	"rdma-ai-platform/scheduler/internal/model"
	core "rdma-ai-platform/scheduler/internal/scheduler"
	"rdma-ai-platform/scheduler/internal/storage"
	"rdma-ai-platform/scheduler/internal/util"
)

// TaskService 负责任务的创建、查询、完结以及触发调度。
type TaskService struct {
	store     *storage.MemoryStore
	scheduler *core.Scheduler
}

// NewTaskService 创建一个任务服务实例。
func NewTaskService(store *storage.MemoryStore, scheduler *core.Scheduler) *TaskService {
	return &TaskService{
		store:     store,
		scheduler: scheduler,
	}
}

// Submit 校验并创建新任务，然后尝试立即触发一次调度。
func (s *TaskService) Submit(req model.SubmitTaskRequest) (model.Task, error) {
	if req.Model == "" {
		return model.Task{}, errors.New("model is required")
	}
	if req.Operation == "" {
		return model.Task{}, errors.New("operation is required")
	}

	now := time.Now().UTC()
	taskID := ""

	err := s.store.Write(func(state *storage.State) error {
		state.TaskCounter++
		taskID = util.FormatTaskID(state.TaskCounter)
		state.Tasks[taskID] = &model.Task{
			ID:         taskID,
			Model:      req.Model,
			Operation:  req.Operation,
			PayloadRef: req.PayloadRef,
			Metadata:   model.CloneMetadata(req.Metadata),
			Status:     model.TaskPending,
			Message:    "queued",
			CreatedAt:  now,
		}
		return nil
	})
	if err != nil {
		return model.Task{}, err
	}

	if err := s.scheduler.DispatchPending(now); err != nil {
		return model.Task{}, err
	}

	task, ok := s.Get(taskID)
	if !ok {
		return model.Task{}, fmt.Errorf("task %q not found", taskID)
	}
	return task, nil
}

// Complete 标记任务最终结果，并在需要时释放对应 worker 的运行中计数。
func (s *TaskService) Complete(taskID string, req model.CompleteTaskRequest) (model.Task, error) {
	if taskID == "" {
		return model.Task{}, errors.New("task id is required")
	}

	now := time.Now().UTC()

	err := s.store.Write(func(state *storage.State) error {
		task, exists := state.Tasks[taskID]
		if !exists {
			return fmt.Errorf("task %q not found", taskID)
		}

		wasRunning := task.Status == model.TaskRunning
		completedAt := now
		task.CompletedAt = &completedAt
		task.Message = req.Message

		if req.Success {
			task.Status = model.TaskCompleted
			if task.Message == "" {
				task.Message = "completed"
			}
		} else {
			task.Status = model.TaskFailed
			if task.Message == "" {
				task.Message = "failed"
			}
		}

		if wasRunning && task.AssignedWorker != "" {
			node, ok := state.Nodes[task.AssignedWorker]
			if ok && node.RunningTasks > 0 {
				node.RunningTasks--
			}
		}

		return nil
	})
	if err != nil {
		return model.Task{}, err
	}

	if err := s.scheduler.DispatchPending(now); err != nil {
		return model.Task{}, err
	}

	task, ok := s.Get(taskID)
	if !ok {
		return model.Task{}, fmt.Errorf("task %q not found", taskID)
	}
	return task, nil
}

// Get 返回指定任务的当前快照，不存在时返回 false。
func (s *TaskService) Get(taskID string) (model.Task, bool) {
	var (
		result model.Task
		found  bool
	)

	_ = s.store.Read(func(state *storage.State) error {
		task, exists := state.Tasks[taskID]
		if !exists {
			return nil
		}

		result = model.CloneTask(task)
		found = true
		return nil
	})

	return result, found
}

// Status 返回调度器当前的总体状态快照。
func (s *TaskService) Status() (model.StatusResponse, error) {
	return s.scheduler.Overview(time.Now().UTC())
}
