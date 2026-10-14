# Testing

## Deterministic baseline

```sh
python3 -m unittest discover -s tests/tools -v
python3 tools/localization/update_catalog.py --check
cmake --preset native
cmake --build --preset native --parallel 2
ctest --preset native
```

The standard suites use fixtures or fake HID exchanges. They do not require
connected peripherals and do not flash firmware or pair hardware. The GUI
suite uses Qt input events against the real QML controls with hardware disabled.
Physical slots remain skipped unless `OPENQUI_LIVE_TEST=1` is explicitly set.

| Suite | Evidence |
| --- | --- |
| `asset-import` | RCC bounds, compression, traversal, repeatability and preservation after failure |
| `native-core` | Workspace locks/persistence, profile import, layer metadata and filter generation |
| `native-hardware` | Framing, identities, stale replies, lighting, DPI and failure contracts |
| `native-keyboard-input` | Exact supported descriptor and bounded key-report decoding |
| `native-pairing` | Pairing transaction, readback, journaling and recovery with fake exchanges |
| `native-audio-routing` | Device ownership, output selection and default-route restoration contracts |
| `native-translations` | Complete de/en/ru catalogs and operation-message translation |
| `native-gui` | Selection, picker preview/cancel, profile editors, direct surround toggle and locale switching |
| `native-dbus` | Isolated session service and single-owner behavior |
| `native-pipewire` | Private PipeWire server, actual graph nodes and eight-channel-to-stereo signal path |

`native-dbus` is added when `dbus-run-session` is available. `native-pipewire`
is added when `pipewire`, `pw-dump` and `jq` are available. Always inspect
`ctest --test-dir build/native -N`: an absent optional suite is not a pass.
Use `LANG=C.UTF-8 LC_ALL=C.UTF-8` for private audio tests so node names are
stable. The private server uses a temporary runtime directory and avoids the
actual desktop server.

Run the narrow relevant group while developing, then the baseline once for the
finished change. CI targets the clean default-resource build on Fedora 44
and Debian 13. See [GitHub validation](validation.md#github-actions) for actual
run results and checks that still need to be exercised.

## Hardware acceptance

A hardware claim needs model, USB ID, firmware, transport, descriptor and
recorded observations. Keep redacted results in ignored `artifacts/`; do not
commit wireless keys or raw keystroke histories. See
[adding-devices.md](adding-devices.md) for the protocol gates.

The read-only probe does not send configuration writes:

```sh
./build/native/open-cue-gui --probe-hardware
```

`--test-lighting` sends real RGB commands and restores firmware control afterward.
`--pair-darkstar` changes the pairing relationship for the supported wired
Darkstar and `1bdc` receiver. Use those only as deliberate physical tests, after
reading the model-specific restrictions. They are not part of the default test
suite and must never be run blindly on a similarly named model.

Opt-in GUI slots `connectedDevicesAcceptLiveRgbEdits` and `connectedVirtuosoUsesExclusiveAudioRoute`
perform live operations. To run a supported slot deliberately:

```sh
OPENQUI_LIVE_TEST=1 OPENQUI_GUI_ARTIFACTS="$artifact_directory" \
  ./build/local-assets/native-gui-tests connectedDevicesAcceptLiveRgbEdits
```

Verify the exact slot name in the current test binary with `-functions` before
running it. Close the normal GUI first to avoid competing device owners. A HID
acknowledgement proves transport success; observe colors on the device as well.
For pairing, unplug the cable and test 2.4 GHz input. For DPI, measure pointer
movement. For surround, route multichannel content and listen for positioning;
a stereo test sound proves audibility, not perceived 7.1 quality.

`native-launch-tests` is an opt-in KDE installed-launch test, not a standard
headless suite. It opens a window and reads KWin state. Use a temporary workspace
and the matching installed executable; do not confuse it with the default GUI
contract suite.
