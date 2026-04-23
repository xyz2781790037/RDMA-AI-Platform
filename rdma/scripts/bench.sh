#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

"${ROOT_DIR}/build/rdma_latency_bench"
"${ROOT_DIR}/build/rdma_throughput_bench"
"${ROOT_DIR}/build/rdma_multi_conn_bench"
