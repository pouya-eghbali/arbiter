#!/usr/bin/env bash
set -euo pipefail
export HOME=/work/home CARGO_HOME=/work/cargo
mkdir -p "$HOME" "$CARGO_HOME"
# A bundle is not a git directory; read its advertised HEAD without touching host files.
sha="$(git bundle list-heads /source.bundle HEAD | cut -d' ' -f1)"
[[ "$sha" =~ ^[a-f0-9]{40}$ ]] || { echo 'Invalid source bundle' >&2; exit 1; }
checkout="/work/checkouts/$sha"
if [[ ! -d "$checkout/.git" ]]; then
  mkdir -p /work/checkouts
  git clone /source.bundle "$checkout"
fi
cd "$checkout"
git checkout --detach "$sha"
export ARBITER_QT_PREFIX="${ARBITER_QT_PREFIX:-/opt/qt/$(python -c 'import json; print(json.load(open("engine/manifest.json"))["qt"])')/gcc_64}"
export ARBITER_ENGINE_CACHE=/work/engine
python tools/release/build.py --package "$@"
find build-release-artifacts -maxdepth 1 -type f \( -name '*.tar.xz' -o -name '*.sha256' \) -exec cp '{}' /artifacts/ \;
