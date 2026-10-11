# Contributing to OpenQUI

Fork the repository, clone your fork and create a focused branch. Use the
native preset to establish a passing baseline before changing code. The default
build needs no Windows installation or proprietary resources.

```sh
git switch -c fix/describe-the-problem
cmake --preset native
cmake --build --preset native --parallel 2
ctest --preset native
```

Keep changes tied to an observable behavior. Describe the trigger, result and
test evidence in a pull request. Use Conventional Commits, for example
`fix(lighting): restore the previous color when the picker is cancelled`.
Separate functional fixes from broad formatting or architectural changes.

Read [development.md](docs/development.md) for module ownership and style,
[testing.md](docs/testing.md) for relevant test commands and
[adding-devices.md](docs/adding-devices.md) before proposing new hardware writes.
Public interfaces and modules need concise documentation. Update the support
matrix when a verified capability changes.

Code and comments are English. User interface messages use the existing German
source keys and English/German/Russian translation catalogs. Preserve device
model names. No Python is required to run the application.

Do not submit build trees, private Windows artifacts, user profiles, pairing
credentials, machine paths or hundreds of diagnostic screenshots. Keep test
output outside the source tree or in ignored `artifacts/`. Small authored
fixtures and the two public preview images are intentionally tracked.

Automated tests do not authorize a new device protocol. New USB IDs require
separate evidence. State which tests used fake exchanges, a real protocol
transport, physical LED observation or listening. Do not claim full iCUE parity
from a working editor or a manifest.
