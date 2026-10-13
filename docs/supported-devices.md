# Supported devices

The goal is native Linux support across Corsair's device range through independent
C++ implementations. The tables below describe current coverage, not that goal's
completion. The [broader backlog](corsair-device-backlog.md) records pending models.

OpenQUI currently has **three peripheral implementations and two receiver UI
entries**. All entries below are **partially supported**: a working RGB or
battery path does not imply every iCUE feature is available. Connection type and
USB identity matter. Other firmware, regional IDs or product variants need their
own protocol and hardware checks.

- [Keyboards](#keyboards)
- [Mice](#mice)
- [Headsets](#headsets)
- [Receivers](#receivers)
- [Other Corsair models](#other-corsair-models)

The [feature matrix](compatibility.md#capabilities) records capability limits;
[validation](validation.md#local-installation-and-physical-observations) separates
automated tests, protocol acknowledgements and physical observations.

## Keyboards

| Model | Connection / USB ID | Available implementation |
| --- | --- | --- |
| K70 CORE RGB | USB `1b1c:1bfd` | Per-key RGB, layer composition and reactive effects using verified passive key reports |

The user confirmed physical press flashes and current DOOM lighting. Assignment
upload, global macros, onboard profile storage, polling-rate changes and firmware
updates remain unimplemented. Other K70 CORE IDs and the TKL/Wireless models are
not automatically supported.

## Mice

| Model | Connection / USB ID | Available implementation |
| --- | --- | --- |
| DARKSTAR WIRELESS | USB `1b1c:1bb2`; verified radio child of `1b1c:1bdc` | Wired/wireless RGB, battery queries, radio status and guarded wired active-DPI transactions |

The fifteen-button assignment editor and F1–F24 selector store local profile
settings. They do not execute global bindings or upload assignments. DPI
acknowledgement/readback is implemented; physical pointer measurement remains
pending. Full stage/Sniper tables, gesture execution and surface calibration are
not implemented. Receiver detection alone does not establish a Darkstar link.

## Headsets

| Model | Connection / USB ID | Available implementation |
| --- | --- | --- |
| VIRTUOSO | Headset through receiver `1b1c:0a42` | RGB, battery queries, receiver-specific audio routing, ten-band EQ and independent binaural 7.1 rendering |

The user confirmed illumination on the headset and heard the headset audio test.
Perceived surround positioning remains a separate acceptance check. Battery
queries produced valid replies, but a later read timed out; the cause remains
unverified. The [validation record](validation.md) contains the observations.
The `0a41` catalog entry does not establish wired RGB control. VIRTUOSO SE, XT,
PRO, MAX, HS and VOID variants are not implicitly covered by this implementation.

## Receivers

| Model | USB ID | Available implementation |
| --- | --- | --- |
| SLIPSTREAM WIRELESS USB Receiver | `1b1c:1bdc` | Scoped USB inventory, verified Darkstar child identity and guarded pairing with a wired Darkstar |
| VIRTUOSO receiver | `1b1c:0a42` | Scoped inventory, tested VIRTUOSO RGB/battery transport and headset audio path |

Darkstar pairing has transaction and identifier-readback checks; operation after
removing the USB cable remains a separate physical test. Full Multipoint is not
implemented. OpenQUI does not pair Darkstar to the VIRTUOSO receiver. The `0a4a`
receiver ID is metadata only and is outside the current command allowlist.

## Other Corsair models

These links lead to a sourced product backlog, **not additional supported devices**.
The inventory includes overlapping product lists and is not a count of every
historical SKU. Basic Linux keyboard, mouse or USB-audio operation is independent
of OpenQUI's vendor configuration support.

| Category | Product inventory | OpenQUI implementation |
| --- | --- | --- |
| Keyboards | [iCUE 4 models](corsair-device-backlog.md#keyboards), [registry models](corsair-device-backlog.md#keyboard) | Only the K70 path above |
| Mice | [iCUE 4 models](corsair-device-backlog.md#mice), [registry models](corsair-device-backlog.md#mouse) | Only the Darkstar paths above |
| Headsets | [VIRTUOSO, HS and VOID models](corsair-device-backlog.md#headsets) | Only the VIRTUOSO receiver path above |
| Hubs, fans and pumps | [Controllers/accessories](corsair-device-backlog.md#pc-fan-accessories), [fans](corsair-device-backlog.md#fan), [other registry devices](corsair-device-backlog.md#other) | Not implemented |
| AIO coolers | [AIO inventory](corsair-device-backlog.md#aio-coolers) | Not implemented |
| DDR4 / DDR5 memory | [DRAM inventory](corsair-device-backlog.md#dram) | Not implemented |
| Power supplies | [PSU inventory](corsair-device-backlog.md#power-supply-units) | Not implemented |
| Mouse mats and headset stands | [Mouse mats](corsair-device-backlog.md#mouse-mats), [stands](corsair-device-backlog.md#headset-stands) | Not implemented |
| Cases and systems | [Cases](corsair-device-backlog.md#cases), [systems](corsair-device-backlog.md#systems) | Not implemented |
| Monitors | [Monitor inventory](corsair-device-backlog.md#monitors) | Not implemented |

To contribute a device, follow [adding-devices.md](adding-devices.md). Images,
source manifests or another application's device list do not enable hardware
writes in OpenQUI.
