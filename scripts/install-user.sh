#!/usr/bin/env bash
# Build and atomically replace the user launcher, native executable and icon.
set -euo pipefail
PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIRECTORY="${OPENQUI_BUILD_DIRECTORY:-build/native}"
INSTALL_PREFIX="${OPENQUI_INSTALL_PREFIX:-$HOME/.local}"
DATA_DIRECTORY="${OPENQUI_INSTALL_DATA_DIRECTORY:-$INSTALL_PREFIX/share}"
for path in "$INSTALL_PREFIX" "$DATA_DIRECTORY"; do
    if [[ "$path" != /* || "$path" == *$'\n'* || "$path" == *$'\r'* ]]; then
        printf 'Installation directories must be absolute paths without line breaks.\n' >&2
        exit 1
    fi
done
"$PROJECT_ROOT/scripts/build.sh"
NATIVE_BINARY="$PROJECT_ROOT/$BUILD_DIRECTORY/open-cue-gui"
if [[ ! -x "$NATIVE_BINARY" ]]; then
    printf 'Native build is missing: %s\n' "$NATIVE_BINARY" >&2
    exit 1
fi
if ! QT_QPA_PLATFORM=offscreen "$NATIVE_BINARY" --version >/dev/null; then
    printf 'The binary cannot run on this system. Install matching Qt/runtime libraries and build for this distribution.\n' >&2
    exit 1
fi
mkdir -p -- "$INSTALL_PREFIX/bin" "$DATA_DIRECTORY/applications" "$DATA_DIRECTORY/icons/hicolor/scalable/apps" "$DATA_DIRECTORY/open-cue-gui"
exec 9>"$DATA_DIRECTORY/open-cue-gui/install.lock"
flock -x 9
WORK_DIRECTORY="$(mktemp -d "$DATA_DIRECTORY/open-cue-gui/install-stage.XXXXXX")"
CURRENT_STAGE=""
trap 'rm -f -- "$CURRENT_STAGE"; rm -rf -- "$WORK_DIRECTORY"' EXIT
DESTINATION_BINARY="$INSTALL_PREFIX/bin/open-cue-gui"
EXEC_VALUE="${DESTINATION_BINARY//\\/\\\\}"
EXEC_VALUE="${EXEC_VALUE//\"/\\\"}"
EXEC_VALUE="${EXEC_VALUE//\$/\\\$}"
EXEC_VALUE="${EXEC_VALUE//\`/\\\`}"
EXEC_VALUE="${EXEC_VALUE//%/%%}"
while IFS= read -r line; do
    if [[ "$line" == Exec=* ]]; then printf 'Exec="%s"\n' "$EXEC_VALUE";
    else printf '%s\n' "$line"; fi
done < "$PROJECT_ROOT/packaging/open-cue-gui.desktop" > "$WORK_DIRECTORY/open-cue-gui.desktop"
if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$WORK_DIRECTORY/open-cue-gui.desktop"
fi
BACKUP_DIRECTORY=""
replace_file() {
    local source="$1" destination="$2" mode="$3" backup_name="$4"
    if [[ -f "$destination" ]] && cmp -s -- "$source" "$destination"; then return; fi
    if [[ -e "$destination" || -L "$destination" ]]; then
        if [[ -z "$BACKUP_DIRECTORY" ]]; then
            mkdir -p -- "$DATA_DIRECTORY/open-cue-gui/install-backups"
            BACKUP_DIRECTORY="$(mktemp -d "$DATA_DIRECTORY/open-cue-gui/install-backups/$(date -u +%Y%m%dT%H%M%SZ).XXXXXX")"
        fi
        cp -p -- "$destination" "$BACKUP_DIRECTORY/$backup_name"
        printf '%s\t%s\n' "$backup_name" "$destination" >> "$BACKUP_DIRECTORY/paths.tsv"
    fi
    CURRENT_STAGE="$(mktemp "${destination}.stage.XXXXXX")"
    install -m "$mode" -- "$source" "$CURRENT_STAGE"
    mv -f -- "$CURRENT_STAGE" "$destination"
    CURRENT_STAGE=""
}
replace_file "$NATIVE_BINARY" "$DESTINATION_BINARY" 755 open-cue-gui
replace_file "$PROJECT_ROOT/resources/branding/open-cue-gui.svg" "$DATA_DIRECTORY/icons/hicolor/scalable/apps/open-cue-gui.svg" 644 open-cue-gui.svg
replace_file "$WORK_DIRECTORY/open-cue-gui.desktop" "$DATA_DIRECTORY/applications/open-cue-gui.desktop" 644 open-cue-gui.desktop
if command -v update-desktop-database >/dev/null; then update-desktop-database "$DATA_DIRECTORY/applications"; fi
if command -v gtk-update-icon-cache >/dev/null; then gtk-update-icon-cache -f -t "$DATA_DIRECTORY/icons/hicolor"; fi
if command -v kbuildsycoca6 >/dev/null; then kbuildsycoca6 --noincremental; fi
printf 'Installed: %s\nDesktop entry: %s\n' "$DESTINATION_BINARY" "$DATA_DIRECTORY/applications/open-cue-gui.desktop"
sha256sum "$DESTINATION_BINARY"
if [[ -n "$BACKUP_DIRECTORY" ]]; then printf 'Previous files preserved: %s\n' "$BACKUP_DIRECTORY"; fi
