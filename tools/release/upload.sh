#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
PYTHON="${ARBITER_PYTHON:-$ROOT/build-release-python/bin/python}"
[[ -x "$PYTHON" ]] || PYTHON=python3.12
exec "$PYTHON" tools/release/publish.py "$@"
