# Validation snapshot

Checked on 2026-10-12–13 during preparation and local installation of OpenQUI.

| Configuration | Toolchain / scope | Result |
| --- | --- | --- |
| Fedora 44, authored resources | GCC 16.2.1 / Qt 6.11.2; deterministic CTest groups | 9/9 passed |
| Debian 13, authored resources | GCC 14.2.0 / Qt 6.8.2; includes private PipeWire signal test | 10/10 passed |
| Debian 13, original local resources | All eight imported device/integration RCCs; same suites | 10/10 passed |
| Final original-resource profile changes | Sidebar input, three-device replay, importer v2, wave/per-key curves, translations | 10/10 passed |
| Native rain/ripple changes | Original-resource build; GUI edits, RGB payloads, K70 trigger/history and import v3 | 10/10 passed |
| Source-only rain/ripple checks | Fallback resources; core, hardware, keyboard input and translations, then complete GUI suite after correcting transient-frame assertions | 5/5 groups passed across the two runs |
| Official-gallery library | 82 publisher files downloaded and SHA-256 verified; native import v3 with HID disabled | 82 parsed; 81 with convertible layers |
| Final Debian targeted checks | Core, translations, D-Bus and asset importer after test path fixes | 4/4 passed |
| Asset importer contracts | Six synthetic tests, no Windows execution | Passed |
| UI catalogs | 455 complete messages in German, English and Russian | Passed |
| C++ formatting | clang-format 19, committed style | Passed |
| Shell syntax, local documentation links and Git whitespace | Source maintenance checks | Passed |
| Fedora host preview | Offscreen software rendering, isolated D-Bus, no HID; existing profile path | Passed |

The deterministic checks do not establish additional hardware support. Physical
observations are recorded below and in [compatibility.md](compatibility.md).
No pairing operation or audible/spatial acceptance test was performed in this
profile-replay work. GitHub CI is configured but has not been run on GitHub.
Raw build/test logs and captures are kept in ignored `artifacts/`.

The asset index contains eight model/integration manifests and 2,697 local file
entries, including runtime aliases. It is an inventory of the supplied iCUE
5.52.93 snapshot, not the complete product catalog. Only the two public README
preview images are tracked; local vendor files and research artifacts are not.

## Current profile/resource checks

The `native` and `editor` presets selected local original resources through
`AUTO`. A copied source checkout without `resources/vendor/` selected fallback
resources through the same `AUTO` setting and passed core, hardware,
translations and GUI (4/4). Explicit `ON` with missing bundles failed during
configuration. This checks both distribution modes rather than treating a
local-asset build as proof of a clean checkout.

Profile tests include independent multi-device duplication, safe reimport
across converter versions, layer order, recorded wave parameters, per-key
transition sampling and three different device RGB payloads in one active
profile. Hardware tests also cover keeping keyboard press state between
frames for other devices. Rain is deterministic and respects selected zones;
ripple samples preserved curves and uses bounded press history, including two
overlapping presses of one key. GUI tests exercise rain palette/opacity edits
and Type/Ripple key reports through outgoing RGB payloads. No physical HID was
opened by these deterministic tests.

## Local installation and physical observations

The tested native binary was installed as `~/.local/bin/open-cue-gui` and the
previous process was stopped after releasing its lighting sessions. The two
existing profile records remained identical. Explicitly reimporting the same
official DOOM bytes created a separate converter-v3 profile with 13 K70 layers,
4 Darkstar layers and 1 VIRTUOSO layer; the old profile was retained.

On the Fedora KDE/Wayland desktop, all three peripherals acknowledged RGB
frames. VIRTUOSO, switched to 2.4 GHz, answered battery queries with 24% charge
after the first restart. A later poll again timed out on the headset query;
receiver RGB acknowledgements alone do not prove a current radio link. The
timeout's cause remains unverified. The user
confirmed changing illumination on both K70 and VIRTUOSO and keyboard flashes
on physical key presses. Exact animation equivalence with Windows remains
unverified, including imported rain direction and spatial timing.

The legacy `open-icue.desktop` and executable were backed up outside the source
checkout. Only the OpenQUI desktop entry remains; the old command name forwards
to the new executable for existing launchers. This local migration is separate
from the install script's normal file replacement.

## VS Code environment

The development container was created with the installed Dev Containers CLI.
The `editor` preset configured successfully, the `open-cue-gui` target built,
and the existing asset-import suite passed (1/1). The generated compilation
database resolves Qt headers and source files within the container. All three
Qt TS catalogs parsed as XML. These checks validate the environment; the live
VS Code Problems panel still requires opening this project in that container.

The supplied VS Code task definitions were subsequently executed in dependency
order: Configure, Build, Test and Translations. The editor preset passed all
10 CTest groups; the 447-message language catalog passed. This verifies the
commands from tasks.json, not a replay of VS Code keyboard shortcuts. The
Russian guide and local documentation links were also checked.
