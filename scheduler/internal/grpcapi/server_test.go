package grpcapi

import (
	"context"
	"net"
	"testing"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/test/bufconn"

	"rdma-ai-platform/scheduler/internal/registry"
	core "rdma-ai-platform/scheduler/internal/scheduler"
	"rdma-ai-platform/scheduler/internal/service"
	"rdma-ai-platform/scheduler/internal/storage"
	schedulerv1 "rdma-ai-platform/scheduler/proto/gen/schedulerv1"
)

// TestSchedulerServiceLifecycle 覆盖 gRPC 调度服务的注册、分配、心跳、回报和查询主流程。
func TestSchedulerServiceLifecycle(t *testing.T) {
	store := storage.NewMemoryStore()
	dispatcher := core.New(store, core.LeastLoadedStrategy{}, 30*time.Second)
	nodeService := service.NewNodeService(registry.New(store, 30*time.Second), dispatcher)
	taskService := service.NewTaskService(store, dispatcher)

	listener := bufconn.Listen(1024 * 1024)
	server := NewServer(nodeService, taskService, store)
	defer server.Stop()

	go func() {
		_ = server.Serve(listener)
	}()
	defer listener.Close()

	client := newTestClient(t, listener)

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	registerResp, err := client.RegisterWorker(ctx, &schedulerv1.RegisterWorkerRequest{
		Worker: &schedulerv1.WorkerDescriptor{
			Id:       "worker-1",
			Address:  "127.0.0.1:9001",
			Capacity: 2,
			Labels:   []string{"gpu"},
		},
	})
	if err != nil {
		t.Fatalf("register worker: %v", err)
	}
	if got := registerResp.GetWorker().GetWorker().GetId(); got != "worker-1" {
		t.Fatalf("registered worker id = %q, want worker-1", got)
	}

	stream, err := client.WatchAssignments(ctx, &schedulerv1.WatchAssignmentsRequest{
		WorkerId:      "worker-1",
		ResumeUnacked: true,
	})
	if err != nil {
		t.Fatalf("watch assignments: %v", err)
	}

	submitResp, err := client.SubmitTask(ctx, &schedulerv1.SubmitTaskRequest{
		Spec: &schedulerv1.TaskSpec{
			Model:     "llama2-13b",
			Operation: "generate",
			Input: &schedulerv1.TaskInput{
				Source: &schedulerv1.TaskInput_Prompt{Prompt: "hello scheduler"},
			},
		},
	})
	if err != nil {
		t.Fatalf("submit task: %v", err)
	}

	assignment, err := stream.Recv()
	if err != nil {
		t.Fatalf("recv assignment: %v", err)
	}
	if assignment.GetTaskId() != submitResp.GetTask().GetId() {
		t.Fatalf("assignment task id = %q, want %q", assignment.GetTaskId(), submitResp.GetTask().GetId())
	}
	if got := assignment.GetSpec().GetInput().GetPrompt(); got != "hello scheduler" {
		t.Fatalf("assignment prompt = %q, want hello scheduler", got)
	}

	heartbeatResp, err := client.Heartbeat(ctx, &schedulerv1.HeartbeatRequest{
		WorkerId:      "worker-1",
		RunningTasks:  1,
		ReportedState: schedulerv1.NodeState_NODE_STATE_READY,
		ActiveTaskIds: []string{assignment.GetTaskId()},
	})
	if err != nil {
		t.Fatalf("heartbeat: %v", err)
	}
	if got := heartbeatResp.GetWorker().GetRunningTasks(); got != 1 {
		t.Fatalf("running tasks = %d, want 1", got)
	}

	reportResp, err := client.ReportTaskResult(ctx, &schedulerv1.ReportTaskResultRequest{
		Result: &schedulerv1.TaskResult{
			TaskId:  assignment.GetTaskId(),
			Success: true,
			Output:  "ok",
		},
	})
	if err != nil {
		t.Fatalf("report result: %v", err)
	}
	if got := reportResp.GetTask().GetState(); got != schedulerv1.TaskState_TASK_STATE_COMPLETED {
		t.Fatalf("task state after report = %s, want completed", got.String())
	}

	taskResp, err := client.GetTask(ctx, &schedulerv1.GetTaskRequest{TaskId: assignment.GetTaskId()})
	if err != nil {
		t.Fatalf("get task: %v", err)
	}
	if got := taskResp.GetTask().GetMessage(); got != "ok" {
		t.Fatalf("task message = %q, want ok", got)
	}

	overviewResp, err := client.GetClusterOverview(ctx, &schedulerv1.GetClusterOverviewRequest{})
	if err != nil {
		t.Fatalf("get overview: %v", err)
	}
	if got := overviewResp.GetOverview().GetCompletedTasks(); got != 1 {
		t.Fatalf("completed tasks = %d, want 1", got)
	}
}

// newTestClient 基于 bufconn 创建一个仅供测试使用的 gRPC 客户端。
func newTestClient(t *testing.T, listener *bufconn.Listener) schedulerv1.SchedulerServiceClient {
	t.Helper()

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.DialContext(
		ctx,
		"bufconn",
		grpc.WithContextDialer(func(context.Context, string) (net.Conn, error) {
			return listener.Dial()
		}),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		t.Fatalf("dial bufconn: %v", err)
	}

	t.Cleanup(func() {
		_ = conn.Close()
	})

	return schedulerv1.NewSchedulerServiceClient(conn)
}
