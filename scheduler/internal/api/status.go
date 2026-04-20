package api

import (
	"net/http"

	"rdma-ai-platform/scheduler/internal/model"
)

// handleHealthz 返回服务健康状态。
func (h *Handler) handleHealthz(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, model.HealthResponse{
		Status:  "ok",
		Service: "rdma-ai-platform-scheduler",
	})
}

// handleGetTask 按任务 ID 查询当前任务详情。
func (h *Handler) handleGetTask(w http.ResponseWriter, r *http.Request) {
	taskID := r.PathValue("id")
	task, ok := h.taskService.Get(taskID)
	if !ok {
		writeError(w, http.StatusNotFound, "task not found")
		return
	}

	writeJSON(w, http.StatusOK, task)
}

// handleStatus 返回当前集群和任务的状态快照。
func (h *Handler) handleStatus(w http.ResponseWriter, _ *http.Request) {
	status, err := h.taskService.Status()
	if err != nil {
		writeError(w, http.StatusInternalServerError, err.Error())
		return
	}

	writeJSON(w, http.StatusOK, status)
}
