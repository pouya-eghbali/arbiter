#!/usr/bin/env bash
set -euo pipefail
# Never trace the local credentials file or signing commands.
set +x
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
[[ "$(uname -s)" == Darwin ]] || { echo 'Run this script on macOS.' >&2; exit 1; }
cd "$ROOT"
if [[ -f env.sh ]]; then
  set -a
  source ./env.sh
  set +x
  set +a
fi
export ARBITER_MACOS_SIGNING_IDENTITY="${ARBITER_MACOS_SIGNING_IDENTITY:-${APPLE_DEVELOPER_ID:-}}"
: "${ARBITER_MACOS_SIGNING_IDENTITY:?Set APPLE_DEVELOPER_ID or ARBITER_MACOS_SIGNING_IDENTITY}"
PYTHON="${ARBITER_PYTHON:-$ROOT/build-release-python/bin/python}"
[[ -x "$PYTHON" ]] || PYTHON=python3.12
if [[ -z "${ARBITER_NOTARY_PROFILE:-}" ]]; then
  : "${APPLE_ID:?Set APPLE_ID or ARBITER_NOTARY_PROFILE}"
  : "${APPLE_APP_PASSWORD:?Set APPLE_APP_PASSWORD or ARBITER_NOTARY_PROFILE}"
  : "${APPLE_TEAM_ID:?Set APPLE_TEAM_ID or ARBITER_NOTARY_PROFILE}"
  export ARBITER_NOTARY_PROFILE=arbiter-release
  # Run without command logging; notarytool saves credentials in the login keychain.
  "$PYTHON" "$ROOT/tools/release/notary-credentials.py"
fi
exec "$PYTHON" tools/release/build.py --package "$@"
