package model

import "time"

// TaskState 表示任务在调度生命周期中的状态。
type TaskState string

const (
	// TaskPending 表示任务已入队，等待调度器分配给 worker。
	TaskPending TaskState = "pending"
	// TaskRunning 表示任务已经分配给 worker，正在执行。
	TaskRunning TaskState = "running"
	// TaskCompleted 表示任务已成功完成。
	TaskCompleted TaskState = "completed"
	// TaskFailed 表示任务执行失败或被显式标记为失败。
	TaskFailed TaskState = "failed"
)

// Task 是调度器内部保存和对外返回的任务模型。
type Task struct {
	// ID 是调度器生成的唯一任务标识。
	ID string `json:"id"`
	// Model 是任务要使用的模型名称。
	Model string `json:"model"`
	// Operation 描述任务要执行的具体操作。
	Operation string `json:"operation"`
	// PayloadRef 指向任务输入数据或载荷的引用位置。
	PayloadRef string `json:"payload_ref"`
	// Metadata 保存额外的业务标签或调度上下文。
	Metadata map[string]string `json:"metadata"`
	// Status 记录任务当前所处的状态。
	Status TaskState `json:"status"`
	// AssignedWorker 是当前负责执行该任务的 worker ID。
	AssignedWorker string `json:"assigned_worker"`
	// Message 保存任务当前状态说明或完成结果信息。
	Message string `json:"message"`
	// CreatedAt 是任务进入调度器的时间。
	CreatedAt time.Time `json:"created_at"`
	// StartedAt 是任务真正开始执行的时间，为空表示尚未开始。
	StartedAt *time.Time `json:"started_at,omitempty"`
	// CompletedAt 是任务完成或失败的时间，为空表示尚未结束。
	CompletedAt *time.Time `json:"completed_at,omitempty"`
}

// SubmitTaskRequest 是创建任务接口接收的请求体。
type SubmitTaskRequest struct {
	// Model 指定本次任务要运行的模型。
	Model string `json:"model"`
	// Operation 指定对模型执行的操作类型。
	Operation string `json:"operation"`
	// PayloadRef 指向待处理输入数据的引用位置。
	PayloadRef string `json:"payload_ref"`
	// Metadata 传递提交任务时附带的额外信息。
	Metadata map[string]string `json:"metadata"`
}

// CompleteTaskRequest 是 worker 回报任务完成结果时使用的请求体。
type CompleteTaskRequest struct {
	// Success 表示任务是否成功完成。
	Success bool `json:"success"`
	// Message 保存结果说明、错误原因或附加输出。
	Message string `json:"message"`
}

// CloneTask 返回任务的值拷贝，并复制其中的 Metadata，避免共享底层 map。
func CloneTask(task *Task) Task {
	if task == nil {
		return Task{}
	}

	clone := *task
	clone.Metadata = CloneMetadata(task.Metadata)
	return clone
}

// CloneMetadata 复制任务元数据，避免调用方修改原始 map。
func CloneMetadata(input map[string]string) map[string]string {
	if len(input) == 0 {
		return nil
	}

	output := make(map[string]string, len(input))
	for key, value := range input {
		output[key] = value
	}
	return output
}
