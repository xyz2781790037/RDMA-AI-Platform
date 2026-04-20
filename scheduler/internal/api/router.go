package api

import (
	"encoding/json"
	"errors"
	"io"
	"net/http"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/service"
)

type Handler struct {
	nodeService *service.NodeService
	taskService *service.TaskService
}

// NewRouter 构建调度器 HTTP 路由，并注册节点和任务相关接口。
func NewRouter(nodeService *service.NodeService, taskService *service.TaskService) http.Handler {
	handler := &Handler{
		nodeService: nodeService,
		taskService: taskService,
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", handler.handleHealthz)
	mux.HandleFunc("POST /workers/register", handler.handleRegister)
	mux.HandleFunc("POST /nodes/register", handler.handleRegister)
	mux.HandleFunc("POST /workers/heartbeat", handler.handleHeartbeat)
	mux.HandleFunc("POST /nodes/heartbeat", handler.handleHeartbeat)
	mux.HandleFunc("POST /tasks/submit", handler.handleSubmitTask)
	mux.HandleFunc("POST /tasks/{id}/complete", handler.handleCompleteTask)
	mux.HandleFunc("GET /tasks/{id}", handler.handleGetTask)
	mux.HandleFunc("GET /cluster/overview", handler.handleStatus)
	mux.HandleFunc("GET /status", handler.handleStatus)

	return mux
}

// decodeJSON 将请求体解码到目标结构，并拒绝未知字段或多余 JSON 对象。
func decodeJSON(r *http.Request, target any) error {
	defer r.Body.Close()

	decoder := json.NewDecoder(r.Body)
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(target); err != nil {
		return err
	}
	if err := decoder.Decode(&struct{}{}); !errors.Is(err, io.EOF) {
		return errors.New("request body must contain a single JSON object")
	}
	return nil
}

// writeJSON 以统一的 JSON 格式写出响应体。
func writeJSON(w http.ResponseWriter, status int, payload any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(payload)
}

// writeError 以统一错误结构返回接口错误。
func writeError(w http.ResponseWriter, status int, message string) {
	writeJSON(w, status, model.ErrorResponse{Error: message})
}
