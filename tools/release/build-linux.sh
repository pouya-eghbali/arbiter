#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ "$(uname -s)" == Darwin && "${ARBITER_LINUX_DOCKER:-0}" != 1 ]]; then
  PYTHON="${ARBITER_PYTHON:-python3.12}"
  QT_VERSION="$("$PYTHON" -c 'import json,sys; print(json.load(open(sys.argv[1]))["qt"])' "$ROOT/engine/manifest.json")"
  HOST_QT="${ARBITER_QT_PREFIX:-$HOME/Qt/$QT_VERSION/macos}"
  LLVM_PREFIX="${ARBITER_LINUX_LLVM:-$(brew --prefix llvm@22)}"
  LLD_PREFIX="${ARBITER_LINUX_LLD:-$(brew --prefix lld@22)}"
  exec "$PYTHON" "$ROOT/tools/cross/linux.py" all --host-qt "$HOST_QT" \
    --llvm "$LLVM_PREFIX" --lld "$LLD_PREFIX" "$@"
fi
exec "${ARBITER_PYTHON:-python3}" "$ROOT/tools/release/build-linux.py" "$@"
