# Architecture

OpenQUI separates local profile state from hardware observations and audio.
The GUI and session D-Bus API share one C++ controller. Stable model, key and
effect IDs stay independent of translated labels.

| Directory | Responsibility |
| --- | --- |
| `src/core` | Workspace locking and atomic JSON persistence |
| `src/devices` | Device catalog, libudev inventory and verified hidraw protocol operations |
| `src/profiles` | Bounded Cereal XML import, partial native lighting conversion and original-byte preservation |
| `src/audio` | ALSA mixer, stereo EQ, measured binaural 7.1 and scoped PipeWire routing |
| `src/gui` | Qt controller, serialized hardware worker, selection geometry, providers, translations and tray |
| `src/service` | Typed session D-Bus adaptor |
| `qml` | Device screens and common controls |
| `resources/devices` | Authored device SVG drawings and catalog data |
| `resources/icons` | SVG icons with semantic color roles |
| `Profiles` | Official-gallery catalog and local downloads; independent of user's active workspace |
| `translations` | Authored language catalog and generated Qt TS sources |
| `tests` | Core, GUI, D-Bus, translation and isolated audio checks |
| `packaging` | Build image, desktop entry and integration templates |

The workspace uses QLockFile and QSaveFile. Import rejects DTD and entity
references and preserves the source XML bytes. Supported static, gradient, wave,
pulse and per-key gradient layers map to known device zones; warnings retain
conversion boundaries. See [profiles.md](profiles.md) for execution limits.
The preview and hardware renderer share normalized gradient interpolation.
UI preferences use a separate atomic file; changing language does not rename profiles or alter key bindings.

The source-only catalog and diagrams are independent application resources.
The native/editor presets automatically select locally supplied original
device images, masks and metadata when all required bundles exist. The
local-assets preset requires them; source-only explicitly selects fallback
drawings. The local-resource build does not
establish permission to redistribute them. Neither catalog proves that
hardware settings can be applied.

Device discovery reads USB descriptors, bound drivers, input-interface names
and power attributes. Command discovery admits known vendor descriptors and
never consumes boot keyboard/mouse input. The exclusive command transport
rechecks USB identity and descriptor after opening, bounds every request and
requires matching replies and success status. Unsupported operations fail.

Reactive K70 lighting uses a separate passive input reader. It opens only the
keyboard HID interface under the selected command device's USB ancestry, then
rechecks VID/PID and the exact observed descriptor. Numbered array and NKRO
reports map physical key usages to known LED slots. Invalid and media reports
do not release held keys. A bounded per-key timestamp map remains in memory
until the editor releases control; raw reports and key histories are not saved.
The frame renderer composes these triggers with continuous layers and supplies
the same outgoing colors to the device preview. Operation state exposes only
an aggregate trigger count, not a report of physical LED colors.

The controller serializes hardware jobs through one QtConcurrent worker and
reports pending operations separately from acknowledged writes. A 30-second
telemetry timer updates device state; software RGB frames run at up to 20 Hz.
Layer edits replace the pending frame while a transaction is in flight. Only
the active lighting editor sends frames; profile activation replays separate
routes for every connected supported peripheral in the same worker. Leaving
the editor restores owned firmware control for the whole group. An empty layer
stack sends black rather than retaining old RGB.
Active Darkstar DPI shares the RGB command session and requires identity,
acknowledgement, exact value readback and an unchanged active stage index.
Editing local stage presets does not schedule device writes. Rollback attempts
and explicit control-mode restoration failures remain visible in operation results.
Normal session cleanup restores the observed initial control mode. Pairing uses
a process lock and an atomic owner-only journal keyed by both USB serials.
Identifier and key writes remain ordered on each device; retries reuse the same
credentials. Pending pairing journals also restore recorded modes after restart.
An abrupt process kill during ordinary RGB/DPI control cannot run RAII cleanup;
that separate recovery path remains unfinished.

The ten-band stereo EQ creates twenty peaking filters and two preamps for an
explicit output node. The app owns its filter process and stops it on exit;
on Linux the child also exits if its parent dies. Graph-loading tests use an
isolated PipeWire server and do not demonstrate audible headset behavior.
Optional 7.1 adds sixteen measured KEMAR convolvers, two mixers and a low-pass
LFE path before the stereo EQ. The embedded HRIR asset is hash-checked before
atomic export. Eight spaced input impulses test actual signal processing,
channel coverage, finite output, stereo differences and clipping.
Physical GUI routing verifies the headset's ALSA card against its exact USB
ancestry, binds the virtual sink to the owned filter PID, and prevents playback
fallback to other devices. Default-output and linked-stream metadata changes are
restored only if still owned by the application. The Stereo/7.1 button uses this
route directly. Locally supplied original voice recordings target the same
physical headset. Metadata restoration after an abrupt application kill remains
unfinished; normal shutdown and filter replacement restore it.

GUI tests send native Qt events to an offscreen window. The optional launch
probe observes a real KDE window and process; that probe requires KWin,
while the application itself does not.
