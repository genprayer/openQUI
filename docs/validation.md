# Validation snapshot

Checked on 2026-10-12 during preparation of the OpenQUI source repository.

| Configuration | Toolchain / scope | Result |
| --- | --- | --- |
| Fedora 44, authored resources | GCC 16.2.1 / Qt 6.11.2; deterministic CTest groups | 9/9 passed |
| Debian 13, authored resources | GCC 14.2.0 / Qt 6.8.2; includes private PipeWire signal test | 10/10 passed |
| Debian 13, original local resources | All eight imported device/integration RCCs; same suites | 10/10 passed |
| Final Debian targeted checks | Core, translations, D-Bus and asset importer after test path fixes | 4/4 passed |
| Asset importer contracts | Six synthetic tests, no Windows execution | Passed |
| UI catalogs | 447 complete messages in German, English and Russian | Passed |
| C++ formatting | clang-format 19, committed style | Passed |
| Shell syntax, local documentation links and Git whitespace | Source maintenance checks | Passed |
| Fedora host preview | Offscreen software rendering, isolated D-Bus, no HID; existing profile path | Passed |

These checks do not establish additional hardware support. No live HID writes,
pairing operation or audible/spatial acceptance test was performed for this
repository preparation. Existing physical observations are identified separately
in [compatibility.md](compatibility.md). GitHub CI is configured but has not been
run on GitHub. Raw build/test logs and captures are kept in ignored `artifacts/`.

The asset index contains eight model/integration manifests and 2,697 local file
entries, including runtime aliases. It is an inventory of the supplied iCUE
5.52.93 snapshot, not the complete product catalog. Only the two public README
preview images are tracked; local vendor files and research artifacts are not.
