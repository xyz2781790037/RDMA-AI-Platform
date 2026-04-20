package model

import "time"

// HealthResponse 是健康检查接口返回的响应体。
type HealthResponse struct {
	// Status 表示服务当前是否健康。
	Status string `json:"status"`
	// Service 是返回该响应的服务名称。
	Service string `json:"service"`
}

// ErrorResponse 是接口统一返回的错误信息结构。
type ErrorResponse struct {
	// Error 保存可直接返回给调用方的错误描述。
	Error string `json:"error"`
}

// StatusResponse 是调度器状态总览接口返回的快照数据。
type StatusResponse struct {
	// GeneratedAt 是本次状态快照生成的时间。
	GeneratedAt time.Time `json:"generated_at"`
	// TotalWorkers 是当前已注册的 worker 总数。
	TotalWorkers int `json:"total_workers"`
	// ActiveWorkers 是当前未离线的 worker 数量。
	ActiveWorkers int `json:"active_workers"`
	// PendingTasks 是等待调度的任务数量。
	PendingTasks int `json:"pending_tasks"`
	// RunningTasks 是正在执行中的任务数量。
	RunningTasks int `json:"running_tasks"`
	// CompletedTasks 是已成功完成的任务数量。
	CompletedTasks int `json:"completed_tasks"`
	// Workers 是当前所有 worker 的状态列表。
	Workers []Node `json:"workers"`
	// Tasks 是当前所有任务的状态列表。
	Tasks []Task `json:"tasks"`
}
