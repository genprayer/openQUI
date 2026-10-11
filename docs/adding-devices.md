# Adding a Corsair device

OpenQUI's hardware support is an explicit allowlist. A new image, a USB name or
an iCUE manifest is insufficient to enable writes. Headsets of the same family
may use different receivers, descriptors, report layouts and audio controls.

## 1. Establish identity and scope

Record the exact product variant, firmware, cable/Bluetooth/2.4 GHz connection,
receiver VID:PID, interface number and HID report descriptor. Start with
read-only USB enumeration. Capture only the interface needed for the feature.
For a wireless device, verify the receiver's linked child, not merely that the
receiver is plugged in. Specify the initial feature, such as battery reads or
static RGB; do not promise the complete device feature list immediately.

## 2. Add editor metadata and pictures

`resources/devices/open-catalog.json` contains independent editor definitions.
Use an existing model as a structural example, not as an assumed protocol.
Define LED IDs/geometry, buttons, navigation, UI controls and validated USB IDs.
Authored drawings and layouts belong in tracked `resources/devices/` and
`resources/keyboard-layouts/`. Optional original packages are imported into
ignored `resources/vendor/library/` using [assets.md](assets.md).

The current `MODEL_IDS` in `src/devices/catalog.cpp` lists the five implemented
editor entries. Add an entry deliberately and update catalog tests; loading
arbitrary resource bundles does not automatically add an operational device to
the header. The original-resource loader registers every imported RCC, so it
does not require another hardcoded resource-loader branch for a new package.
For local-resource mode, the manifest must expose the matching resource path
`:/<model>/<model>_manifest.cuebmf`. Keep an authored definition for a clean build.

Use `Catalog::models`, `keys`, `lightingKeys`, `navigation` and `lightingTypes`
to expose metadata to the shared editors. Confirm raster bodies and masks have
matching dimensions and that selection coordinates identify actual LEDs.
Add no hardware-control buttons for capabilities whose backend is unavailable.
Partner-integration bundles are inventory entries, not Corsair HID devices.

## 3. Verify the protocol

The transport boundary is `src/devices/hid_transport.*`; model operations live
in `src/devices/hardware_backend.*`. Begin with a known read-only identity or
battery transaction. Verify report ID, length, endpoint, command, byte order,
status, timeout, stale-response handling and the firmware's reply shape.

Use observed USB traffic, narrowly decompiled call sites or a cited independent
implementation as evidence. A DLL constant alone does not establish framing.
Static inspection should occur in a separate analysis environment without
running Windows binaries. Keep large original artifacts outside the repo and
retain small authored, redacted reply fixtures where redistribution is allowed.

Add descriptor and identity checks before the first configuration write. Make
resource ownership explicit, bound reads/writes, serialize transactions and
restore software modes on shutdown/failure. Never probe unknown devices with
random commands or implement firmware writes just to see what happens.
Pairing needs exact device/receiver compatibility, key ownership, readback and
restart recovery; it is not a generic Bluetooth/USB pairing routine.

## 4. Implement Linux audio or key output separately

For a headset, identify the actual ALSA/PipeWire card and microphone controls.
Extend `src/audio/routing.*` only with verified identity checks and ownership
restoration. A new headset's RGB support does not make its audio routing work.
Do not route arbitrary system audio to a device guessed from a display name.

Assignments, macros, reactive input and onboard storage are distinct features.
Do not mark the assignment editor as hardware upload. Any future key replay
needs its own Linux input/output design, permissions and loop-prevention tests.

## 5. Test and document the acceptance boundary

Add fake-transport tests for normal replies, wrong identity, truncated packets,
status errors, stale replies, unplugging and restoration failure. Add GUI tests
for model selection/geometry and unsupported-state behavior. Run the clean
preset and the optional original-resource preset.

Then perform opt-in physical tests on the exact model. Observe per-zone LED
colors, battery response, real pointer behavior or audio positioning as
appropriate. Pairing must be retested after cable removal and process restart.
Report firmware and whether the result applies to wired, wireless or both.

Update `docs/compatibility.md` with one of these statuses per capability:

| Status | Meaning |
| --- | --- |
| Assets/editor only | Metadata and images can be inspected; no hardware write claim |
| Contract tested | Deterministic fixtures exercise the code |
| Transport verified | Exact device identity and expected reply/readback observed |
| Physically accepted | Device behavior observed or heard under the stated conditions |

Include the same boundary in the PR description. Do not promote the whole
product family after a successful test on one USB ID.
