#!/usr/bin/env bash
# Load the C++-generated EQ into a private PipeWire daemon without host routing.
set -euo pipefail
ICUE_BINARY="${1:?Pass the native open-icue binary}"
ICUE_ARTIFACTS="${OPENQUI_PIPEWIRE_ARTIFACTS:-$(mktemp -d /tmp/open-icue-pipewire.XXXXXX)}"
ICUE_SERVER_NAME="open-icue-native-test"
ICUE_WAIT_ATTEMPTS=60
ICUE_POLL_INTERVAL=0.1
ICUE_SERVER_PID=""
ICUE_FILTER_PID=""
ICUE_PLAY_PID=""
ICUE_RECORD_PID=""
ICUE_RUNTIME_PARENT="${OPENQUI_PIPEWIRE_RUNTIME_PARENT:-${TMPDIR:-/tmp}}"
ICUE_RUNTIME_DIRECTORY="$(mktemp -d "$ICUE_RUNTIME_PARENT/oicue-pw.XXXXXX")"
mkdir -p "$ICUE_ARTIFACTS/config"
printf '%s\n' "$ICUE_RUNTIME_DIRECTORY" > "$ICUE_ARTIFACTS/runtime-directory.txt"
# Unix socket addresses are bounded independently of the artifact directory depth.
export XDG_RUNTIME_DIR="$ICUE_RUNTIME_DIRECTORY"
export PIPEWIRE_RUNTIME_DIR="$XDG_RUNTIME_DIR"
export PIPEWIRE_REMOTE="$ICUE_SERVER_NAME"
export XDG_CONFIG_HOME="$ICUE_ARTIFACTS/config"
cleanup() {
    if [[ -n "$ICUE_PLAY_PID" ]]; then kill "$ICUE_PLAY_PID" 2>/dev/null || true; wait "$ICUE_PLAY_PID" 2>/dev/null || true; fi
    if [[ -n "$ICUE_RECORD_PID" ]]; then kill "$ICUE_RECORD_PID" 2>/dev/null || true; wait "$ICUE_RECORD_PID" 2>/dev/null || true; fi
    if [[ -n "$ICUE_FILTER_PID" ]]; then kill "$ICUE_FILTER_PID" 2>/dev/null || true; wait "$ICUE_FILTER_PID" 2>/dev/null || true; fi
    if [[ -n "$ICUE_SERVER_PID" ]]; then kill "$ICUE_SERVER_PID" 2>/dev/null || true; wait "$ICUE_SERVER_PID" 2>/dev/null || true; fi
    rm -rf -- "$ICUE_RUNTIME_DIRECTORY"
}
trap cleanup EXIT
cat > "$ICUE_ARTIFACTS/server.conf" <<EOF
context.properties = { core.daemon = true core.name = "$ICUE_SERVER_NAME" }
context.spa-libs = {
    support.* = support/libspa-support
    audio.convert.* = audioconvert/libspa-audioconvert
}
context.modules = [
    { name = libpipewire-module-protocol-native }
    { name = libpipewire-module-spa-node-factory }
    { name = libpipewire-module-client-node }
    { name = libpipewire-module-adapter }
    { name = libpipewire-module-link-factory }
    { name = libpipewire-module-access args = {
        access.socket = { "$ICUE_SERVER_NAME" = unrestricted "$ICUE_SERVER_NAME-manager" = unrestricted }
    } }
]
context.objects = [
    { factory = spa-node-factory args = {
        factory.name = support.node.driver node.name = Test-Driver
        node.group = pipewire.dummy priority.driver = 20000
    } }
    { factory = adapter args = {
        factory.name = support.null-audio-sink node.name = isolated-output
        node.description = Isolated-Test-Output media.class = Audio/Sink
        audio.channels = 2 audio.position = [ FL FR ]
    } }
]
EOF
pipewire -c "$ICUE_ARTIFACTS/server.conf" > "$ICUE_ARTIFACTS/server.log" 2>&1 &
ICUE_SERVER_PID=$!
ICUE_IS_READY=false
for ((i=0; i<ICUE_WAIT_ATTEMPTS; ++i)); do
    if ! kill -0 "$ICUE_SERVER_PID" 2>/dev/null; then cat "$ICUE_ARTIFACTS/server.log" >&2; exit 1; fi
    if timeout 2 pw-dump > "$ICUE_ARTIFACTS/nodes.json" 2>/dev/null && jq -e 'any(.[]; .info.props["node.name"] == "Test-Driver")' "$ICUE_ARTIFACTS/nodes.json" >/dev/null; then ICUE_IS_READY=true; break; fi
    sleep "$ICUE_POLL_INTERVAL"
done
if [[ "$ICUE_IS_READY" != true ]]; then printf 'Private PipeWire daemon did not become ready\n' >&2; exit 1; fi
"$ICUE_BINARY" --state-directory "$ICUE_ARTIFACTS/state" --device virtuoso_black --export-eq "$ICUE_ARTIFACTS/equalizer.conf" --target isolated-output
pipewire -c "$ICUE_ARTIFACTS/equalizer.conf" > "$ICUE_ARTIFACTS/filter.log" 2>&1 &
ICUE_FILTER_PID=$!
ICUE_IS_READY=false
for ((i=0; i<ICUE_WAIT_ATTEMPTS; ++i)); do
    if ! kill -0 "$ICUE_FILTER_PID" 2>/dev/null; then cat "$ICUE_ARTIFACTS/filter.log" >&2; exit 1; fi
    if timeout 2 pw-dump > "$ICUE_ARTIFACTS/nodes.json" 2>/dev/null && jq -e '[.[] | select(.info.props["node.name"] == "open-icue.input" or .info.props["node.name"] == "open-icue.output") | .info.props] | length == 2 and all(.[]; .["audio.channels"] == 2)' "$ICUE_ARTIFACTS/nodes.json" >/dev/null; then ICUE_IS_READY=true; break; fi
    sleep "$ICUE_POLL_INTERVAL"
done
if [[ "$ICUE_IS_READY" != true ]]; then cat "$ICUE_ARTIFACTS/filter.log" >&2; printf 'Stereo EQ nodes were not created\n' >&2; exit 1; fi
jq '[.[] | select(.info.props["node.name"] == "open-icue.input" or .info.props["node.name"] == "open-icue.output") | {id,props:.info.props}]' "$ICUE_ARTIFACTS/nodes.json" > "$ICUE_ARTIFACTS/eq-nodes.json"
kill "$ICUE_FILTER_PID"
wait "$ICUE_FILTER_PID" || true
ICUE_FILTER_PID=""
ICUE_DBUS_TESTS="$(dirname -- "$ICUE_BINARY")/native-dbus-tests"
"$ICUE_BINARY" --state-directory "$ICUE_ARTIFACTS/surround-state" --device virtuoso_black --surround --export-eq "$ICUE_ARTIFACTS/surround.conf" --target isolated-output
pipewire -c "$ICUE_ARTIFACTS/surround.conf" > "$ICUE_ARTIFACTS/surround.log" 2>&1 &
ICUE_FILTER_PID=$!
ICUE_IS_READY=false
for ((i=0; i<ICUE_WAIT_ATTEMPTS; ++i)); do
    if ! kill -0 "$ICUE_FILTER_PID" 2>/dev/null; then cat "$ICUE_ARTIFACTS/surround.log" >&2; exit 1; fi
    if timeout 2 pw-dump > "$ICUE_ARTIFACTS/surround-nodes.json" 2>/dev/null && jq -e 'any(.[]; .info.props["node.name"] == "open-icue.input" and .info.props["audio.channels"] == 8) and any(.[]; .info.props["node.name"] == "open-icue.output" and .info.props["audio.channels"] == 2)' "$ICUE_ARTIFACTS/surround-nodes.json" >/dev/null; then ICUE_IS_READY=true; break; fi
    sleep "$ICUE_POLL_INTERVAL"
done
if [[ "$ICUE_IS_READY" != true ]]; then cat "$ICUE_ARTIFACTS/surround.log" >&2; printf '7.1 binaural nodes were not created\n' >&2; exit 1; fi
if command -v pw-cat >/dev/null && command -v pw-link >/dev/null && command -v pw-cli >/dev/null && command -v python3 >/dev/null; then
    bash "$(dirname -- "${BASH_SOURCE[0]}")/test_surround_signal.sh" "$ICUE_ARTIFACTS"
fi
kill "$ICUE_FILTER_PID"
wait "$ICUE_FILTER_PID" || true
ICUE_FILTER_PID=""
if [[ -x "$ICUE_DBUS_TESTS" ]]; then
    OPENQUI_TEST_AUDIO_TARGET=isolated-output dbus-run-session -- "$ICUE_DBUS_TESTS" filterDiesWithOwner -o "$ICUE_ARTIFACTS/filter-lifetime-test.txt",txt
fi
printf 'Native ten-band stereo EQ loaded in private PipeWire: %s\n' "$ICUE_ARTIFACTS"
