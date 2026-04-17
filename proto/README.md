# Proto Contracts

This directory defines the control-plane gRPC contracts shared by the Go scheduler and the C++ worker.

## Files

- `common.proto`: shared enums and domain models.
- `scheduler.proto`: scheduler service RPCs for worker lifecycle, task submission, result reporting, and assignment streaming.

## Communication Model

1. Worker calls `RegisterWorker`.
2. Worker sends periodic `Heartbeat`.
3. Worker opens `WatchAssignments` and keeps the stream alive.
4. Scheduler dispatches `TaskAssignment` messages over the stream.
5. Worker executes the task and calls `ReportTaskResult`.
6. External clients can call `SubmitTask`, `GetTask`, and `GetClusterOverview`.

`TaskInput` supports both:

- `prompt`: current fake-model execution path.
- `payload_ref`: future external payload or RDMA-resident input reference.

This keeps the current worker runnable without blocking the later RDMA transport transition.

## Suggested Code Generation

Generate Go stubs from the repository root:

```bash
protoc -I ./proto \
  --go_out=./scheduler \
  --go-grpc_out=./scheduler \
  ./proto/common.proto \
  ./proto/scheduler.proto
```

Generate C++ stubs from the repository root:

```bash
protoc -I ./proto \
  --cpp_out=./worker/generated \
  --grpc_out=./worker/generated \
  --plugin=protoc-gen-grpc="$(which grpc_cpp_plugin)" \
  ./proto/common.proto \
  ./proto/scheduler.proto
```

If generated code is later committed into the repository, keep it outside this directory so the source proto contracts remain the single source of truth.
