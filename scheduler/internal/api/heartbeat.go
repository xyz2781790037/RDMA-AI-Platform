package api

import (
	"net/http"

	"rdma-ai-platform/scheduler/internal/model"
)

// handleHeartbeat 处理 worker 心跳上报，并返回最新节点状态。
func (h *Handler) handleHeartbeat(w http.ResponseWriter, r *http.Request) {
	var req model.HeartbeatRequest
	if err := decodeJSON(r, &req); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	node, err := h.nodeService.Heartbeat(req)
	if err != nil {
		writeError(w, http.StatusNotFound, err.Error())
		return
	}

	writeJSON(w, http.StatusOK, node)
}
