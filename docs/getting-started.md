# Getting started

## Dependencies

The supported build contract is Linux, C++20, CMake 3.24+, Qt 6.8+, libudev and
ALSA. A Wayland desktop needs the Qt Wayland plugin. PipeWire provides headset
processing; its service remains owned by the user's session.

Fedora 44:

```sh
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config python3 \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qttools-devel \
  qt6-linguist qt6-qtwayland libudev-devel alsa-lib-devel \
  pipewire pipewire-utils dbus-daemon jq dejavu-sans-fonts
```

Debian 13:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config python3 \
  qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-svg-plugins \
  qt6-tools-dev qt6-tools-dev-tools qt6-wayland libudev-dev libasound2-dev \
  dbus-daemon pipewire-bin jq fonts-dejavu-core \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-dialogs qml6-module-qtqml-workerscript \
  qml6-module-qtquick-templates qml6-module-qtquick-window
```

Use source builds with the target distribution's Qt libraries. Check
`pkg-config --modversion Qt6Quick` before building on another distribution.
Older Qt releases require an upgrade or a separate Qt toolchain; changing only
the CMake minimum does not make unsupported APIs work.

## Native workflow

Run the commands from the repository root, after installing the development
packages above. A successful Docker build does not install headers, compilers
or CMake on the host. For editing without those host packages, use the
[VS Code development container](development.md#vs-code-development-container).
The [VS Code guide in Russian](vscode-guide.ru.md) covers setup, built-in tasks,
IntelliSense, translation file types and recovery after environment changes.

```sh
cmake --preset native
cmake --build --preset native --parallel 2
ctest --preset native
./build/native/open-cue-gui
```

The default preset selects original device renderings automatically when all
required bundles are present. Otherwise it uses authored fallback drawings.
Import original resources using [assets.md](assets.md), then force that mode with:

```sh
cmake --preset local-assets
cmake --build --preset local-assets --parallel 2
ctest --preset local-assets
./build/local-assets/open-cue-gui
```

For a build that deliberately uses only the fallback resources:

```sh
cmake --preset source-only
cmake --build --preset source-only --parallel 2
```

`OPENQUI_USE_LOCAL_CORSAIR_RESOURCES` accepts `AUTO` (default), `ON` (all required
bundles must be present) or `OFF` (fallback drawings). The build scripts follow
the same default. Reconfigure and rebuild existing binaries to change the mode.

To install in the current user's prefix:

```sh
./scripts/install-user.sh
# For an already selected local-resource configuration:
OPENQUI_BUILD_DIRECTORY=build/local-assets \
OPENQUI_USE_LOCAL_CORSAIR_RESOURCES=ON ./scripts/install-user.sh
```

The installer validates the binary and desktop entry, uses an installation
lock and preserves replaced files. It installs `open-cue-gui`, the OpenQUI
launcher and its SVG icon. It does not install udev rules or restart PipeWire.
It does not remove an older Open-iCUE installation automatically.

## Docker build

`./scripts/build.sh` uses an installed compatible native toolchain or the
Fedora build Dockerfile. Docker receives a source mount and the current UID/GID;
it receives no hardware devices. Choose resources explicitly:

```sh
./scripts/build.sh
OPENQUI_BUILD_DIRECTORY=build/local-assets \
OPENQUI_USE_LOCAL_CORSAIR_RESOURCES=ON ./scripts/build.sh
./scripts/check.sh
```

Variables are `OPENQUI_BUILD_DIRECTORY`, `OPENQUI_BUILD_JOBS`,
`OPENQUI_BUILD_IMAGE` and `OPENQUI_BUILD_CONTAINER`. A reusable container must
carry label `org.open-cue-gui.purpose=native-build` and mount this checkout at
`/workspace`. Do not reuse a build tree from another checkout or Qt version.
Container-built executables still require matching runtime libraries on the host.
The generated compilation database uses container paths such as `/workspace`;
use it inside that container, not as a host IntelliSense configuration.

## Preview without touching hardware

```sh
preview_state="$(mktemp -d)"
./build/native/open-cue-gui --no-hardware --state-directory "$preview_state" \
  --device k70_core_rgb --panel Lightings --language en
```

`--no-hardware` disables HID and headset auto-routing. A temporary state
directory keeps changes separate from actual profiles. Use `--capture` to
save a rendered preview and exit. `--dump-state` produces structured local
state; inspect it before sharing because user documents can be included.

## Device access

Linux's standard HID and USB-audio drivers handle basic input and audio.
OpenQUI uses the supported vendor command interfaces through `/dev/hidraw`.
Enumeration does not prove write access, a linked RF child or protocol support.
An access error should be diagnosed against the matching VID:PID and HID
interface. Do not run the entire GUI as root or make every hidraw node writable.
A scoped template is provided in `packaging/udev/70-openqui.rules` for the
four supported command-device USB IDs. It grants access to the active local
session through `uaccess` on systems using logind. Neither CMake nor the user
installer installs it automatically. To install it deliberately:

```sh
sudo install -m 0644 packaging/udev/70-openqui.rules /etc/udev/rules.d/70-openqui.rules
sudo udevadm control --reload-rules
```

Reconnect only the relevant device after loading the rule. Permissions do not
enable an unsupported protocol or prove a wireless connection. Distribution
packagers should validate their session access behavior; installation of this
template was not exercised during repository preparation.

Only one session owner may use `org.kde.cyberdeck.icue`. Exit an older Open-iCUE
process before launching OpenQUI normally. The D-Bus name, `open_icue` C++
namespace, PipeWire node names and the existing `OpenICUE/open-icue/native`
state directory remain stable for compatibility; the UI and launcher say OpenQUI.
