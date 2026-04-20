package grpcapi

import (
	"context"
	"errors"
	"fmt"
	"sort"
	"strings"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/types/known/timestamppb"

	"rdma-ai-platform/scheduler/internal/model"
	"rdma-ai-platform/scheduler/internal/service"
	"rdma-ai-platform/scheduler/internal/storage"
	schedulerv1 "rdma-ai-platform/scheduler/proto/gen/schedulerv1"
)

const (
	assignmentPollInterval = 200 * time.Millisecond
	inputSourceMetadataKey = "_scheduler_input_source"
	inputSourcePrompt      = "prompt"
	inputSourcePayloadRef  = "payload_ref"
	inputSourceInlineBytes = "inline_bytes"
)

// serviceServer 实现 gRPC 调度服务，并复用内部节点、任务与存储服务。
type serviceServer struct {
	schedulerv1.UnimplementedSchedulerServiceServer
	nodeService *service.NodeService
	taskService *service.TaskService
	store       *storage.MemoryStore
}

// NewServer 创建并注册 gRPC 调度服务。
func NewServer(nodeService *service.NodeService, taskService *service.TaskService, store *storage.MemoryStore) *grpc.Server {
	server := grpc.NewServer()
	schedulerv1.RegisterSchedulerServiceServer(server, &serviceServer{
		nodeService: nodeService,
		taskService: taskService,
		store:       store,
	})
	return server
}

// RegisterWorker 注册或刷新 worker 的静态信息。
func (s *serviceServer) RegisterWorker(_ context.Context, req *schedulerv1.RegisterWorkerRequest) (*schedulerv1.RegisterWorkerResponse, error) {
	worker := req.GetWorker()
	if worker == nil {
		return nil, status.Error(codes.InvalidArgument, "worker is required")
	}

	node, err := s.nodeService.Register(model.RegisterNodeRequest{
		ID:       worker.GetId(),
		Address:  worker.GetAddress(),
		Capacity: int(worker.GetCapacity()),
		Labels:   append([]string(nil), worker.GetLabels()...),
	})
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	return &schedulerv1.RegisterWorkerResponse{
		Worker:  toWorkerStatus(node),
		Message: "worker registered",
	}, nil
}

// Heartbeat 接收 worker 心跳，并返回刷新后的 worker 状态。
func (s *serviceServer) Heartbeat(_ context.Context, req *schedulerv1.HeartbeatRequest) (*schedulerv1.HeartbeatResponse, error) {
	node, err := s.nodeService.Heartbeat(model.HeartbeatRequest{
		ID:           req.GetWorkerId(),
		RunningTasks: int(req.GetRunningTasks()),
		Status:       fromProtoNodeState(req.GetReportedState()),
	})
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			return nil, status.Error(codes.NotFound, err.Error())
		}
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	return &schedulerv1.HeartbeatResponse{
		Worker:  toWorkerStatus(node),
		Message: "heartbeat accepted",
	}, nil
}

// WatchAssignments 以流式方式轮询并下发指定 worker 的新任务分配。
func (s *serviceServer) WatchAssignments(req *schedulerv1.WatchAssignmentsRequest, stream grpc.ServerStreamingServer[schedulerv1.TaskAssignment]) error {
	workerID := req.GetWorkerId()
	if workerID == "" {
		return status.Error(codes.InvalidArgument, "worker_id is required")
	}

	exists, err := s.workerExists(workerID)
	if err != nil {
		return status.Error(codes.Internal, err.Error())
	}
	if !exists {
		return status.Errorf(codes.NotFound, "worker %q not found", workerID)
	}

	sent := make(map[string]struct{})
	if !req.GetResumeUnacked() {
		assignments, err := s.runningAssignments(workerID)
		if err != nil {
			return status.Error(codes.Internal, err.Error())
		}
		for _, assignment := range assignments {
			sent[assignment.ID] = struct{}{}
		}
	}

	ticker := time.NewTicker(assignmentPollInterval)
	defer ticker.Stop()

	for {
		assignments, err := s.runningAssignments(workerID)
		if err != nil {
			return status.Error(codes.Internal, err.Error())
		}

		for _, assignment := range assignments {
			if _, ok := sent[assignment.ID]; ok {
				continue
			}
			if err := stream.Send(toTaskAssignment(assignment)); err != nil {
				return err
			}
			sent[assignment.ID] = struct{}{}
		}

		select {
		case <-stream.Context().Done():
			return nil
		case <-ticker.C:
		}
	}
}

// ReportTaskResult 接收 worker 上报的任务执行结果，并更新任务终态。
func (s *serviceServer) ReportTaskResult(_ context.Context, req *schedulerv1.ReportTaskResultRequest) (*schedulerv1.ReportTaskResultResponse, error) {
	result := req.GetResult()
	if result == nil {
		return nil, status.Error(codes.InvalidArgument, "result is required")
	}

	message := result.GetOutput()
	if !result.GetSuccess() && result.GetErrorMessage() != "" {
		message = result.GetErrorMessage()
	}

	task, err := s.taskService.Complete(result.GetTaskId(), model.CompleteTaskRequest{
		Success: result.GetSuccess(),
		Message: message,
	})
	if err != nil {
		if strings.Contains(err.Error(), "not found") {
			return nil, status.Error(codes.NotFound, err.Error())
		}
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	return &schedulerv1.ReportTaskResultResponse{
		Task:    toTaskInfo(task),
		Message: "task result accepted",
	}, nil
}

// SubmitTask 将 gRPC 请求转换为内部任务请求并提交到调度器。
func (s *serviceServer) SubmitTask(_ context.Context, req *schedulerv1.SubmitTaskRequest) (*schedulerv1.SubmitTaskResponse, error) {
	spec := req.GetSpec()
	if spec == nil {
		return nil, status.Error(codes.InvalidArgument, "spec is required")
	}

	modelReq := model.SubmitTaskRequest{
		Model:     spec.GetModel(),
		Operation: spec.GetOperation(),
		Metadata:  publicMetadata(spec.GetMetadata()),
	}

	if input := spec.GetInput(); input != nil {
		switch source := input.Source.(type) {
		case *schedulerv1.TaskInput_Prompt:
			modelReq.PayloadRef = source.Prompt
			modelReq.Metadata[inputSourceMetadataKey] = inputSourcePrompt
		case *schedulerv1.TaskInput_PayloadRef:
			modelReq.PayloadRef = source.PayloadRef
			modelReq.Metadata[inputSourceMetadataKey] = inputSourcePayloadRef
		case *schedulerv1.TaskInput_InlineBytes:
			modelReq.PayloadRef = string(source.InlineBytes)
			modelReq.Metadata[inputSourceMetadataKey] = inputSourceInlineBytes
		}
	}

	task, err := s.taskService.Submit(modelReq)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	return &schedulerv1.SubmitTaskResponse{Task: toTaskInfo(task)}, nil
}

// GetTask 查询单个任务的当前状态。
func (s *serviceServer) GetTask(_ context.Context, req *schedulerv1.GetTaskRequest) (*schedulerv1.GetTaskResponse, error) {
	task, ok := s.taskService.Get(req.GetTaskId())
	if !ok {
		return nil, status.Error(codes.NotFound, "task not found")
	}
	return &schedulerv1.GetTaskResponse{Task: toTaskInfo(task)}, nil
}

// GetClusterOverview 返回整个集群的运行状态快照。
func (s *serviceServer) GetClusterOverview(_ context.Context, _ *schedulerv1.GetClusterOverviewRequest) (*schedulerv1.GetClusterOverviewResponse, error) {
	overview, err := s.taskService.Status()
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	return &schedulerv1.GetClusterOverviewResponse{Overview: toClusterOverview(overview)}, nil
}

// workerExists 判断指定 worker 是否已经注册。
func (s *serviceServer) workerExists(workerID string) (bool, error) {
	exists := false
	err := s.store.Read(func(state *storage.State) error {
		_, exists = state.Nodes[workerID]
		return nil
	})
	return exists, err
}

// runningAssignments 返回某个 worker 当前正在执行的任务，并按开始时间排序。
func (s *serviceServer) runningAssignments(workerID string) ([]model.Task, error) {
	assignments := make([]model.Task, 0)
	err := s.store.Read(func(state *storage.State) error {
		for _, task := range state.Tasks {
			if task.AssignedWorker != workerID || task.Status != model.TaskRunning {
				continue
			}
			assignments = append(assignments, model.CloneTask(task))
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	sort.Slice(assignments, func(i, j int) bool {
		left := assignments[i]
		right := assignments[j]

		switch {
		case left.StartedAt == nil && right.StartedAt == nil:
			return left.ID < right.ID
		case left.StartedAt == nil:
			return true
		case right.StartedAt == nil:
			return false
		case left.StartedAt.Equal(*right.StartedAt):
			return left.ID < right.ID
		default:
			return left.StartedAt.Before(*right.StartedAt)
		}
	})

	return assignments, nil
}

// toWorkerStatus 将内部节点模型转换为 gRPC worker 状态。
func toWorkerStatus(node model.Node) *schedulerv1.WorkerStatus {
	return &schedulerv1.WorkerStatus{
		Worker: &schedulerv1.WorkerDescriptor{
			Id:       node.ID,
			Address:  node.Address,
			Capacity: uint32(node.Capacity),
			Labels:   append([]string(nil), node.Labels...),
		},
		RunningTasks:  uint32(node.RunningTasks),
		State:         toProtoNodeState(node.Status),
		LastHeartbeat: timestampOrNil(node.LastHeartbeat),
	}
}

// toTaskAssignment 将内部任务模型转换为流式下发的任务分配消息。
func toTaskAssignment(task model.Task) *schedulerv1.TaskAssignment {
	return &schedulerv1.TaskAssignment{
		TaskId:     task.ID,
		Spec:       toTaskSpec(task),
		AssignedAt: timestampPointerOrNil(task.StartedAt),
	}
}

// toTaskInfo 将内部任务模型转换为对外返回的任务详情。
func toTaskInfo(task model.Task) *schedulerv1.TaskInfo {
	return &schedulerv1.TaskInfo{
		Id:             task.ID,
		Spec:           toTaskSpec(task),
		State:          toProtoTaskState(task.Status),
		AssignedWorker: task.AssignedWorker,
		Message:        task.Message,
		CreatedAt:      timestampOrNil(task.CreatedAt),
		StartedAt:      timestampPointerOrNil(task.StartedAt),
		CompletedAt:    timestampPointerOrNil(task.CompletedAt),
	}
}

// toTaskSpec 将内部任务模型还原为 gRPC 任务规格。
func toTaskSpec(task model.Task) *schedulerv1.TaskSpec {
	metadata := publicMetadata(task.Metadata)
	input := &schedulerv1.TaskInput{}

	switch task.Metadata[inputSourceMetadataKey] {
	case inputSourcePrompt:
		input.Source = &schedulerv1.TaskInput_Prompt{Prompt: task.PayloadRef}
	case inputSourceInlineBytes:
		input.Source = &schedulerv1.TaskInput_InlineBytes{InlineBytes: []byte(task.PayloadRef)}
	default:
		input.Source = &schedulerv1.TaskInput_PayloadRef{PayloadRef: task.PayloadRef}
	}

	return &schedulerv1.TaskSpec{
		Model:     task.Model,
		Operation: task.Operation,
		Input:     input,
		Metadata:  metadata,
	}
}

// toClusterOverview 将内部总览快照转换为 gRPC 响应结构。
func toClusterOverview(overview model.StatusResponse) *schedulerv1.ClusterOverview {
	workers := make([]*schedulerv1.WorkerStatus, 0, len(overview.Workers))
	for _, worker := range overview.Workers {
		workers = append(workers, toWorkerStatus(worker))
	}

	tasks := make([]*schedulerv1.TaskInfo, 0, len(overview.Tasks))
	for _, task := range overview.Tasks {
		tasks = append(tasks, toTaskInfo(task))
	}

	return &schedulerv1.ClusterOverview{
		GeneratedAt:    timestampOrNil(overview.GeneratedAt),
		Workers:        workers,
		Tasks:          tasks,
		TotalWorkers:   uint32(overview.TotalWorkers),
		ActiveWorkers:  uint32(overview.ActiveWorkers),
		PendingTasks:   uint32(overview.PendingTasks),
		RunningTasks:   uint32(overview.RunningTasks),
		CompletedTasks: uint32(overview.CompletedTasks),
	}
}

// toProtoNodeState 将内部节点状态映射为 gRPC 枚举。
func toProtoNodeState(state model.NodeState) schedulerv1.NodeState {
	switch state {
	case model.NodeReady:
		return schedulerv1.NodeState_NODE_STATE_READY
	case model.NodeBusy:
		return schedulerv1.NodeState_NODE_STATE_BUSY
	case model.NodeOffline:
		return schedulerv1.NodeState_NODE_STATE_OFFLINE
	default:
		return schedulerv1.NodeState_NODE_STATE_UNSPECIFIED
	}
}

// fromProtoNodeState 将 gRPC 节点状态映射为内部枚举。
func fromProtoNodeState(state schedulerv1.NodeState) model.NodeState {
	switch state {
	case schedulerv1.NodeState_NODE_STATE_BUSY:
		return model.NodeBusy
	case schedulerv1.NodeState_NODE_STATE_OFFLINE:
		return model.NodeOffline
	case schedulerv1.NodeState_NODE_STATE_READY:
		return model.NodeReady
	default:
		return model.NodeReady
	}
}

// toProtoTaskState 将内部任务状态映射为 gRPC 枚举。
func toProtoTaskState(state model.TaskState) schedulerv1.TaskState {
	switch state {
	case model.TaskPending:
		return schedulerv1.TaskState_TASK_STATE_PENDING
	case model.TaskRunning:
		return schedulerv1.TaskState_TASK_STATE_RUNNING
	case model.TaskCompleted:
		return schedulerv1.TaskState_TASK_STATE_COMPLETED
	case model.TaskFailed:
		return schedulerv1.TaskState_TASK_STATE_FAILED
	default:
		return schedulerv1.TaskState_TASK_STATE_UNSPECIFIED
	}
}

// timestampOrNil 在零值时间上返回 nil，避免生成无意义的 protobuf 时间戳。
func timestampOrNil(value time.Time) *timestamppb.Timestamp {
	if value.IsZero() {
		return nil
	}
	return timestamppb.New(value)
}

// timestampPointerOrNil 将可选时间指针转换为 protobuf 时间戳。
func timestampPointerOrNil(value *time.Time) *timestamppb.Timestamp {
	if value == nil || value.IsZero() {
		return nil
	}
	return timestamppb.New(*value)
}

// publicMetadata 过滤内部保留字段，仅保留可对外暴露的元数据。
func publicMetadata(input map[string]string) map[string]string {
	if len(input) == 0 {
		return map[string]string{}
	}

	output := make(map[string]string, len(input))
	for key, value := range input {
		if key == inputSourceMetadataKey {
			continue
		}
		output[key] = value
	}
	return output
}

// translateInternalError 将内部错误统一映射为 gRPC 状态错误。
func translateInternalError(err error) error {
	if err == nil {
		return nil
	}
	if errors.Is(err, context.Canceled) {
		return status.Error(codes.Canceled, err.Error())
	}
	return status.Error(codes.Internal, fmt.Sprintf("internal error: %v", err))
}
