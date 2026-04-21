#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKER_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

mkdir -p "${WORKER_DIR}/build"
cd "${WORKER_DIR}/build"
cmake ..
make -j
./worker
