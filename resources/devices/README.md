# Fallback device views

These authored SVG files are fallback illustrations for checkouts without
original iCUE bundles. They are not original Corsair renderings or 3D meshes.
The native/editor presets use original views automatically when the required
local bundles are present. The source-only preset deliberately uses these SVGs.

`open-catalog.json` contains independent editor metadata: keys, LED identifiers,
selection geometry, navigation and capabilities. It is needed for the fallback
mode. The SVG drawing is a background; it does not determine hardware LED order.
Original-resource mode reads the corresponding manifests/images/masks from
the RCC bundles instead.

Import and resource selection are described in [docs/assets.md](../../docs/assets.md).
