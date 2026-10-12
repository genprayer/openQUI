# Compatibility and feature status

## Linux build contract

Linux, C++20, CMake 3.24+, Qt 6.8+, libudev and ALSA are required. The headset
processor needs PipeWire and command-line utilities. Qt Wayland is needed for
a Wayland session. Package commands are in [getting-started.md](getting-started.md).

| Platform | Validation boundary |
| --- | --- |
| Fedora 44, x86-64 | Native source build and deterministic test suites; predecessor physically exercised on the user's KDE/Wayland desktop |
| Debian 13, x86-64 | Source build, GUI contracts, D-Bus and private PipeWire signal tests; physical Debian desktop/device use not validated |
| Other distributions with Qt 6.8+ | Source-build candidates; not yet tested as release targets |
| Older Qt or other operating systems | Outside the current build contract |

CI is configured for Fedora/Debian, but no GitHub run exists before publication.
Source-build success does not prove USB permissions or every physical feature.
No portable binary, AppImage, Flatpak, RPM or DEB release is supplied yet. Build
with the target distribution's shared libraries. Other architectures need their
own compile and hardware validation.

## Device identities

| Catalog entry | Identity | Evidence boundary |
| --- | --- | --- |
| K70 CORE RGB | `1b1c:1bfd` USB | RGB writes and real passive key reports; user confirmed Type flashes over a continuous wave |
| DARKSTAR WIRELESS | `1b1c:1bb2` cable; verified child on `1b1c:1bdc` | Wired/wireless RGB acknowledgements, battery and radio status |
| VIRTUOSO | `1b1c:0a41` USB catalog entry; verified command path through `0a42` | RGB/battery and live headset audio through the tested receiver |
| SLIPSTREAM receiver | `1b1c:1bdc` | Scoped inventory, child identity and guarded wired-Darkstar pairing |
| VIRTUOSO receiver | `1b1c:0a42`; `0a4a` is a catalog ID only | `0a42` transport/audio tested; `0a4a` not accepted by the current command allowlist |

There are **three peripheral implementations and two receiver editor entries**.
The [device backlog](corsair-device-backlog.md) inventories the broader Corsair
product range and records the models/variants with pending Linux support.
The only headset implementation currently verified is the VIRTUOSO path through
`0a42`. VIRTUOSO XT, PRO, MAX, HS-series and other headset revisions are not
implicitly supported. The `0a41` catalog entry does not prove wired RGB control.
Receiver enumeration alone does not identify a paired peripheral. Basic mouse,
keyboard and USB-audio use is provided by Linux's standard drivers, independently
of OpenQUI's vendor configuration support.

## Capabilities

| Feature | Current behavior and acceptance boundary |
| --- | --- |
| Local profiles | Locked atomic persistence; sidebar add and right-click rename/duplicate/delete; independent profile and language settings |
| iCUE import | Static/gradient/wave/pulse and the recorded K70 per-key gradient mode converted; source XML retained; profile activation replays each connected supported peripheral while a lighting editor is open; see [limits](profiles.md) |
| LED selection | Click, Shift/Ctrl, drag rectangle; per-layer zone selection |
| Live RGB | Sends frames only while a lighting editor is active; profile activation includes each connected supported peripheral; coalesces edits; picker previews before OK and Cancel restores the previous color |
| K70 Type effect | Real descriptor-verified key input, fading and composition with continuous layers; physical reaction confirmed by the user |
| Other reactive lighting | Ripple/Rain and audio-reactive effects are not implemented; editor presence does not establish hardware execution |
| Mouse assignment editor | Fifteen physical inputs and a virtual keyboard with F1–F24; profile storage only |
| Active Darkstar DPI | Guarded wired operation, identity/ACK/value/index readback and rollback tests; physical pointer measurement pending |
| DPI stage/Sniper tables, gesture execution and surface calibration | Editors/local state do not yet implement full hardware behavior |
| RGB hardware writes | Transport acknowledgements on K70, wired/wireless Darkstar and VIRTUOSO/0a42; exact colors require physical observation |
| Battery | Verified Darkstar and VIRTUOSO/0a42 queries; tray display; no UPower provider |
| SLIPSTREAM pairing | Darkstar + separate `1bdc` receiver only; writes/identifier readback implemented; cable-removal RF acceptance pending |
| Full Multipoint | Not implemented; the VIRTUOSO receiver is not used for Darkstar pairing by this implementation |
| Ten-band EQ | 32 Hz–16 kHz, ±12 dB, native PipeWire stereo graph |
| 7.1 Sound | Independent KEMAR binaural 8-input/2-output graph; private signal test and live receiver route exercised; user heard the headset test, perceived surround positioning pending |
| Audio controls | Auto-selects the verified VIRTUOSO route; restores only metadata owned by the app; available ALSA setters require physical acceptance |
| Original Surround On/Off prompts | Optional local PCM data; playback exercised on the user's snapshot; independent-resource build does not bundle those prompts |
| UI languages | English, German and Russian; model names unchanged |
| Macros/global key replay, hardware assignment upload, onboard profiles, firmware update | Not implemented |
| Every Corsair product / complete iCUE parity | Not implemented |

## Assets are a separate inventory

The local importer has no five-package ceiling. It stores every supplied RCC
and static resource with provenance. The audited installation has five Corsair
bundles and three partner integrations; it lacks the complete Windows iCUE
product catalog. See [assets.md](assets.md) and [asset-catalog.json](asset-catalog.json).
Images for an unsupported product neither enable a driver nor establish testing.
To add another device, follow [adding-devices.md](adding-devices.md).
