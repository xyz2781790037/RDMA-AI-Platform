package api

import (
	"net/http"

	"rdma-ai-platform/scheduler/internal/model"
)

// handleRegister 处理 worker 注册请求，并返回刷新后的节点状态。
func (h *Handler) handleRegister(w http.ResponseWriter, r *http.Request) {
	var req model.RegisterNodeRequest
	if err := decodeJSON(r, &req); err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	node, err := h.nodeService.Register(req)
	if err != nil {
		writeError(w, http.StatusBadRequest, err.Error())
		return
	}

	writeJSON(w, http.StatusCreated, node)
}
