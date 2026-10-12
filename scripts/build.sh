#!/usr/bin/env bash
set -euo pipefail
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIRECTORY="${OPENQUI_BUILD_DIRECTORY:-build/native}"
BUILD_JOBS="${OPENQUI_BUILD_JOBS:-4}"
LOCAL_RESOURCES="${OPENQUI_USE_LOCAL_CORSAIR_RESOURCES:-AUTO}"
if command -v cmake >/dev/null && pkg-config --atleast-version=6.8 Qt6Quick && pkg-config --exists alsa libudev; then
    cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/$BUILD_DIRECTORY" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENQUI_USE_LOCAL_CORSAIR_RESOURCES="$LOCAL_RESOURCES"
    cmake --build "$PROJECT_ROOT/$BUILD_DIRECTORY" -j "$BUILD_JOBS"
    exit 0
fi
BUILD_CONTAINER="${OPENQUI_BUILD_CONTAINER:-open-cue-gui-build}"
if [[ "$(docker inspect -f '{{index .Config.Labels "org.open-cue-gui.purpose"}}' "$BUILD_CONTAINER" 2>/dev/null || true)" == "native-build" ]]; then
    MOUNT_SOURCE="$(docker inspect -f '{{range .Mounts}}{{if eq .Destination "/workspace"}}{{.Source}}{{end}}{{end}}' "$BUILD_CONTAINER")"
    if [[ "$MOUNT_SOURCE" != "$PROJECT_ROOT" ]]; then
        printf 'Build container mounts a different project: %s\n' "$MOUNT_SOURCE" >&2
        exit 1
    fi
    docker exec --user "$(id -u):$(id -g)" "$BUILD_CONTAINER" cmake -S . -B "$BUILD_DIRECTORY" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENQUI_USE_LOCAL_CORSAIR_RESOURCES="$LOCAL_RESOURCES"
    docker exec --user "$(id -u):$(id -g)" "$BUILD_CONTAINER" cmake --build "$BUILD_DIRECTORY" -j "$BUILD_JOBS"
else
    BUILD_IMAGE="${OPENQUI_BUILD_IMAGE:-open-cue-gui-build:fedora44}"
    docker build --build-arg "BUILD_UID=$(id -u)" --build-arg "BUILD_GID=$(id -g)" -t "$BUILD_IMAGE" -f "$PROJECT_ROOT/packaging/Dockerfile.build" "$PROJECT_ROOT/packaging"
    docker run --rm --user "$(id -u):$(id -g)" --mount "type=bind,src=$PROJECT_ROOT,dst=/workspace" "$BUILD_IMAGE" cmake -S . -B "$BUILD_DIRECTORY" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENQUI_USE_LOCAL_CORSAIR_RESOURCES="$LOCAL_RESOURCES"
    docker run --rm --user "$(id -u):$(id -g)" --mount "type=bind,src=$PROJECT_ROOT,dst=/workspace" "$BUILD_IMAGE" cmake --build "$BUILD_DIRECTORY" -j "$BUILD_JOBS"
fi
