# Optional iCUE resources

The application and independent drawings are tracked. Original resource bundles
remain local to the user's installation and are ignored by Git. Importing them
does not delete or replace existing files outside the importer-owned library.
The MIT license covers authored code; it does not relicense imported Corsair
images, fonts, sound prompts or manifests.

## Import all available packages

Set `icue_installation` to a copied Windows iCUE installation containing its
`devices/` directory. No Windows process is launched:

```sh
python3 tools/assets/import_library.py "$icue_installation" \
  --output resources/vendor/library
```

Most maintenance tools use only Python's standard library. A package compressed
with Zstandard additionally needs `zstandard`, preferably in a local virtual
environment. Python is not an application dependency.

If static resources have already been extracted from DLLs in a separate
analysis environment, supply their root:

```sh
python3 tools/assets/import_library.py "$icue_installation" \
  --extracted "$extracted_resources" --output resources/vendor/library
```

The extracted tree has layout
`<container>/registration-<offset>/<original-resource-path>`. The importer
accepts static images, fonts, audio and metadata; it does not copy decompiled
QML scripts or execute PE/DLL files. Original RCC bundles are retained as opaque
resource data; OpenQUI loads its own QML, not the bundled Windows interface.

Imports include every `devices/*.rcc` rather than a five-model list. Paths,
SHA-256 hashes and names are indexed without absolute host paths. Bounds reject
oversized/truncated/cyclic records, decompression inconsistencies and traversal.
Imports use a lock, a staged tree and recovery of the previous tree on interruption.
Repeated imports of the same input produce the same index. An invalid new
package leaves the prior completed library intact.

## Layout and use

| Local path | Contents |
| --- | --- |
| `resources/vendor/library/bundles/` | All source RCC packages and optional global bundle |
| `resources/vendor/library/assets/` | Static contents by source container and original path |
| `resources/vendor/library/runtime/` | Optional fonts, icons, factory EQ presets and verified-format voice data |
| `resources/vendor/library/index.json` | Per-file hashes, counts and model/integration metadata |

```sh
cmake --preset local-assets
cmake --build --preset local-assets --parallel 2
./build/local-assets/open-cue-gui
```

Only RCC packages and selected runtime resources are embedded in that build
using [Qt BIG_RESOURCES](https://doc.qt.io/qt-6/qt-add-resources.html);
the large expanded asset library is retained locally for inspection. The native
loader registers all imported device bundles. The editor/hardware catalog still
has explicit supported entries, described in [adding-devices.md](adding-devices.md).
Required editor manifests must be present for the local-assets preset; importing
only unrelated devices cannot replace them. Missing optional factory EQ data
uses authored presets. The default `native` preset remains reproducible without
vendor files.

## Actual catalog boundary

[asset-catalog.json](asset-catalog.json) summarizes the audited **iCUE 5.52.93**
installation snapshot. It includes five Corsair device bundles: DARKSTAR,
K70 CORE, VIRTUOSO and two receivers. GOVEE, NANOLEAF and PHILIPS HUE are partner
integration bundles, not three additional Corsair peripherals. Shared resources
also include cooling, memory, lighting-strip and tutorial images. Their presence
does not establish standalone product modules or driver support.

[Corsair explains that iCUE 5 downloads device modules as hardware is detected](https://www.corsair.com/uk/en/explorer/gamer/accessories/how-do-icue-modules-work/).
This snapshot does not contain every package supported by Windows iCUE. To
populate the entire asset library, supply the additional actual device modules
from an installation/archive that has them, then rerun the same importer. The
project does not invent missing pictures or silently declare a full catalog.
`isCompleteCorsairCatalog` stays false because the complete official product
list and package coverage have not been verified.

The resource inventory contains renders and masks, but no GLB, GLTF, FBX, OBJ,
DAE or mesh files. Keep original renderings locally; the missing 3D source data
cannot be reconstructed merely by renaming raster files.

The repository tracks two screenshots of the authored-resource build for the
README. Diagnostic captures belong in ignored `artifacts/` or the existing
private verification archive. New screenshots should illustrate a distinct
feature and stay small enough for a useful source checkout.
