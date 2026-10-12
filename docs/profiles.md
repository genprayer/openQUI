# Profiles and iCUE import

## Library and interface

[Profiles/](../Profiles/README.md) contains the reproducible official-gallery
catalog and local downloads. The left profile sidebar adds a profile below the
list. Left-click activates a complete profile document; right-click selects
that document and opens Rename, Duplicate and Delete. Import is in the sidebar
header. The final profile cannot be deleted. Duplicating copies all device
documents into independent state, including devices that are not selected.

This interaction was compared with the extracted `ProfilesLeftPanelContent`,
`SoftwareProfileCreateView` and `SoftwareProfileContextMenu` resources from
the supplied Windows iCUE 5.52.93 installation. The Linux QML is independently
implemented. Folder trees, default-profile assignment, application linking,
Windows-compatible export and onboard profile slots remain unimplemented.

## Conversion boundary

The C++ importer preserves the complete XML in the user's `imports/` directory,
named by SHA-256. It separately converts known software-lighting routes into
native device documents. One imported profile can contain keyboard, mouse and
headset settings; selecting another device does not create another profile.
Exact model routes take priority over shared routes. Receiver entries never
receive a peripheral lighting route. XML for another model is not relabeled
as supported hardware.

| Source effect | Current conversion |
| --- | --- |
| Continuous StaticLighting | Selected zones, RGBA and opacity |
| Continuous GradientLighting | Uneven transition times, RGBA, duration and loop |
| Continuous WaveLighting | Transition curve, selected zones, velocity, tail length, angle and two-sided flag; independent native spatial replay |
| Continuous ColorPulseLighting | Both ARGB colors, random flag, speed and duration |
| GradientLighting on a key, PlayOnKeyCenter, one iteration | Native Type effect on K70; samples the original curve using that key's press age |
| Other triggers, Rain, Ripple, audio-reactive or unknown effects | Preserved in XML; omitted from native playback with conversion warnings |

The wave replay is independently implemented using the model's extracted LED
geometry. Its visual timing has **not been calibrated against Windows iCUE**;
preserved parameters do not prove identical physical output. Conversion v2
reverses iCUE's top-first layer order for the native bottom-first compositor.
Unknown zones, such as macro keys absent on K70 CORE, are reported rather than
redirected to unrelated LEDs.

Import or profile selection requests live playback on **all connected supported
peripherals while a lighting editor is active**. Each device receives its own
converted layers; receiver entries receive no peripheral frames. Subsequent
edits update that device's document, and all profile routes share the animation
clock and serialized hardware worker. Ordinary per-device editing before a
profile activation remains scoped to the selected device. Leaving the lighting
editor releases software lighting control for the whole group. Persistent
onboard writes remain separate work.

Reimporting identical bytes with the same converter version selects the
existing document and preserves local edits. When the converter version
changes, an explicit reimport creates a new document and leaves the earlier
document untouched. It does not silently replace your lighting or assignments.

## Developer checks

The 2026-10-12 local snapshot was checked through the native importer with
hardware disabled: **82/82 files parsed; 80 contained at least one convertible
route** for the current three peripherals. `Mushroom Cloud` and `Rainbow Vertical`
contain no convertible route under the current effect/execution rules; their
original bytes remain available. This count does not mean full playback of 80
profiles.

`DOOM-TheDarkAges` converted into **10 K70, 4 Darkstar and 1 Virtuoso layers**.
Its K70 black background is below the waves and per-key overlay. Rain and other
unimplemented triggers remain in the source XML. The library audit and
deterministic three-device replay do not constitute new physical acceptance.

`src/profiles/cereal.cpp` parses and converts; `src/core/workspace.cpp` owns
atomic persistence and import identity; `qml/ProfileSidebar.qml` owns the list
and its menu. The normal controller and hardware compositor handle playback.

```sh
ctest --preset editor -R 'native-(core|hardware|gui|translations)'
./build/native/open-cue-gui --no-hardware --state-directory ./artifacts/profile-preview \
  --import-profile ./Profiles/downloads/doom-thedarkages--DOOM-TheDarkAges.cueprofile \
  --device k70_core_rgb --panel Lightings
```

The temporary state above avoids changing personal profiles. Inspect
`sourceMetadata.conversion` and every profile's `devices` object before
claiming complete import. GUI events, protocol acknowledgements and physical
LED behavior are different acceptance gates.
