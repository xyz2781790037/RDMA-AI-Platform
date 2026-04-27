#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
SCHEDULER_BIND="${SCHEDULER_BIND:-:8080}"
SCHEDULER_URL="${SCHEDULER_URL:-http://127.0.0.1:8080}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --target worker_node >/dev/null

(
  cd "${ROOT_DIR}/scheduler"
  SCHEDULER_ADDR="${SCHEDULER_BIND}" go run .
) &
SCHEDULER_PID=$!

cleanup() {
  kill "${SCHEDULER_PID}" >/dev/null 2>&1 || true
}

trap cleanup EXIT
sleep 1

(
  cd "${ROOT_DIR}/scheduler"
  go run ./cmd/schedulerctl -addr "${SCHEDULER_URL}" -action register -worker worker-a -worker-addr 10.0.0.11:9000 -capacity 2
  go run ./cmd/schedulerctl -addr "${SCHEDULER_URL}" -action register -worker worker-b -worker-addr 10.0.0.12:9000 -capacity 2
  go run ./cmd/schedulerctl -addr "${SCHEDULER_URL}" -action submit -model llama2-13b -operation tensor-broadcast -payload rdma://tensor/demo-step
  go run ./cmd/schedulerctl -addr "${SCHEDULER_URL}" -action overview
)

"${BUILD_DIR}/worker_node"
