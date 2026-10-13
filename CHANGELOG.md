# Changelog

## Unreleased — OpenQUI repository

- Add a categorized supported-device page and documentation index; shorten README navigation and document project intent, hardware testing limits and possible OpenLinkHub reuse.
- Refresh README captures from the original-resource build, showing selected WASD keys and the VIRTUOSO equalizer.
- Replay rain and supported ripple modes in native C++, including bounded history for overlapping K70 key presses.
- Import global key-triggered gradient curves and retain source rain/ripple parameters with explicit approximation warnings.
- Share rain/ripple geometry with the local preview; make rain opacity/direction controls live.
- Allow user installation of an already tested binary with `--skip-build`.
- Add the official-gallery profile catalog/downloader and retain 82 local original downloads.
- Move profile creation below the list and add right-click rename, duplicate and delete.
- Replay activated profiles on each connected supported peripheral while the lighting editor is open; preserve iCUE layer order and convert moving waves, pulse colors and K70 per-key gradient curves.
- Preserve previous converter versions and local edits when explicitly reimporting a profile.
- Automatically select complete local original resource sets and show original device views in README previews.
- Document the sourced Corsair model backlog separately from verified Linux support.
- Use `openQUI` as the repository name and directory.
- Add a Russian VS Code setup/troubleshooting guide and preset-based Build/Test tasks.
- Associate Qt translation catalogs with XML in VS Code; provide a Qt/C++ development container and isolated editor preset.

- Rename the application presentation and launcher to OpenQUI / open-cue-gui.
- Keep profile, D-Bus and PipeWire identifiers compatible with existing installations.
- Separate native and optional original-resource build presets.
- Import every available device RCC and static resource with a portable asset index.
- Register imported bundles with RAII cleanup, including partial-registration failure.
- Add importer recovery/validation tests and developer/device contribution guides.
- Exclude local assets, analysis artifacts and builds from Git; retain two README previews.

## Unreleased — 0.6.0

- Restore original device renderings in builds with locally supplied resources;
  keep K70's German ISO image and hit geometry aligned.
- Stream current lighting layers automatically while the lighting editor is active.
  Coalesce in-flight edits, update zones during rectangle selection and clear LEDs
  when the last layer is removed. Remove the manual lighting application buttons.
- Preview and transmit color-picker changes before OK; Cancel restores the old color.
- React to K70 key presses through its validated HID input interface, preserve
  continuous layers and show the outgoing RGB frame in the device preview.
- Toggle Stereo and 7.1 Sound directly, automatically target the selected Virtuoso,
  route through its EQ filter and restore only audio metadata still owned by the app.
- Play the supplied original Surround On/Off PCM notifications through that headset.
- Scope USB/drivers and pairing controls to the selected device or receiver.
- Add GUI-to-HID and audio-route restoration tests, with de/en/ru translations.

## 0.5.0

- Bound the first HID resource chunk to the available data; prevent reading past short
  pairing/RGB buffers and verify zero padding and continuation boundaries in Debug builds.
- Add active Darkstar software DPI with identity checks, acknowledgements, exact readback,
  unchanged stage index checks and rollback. Physical DPI acceptance remains pending.
- Expose DPI application in QML and session D-Bus; keep local edits separate from writes.
- Report firmware mode restoration failures instead of displaying a successful release.
- Restrict independent X/Y DPI using device capabilities.
- Convert supported Cereal static and continuous gradient layers, with duration, timing,
  alpha and known zones; preserve the original XML and report unsupported content.
- Keep imported gradient timing editable and use the same timeline in preview and HID frames.
- Add headless profile import, fault and GUI tests, and a source-based feature inventory.
- Translate native validation and recovery errors in German, English and Russian,
  retaining HID command/status codes and checking coverage of literal native errors.

## 0.4.0

- Add descriptor-checked hidraw command sessions with bounded replies and mode restoration.
- Add acknowledged software RGB for K70 CORE, wired Darkstar and Virtuoso; implement the
  Darkstar receiver path with protocol tests, pending physical wireless acceptance.
- Add Darkstar/1bdc pairing, private restart recovery and identifier readback; RF operation
  after cable removal remains pending.
- Add real battery telemetry and a translated system tray.
- Add measured KEMAR 7.1 binaural rendering and an eight-channel signal test in private PipeWire.
- Add recovery/fault tests and translate hardware status and diagnostic messages.

## 0.3.0

- Add German, English and Russian UI with persistent language selection.
- Add Open-iCUE branding and a user installer with atomic replacement and backups.
- Add read-only USB driver, input-interface and power diagnostics.
- Add original device schematics and starter EQ presets for builds without vendor files.
- Add public build presets, distribution requirements, MIT licensing and contribution rules.
- Format native C++ and QML and add multilingual navigation and F24 checks.

## 0.2.0

- Introduce the C++20/Qt 6 application, native profiles and bounded Cereal XML import.
- Add local EQ, DPI, gestures, lighting layers and key assignment editors.
- Add keyboard contour selection and all fifteen physical Darkstar assignment targets.
- Add a 118-key virtual keyboard with F1–F24 and numpad.
- Add stereo PipeWire EQ lifecycle, ALSA mixer access and session D-Bus.
- Add core, GUI, D-Bus and isolated audio tests.

## 0.1.0

- Initial prototype, superseded by the native implementation.
