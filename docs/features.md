# Profiles, lighting and active DPI

OpenQUI 0.6.0 keeps local profile changes, device observations and acknowledged
hardware operations separate. A saved profile or a successful import does not mean
the device has received those settings. Support currently covers three peripheral
models: K70 CORE RGB, Darkstar Wireless and Virtuoso.

## Live lighting

Layer, palette, effect, duration and zone edits stream while the selected device's
Lighting Effects page is active. The color picker sends changes before OK;
Cancel restores the previous color. Leaving the page releases owned control.

On K70 CORE, Type lighting reads its descriptor-verified keyboard HID interface.
A newly pressed selected key lights up and fades over the layer's duration.
Repeated reports while a key is held do not restart the effect. Custom colors
or a color selected for each press blend over continuous layers. The device image
uses the acknowledged outgoing frame for its preview; this is not LED readback.
Only the selected K70 is read, without input grabbing or persistent key records.
Other reactive effects and the audio visualizer are not implemented.

## Importing iCUE profiles

Use Import in the profile menu, or run:

```bash
open-cue-gui --no-hardware --import-profile /path/to/profile.cueprofile --dump-state
```

The C++ Cereal importer preserves the original bytes and their SHA-256. It converts
enabled static layers and continuously repeating gradients for matching device
categories and supported USB IDs. Generic keyboard, mouse and headset routes can
map to the corresponding supported model. Receiver entries are not lighting targets.
Known keys and LEDs retain their zones; unknown zones are reported and skipped.

Conversion retains static color/opacity and gradient duration, color alpha and
normalized transition times. The native preview and hardware frame renderer use
the same interpolation. Palette edits update imported transition colors; interior
transition times are editable. Changing to another effect type removes the previous
timeline. The first and last times remain 0% and 100%.

The conversion summary is stored under the imported profile's
`sourceMetadata.conversion` in the workspace. It includes converted/preserved
source layer counts, target device count, unknown zones and warning codes.
Original files can contain profiles for hundreds of devices; the source count
includes all those routes. It is not a
count of effect types supported by the renderer.

Triggered playback, unsupported effects and assignments remain in the preserved
XML. Gradients without an explicit complete continuous timeline are not guessed.
The importer reports partial conversion rather than silently turning such content
into a different effect. Reimporting the same bytes does not overwrite edited
native layers. Importing from a headless or inactive editor performs no HID writes.
With Lighting Effects active, the selected profile's supported native layers use
the live RGB sender just like manual layer edits.

## Applying Darkstar DPI

Connect Darkstar by USB cable and open DPI. The application checks the command
interface descriptor, firmware VID/PID and readable DPI properties before enabling
Apply active DPI. Select an enabled stage, edit its value, then press the button.
Editing a stage alone changes the local profile.

The transaction reads the previous active DPI and stage index, enters software
mode, writes the unified active DPI property and checks both the acknowledgement
and exact readback. It also checks that the active stage index did not change.
The model range is 100–26000. Darkstar does not advertise independent X/Y axes;
unequal axes are rejected before a write.

On failure after a write attempt, the backend tries to restore the previous DPI
and reports any rollback error. Temporary sessions also restore their previous
control mode. Successful control shares one exclusive HID session with software
RGB. End software control releases that session and checks the firmware mode
readback; restoration errors are reported as errors.

Only the current software DPI value is applied. Onboard stage tables, stage colors
and Sniper storage are not written. A guarded receiver-child route exists in the
backend and its tests, but the GUI currently enables DPI only for a verified wired
Darkstar. Physical acceptance of DPI is pending. An acknowledgement and readback
test using a simulated HID peer does not prove mouse movement on real hardware.

Session D-Bus exposes `applyDpiSettings()` on `org.kde.cyberdeck.icue` at
`/org/kde/cyberdeck/icue`. It returns a pending operation when scheduled; clients
must observe `GetState`/state changes for the completed result. `dpiState` contains
the acknowledged DPI value and `isDpiActive`, separate from the local stage editor.

## Remaining execution gaps

System-wide key replay and macros, onboard profiles, firmware updates, mouse
surface/tilt calibration, polling-rate changes and full SLIPSTREAM Multipoint are
not implemented. The fifteen-button assignment editor and F1–F24 key selector do
not yet execute those assignments globally. Existing RGB, pairing, battery and
PipeWire paths have their own validation boundaries in [compatibility](compatibility.md).

## Live editing in 0.6.0

On the lighting page, layer, palette, effect, timing and zone changes update the
selected connected device automatically. The sender coalesces edits while a HID
transaction is in flight and reports acknowledgements separately from profile
edits. Leaving that page stops the sender and restores owned firmware control.
Removing the last layer sends a black frame. Color Picker previews also update the
originating swatch and hardware before OK; Cancel restores the original color.
Triggered effects still need their corresponding input/audio source implementation.

The headset's Stereo / 7.1 Sound button switches directly. Physical GUI audio
routing accepts only the selected Virtuoso's exact USB/ALSA identity. It creates
and selects the EQ sink automatically, moves playback streams already linked to
that headset, and restores metadata only while it remains owned by this app.
Other physical outputs cannot become the filter's fallback target.
Original Surround On/Off recordings are available with locally supplied resources;
source-only builds do not include those files.

Device and receiver connection pages show only their own USB interfaces. Darkstar
pairing is exposed for Darkstar and its supported 1bdc receiver, with the existing
wired-device and identity checks. Selecting the Virtuoso receiver does not expose
a Darkstar pairing operation.
