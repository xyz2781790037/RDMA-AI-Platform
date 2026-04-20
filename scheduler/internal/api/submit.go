package api

import (
	"net/http"

	"rdma-ai-platform/scheduler/internal/model"
)

// handleSubmitTask 处理任务提交请求，并返回最新任务状态。
func (h *Handler) handleSubmitTask(w http.ResponseWriter, r *http.Request) {
	var req model.SubmitTaskRequest
	if err := decodeJSON(r, &req); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	task, err := h.taskService.Submit(req)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusCreated, task)
}

// handleCompleteTask 处理 worker 回报任务完成结果的请求。
func (h *Handler) handleCompleteTask(w http.ResponseWriter, r *http.Request) {
	taskID := r.PathValue("id")

	var req model.CompleteTaskRequest
	if err := decodeJSON(r, &req); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	task, err := h.taskService.Complete(taskID, req)
	if err != nil {
		writeError(w, http.StatusNotFound, err.Error())
		return
	}

	writeJSON(w, http.StatusOK, task)
}
