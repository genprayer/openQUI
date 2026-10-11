#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
"$PROJECT_ROOT/scripts/build.sh"
BUILD_DIRECTORY="${OPENQUI_BUILD_DIRECTORY:-build/native}"
if command -v ctest >/dev/null; then
    ctest --test-dir "$PROJECT_ROOT/$BUILD_DIRECTORY" --output-on-failure
else
    BUILD_CONTAINER="${OPENQUI_BUILD_CONTAINER:-open-cue-gui-build}"
    if [[ "$(docker inspect -f '{{index .Config.Labels "org.open-cue-gui.purpose"}}' "$BUILD_CONTAINER" 2>/dev/null || true)" == "native-build" ]]; then
        docker exec --user "$(id -u):$(id -g)" "$BUILD_CONTAINER" ctest --test-dir "$BUILD_DIRECTORY" --output-on-failure
    else
        BUILD_IMAGE="${OPENQUI_BUILD_IMAGE:-open-cue-gui-build:fedora44}"
        docker run --rm --user "$(id -u):$(id -g)" --mount "type=bind,src=$PROJECT_ROOT,dst=/workspace" "$BUILD_IMAGE" ctest --test-dir "$BUILD_DIRECTORY" --output-on-failure
    fi
fi
if command -v pipewire >/dev/null && command -v pw-dump >/dev/null && command -v jq >/dev/null; then
    bash "$PROJECT_ROOT/tests/native/test_pipewire.sh" "$PROJECT_ROOT/$BUILD_DIRECTORY/open-cue-gui"
fi
