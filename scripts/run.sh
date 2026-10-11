#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="${OPENQUI_BINARY:-$PROJECT_ROOT/${OPENQUI_BUILD_DIRECTORY:-build/native}/open-cue-gui}"
if [[ ! -x "$BINARY" ]]; then "$PROJECT_ROOT/scripts/build.sh"; fi
exec "$BINARY" "$@"
