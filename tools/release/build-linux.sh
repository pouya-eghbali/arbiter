#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
exec "${ARBITER_PYTHON:-python3}" "$ROOT/tools/release/build-linux.py" "$@"
