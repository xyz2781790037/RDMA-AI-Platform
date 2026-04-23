#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build"
cmake --build "${ROOT_DIR}/build"
