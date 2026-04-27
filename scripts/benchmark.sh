#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" --target latency_test throughput_test tcp_compare >/dev/null

"${BUILD_DIR}/latency_test"
"${BUILD_DIR}/throughput_test"
"${BUILD_DIR}/tcp_compare"
