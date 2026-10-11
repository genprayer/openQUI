# Development

## Source map

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Process modes, CLI, Qt setup and single D-Bus owner |
| `src/core/workspace.*` | Locked profile transactions and atomic persistence |
| `src/devices/catalog.*` | Authored/local manifests, geometry and editor metadata |
| `src/devices/inventory.*` | USB/sysfs inventory; no configuration writes |
| `src/devices/hid_transport.*` | Descriptor allowlists, report framing and bounded exchanges |
| `src/devices/hardware_backend.*` | Verified model-specific RGB, battery, DPI and pairing operations |
| `src/devices/keyboard_input.*` | Passive K70 input decoding for Type lighting |
| `src/gui/application_model.*` | QML-facing state, layer composition and operation scheduling |
| `src/gui/device_selection.*` | Click, modifiers and rectangle selection |
| `src/audio/` | Filter graph generation, device routing and ALSA controls |
| `src/profiles/cereal.*` | Bounded XML import and unsupported-data preservation |
| `src/service/` | Session D-Bus facade |
| `qml/` | Qt Quick application and reusable controls |
| `resources/` | Authored icons, layouts, drawings and attributed impulse responses |
| `translations/` | English, German and Russian catalogs |
| `tests/native/` | QtTest suites and isolated PipeWire integration |
| `tools/` | Asset, translation and dataset maintenance; no application runtime |
| `packaging/` | Desktop entry, optional service template and build container |

## Editing QML and C++

Open this directory as a CMake project in Qt Creator, or use an editor with
clangd and point it at `build/native/compile_commands.json`. Choose the
appropriate build preset. QML is embedded in the executable, so rebuild after
changing it. The preview command in [getting-started.md](getting-started.md)
uses a temporary workspace and disables hardware writes.

Keep the current module boundaries. A control updates the document through
`ApplicationModel`; it should not open a hidraw device, create its own audio
process or write JSON directly. Hardware transactions belong to the backend.
Report failures as failures rather than updating a success label optimistically.

New public functions need a short description of their contract. Use explicit
ownership, RAII for file descriptors and resource registrations, scoped locks,
immutable constants and small functions. Preserve bounds and rollback paths.
Avoid unrelated framework rewrites to fix a single device operation.

## Formatting

C++ formatting is pinned to clang-format 19 and `.clang-format`:

```sh
find src tests/native -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format-19 -i
find src tests/native -type f \( -name '*.cpp' -o -name '*.h' \) -print0 \
  | xargs -0 clang-format-19 --dry-run --Werror
git diff --check
```

Use the style of neighboring QML controls and the settings in `.editorconfig`.
Keep UI text translatable. There is no mandatory GPU, CUDA, Vulkan or Python
runtime for this Qt Quick app.

## Translations

German source keys use context `OpenICUE`; stable context/catalog filenames are
retained. Device names and user profile names are not translated. Add a string
through the existing translation path, update all three languages, then run:

```sh
python3 tools/localization/update_catalog.py
python3 tools/localization/update_catalog.py --check
cmake --build --preset native --parallel 2
ctest --test-dir build/native -R 'native-translations|native-gui' --output-on-failure
```

Do not mark a release complete when a language contains unfinished or missing
messages. Translation checks test the catalogs and rendered GUI behavior.

## Forks and packaging

Change application presentation deliberately. Legacy state, D-Bus and PipeWire
identifiers are documented compatibility interfaces. A fork that changes those
identifiers needs its own data migration and routing ownership tests.

Use `cmake --install build/native --prefix "$staging_prefix"` with an explicit
staging prefix when packaging. The service file is a template; installing or
starting it is an explicit distribution/user choice. CI builds Fedora/Debian
from source, uses read-only GitHub permissions and does not publish artifacts
or perform hardware writes.
