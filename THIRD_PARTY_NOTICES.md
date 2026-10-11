# Third-party notices

The MIT license covers OpenQUI's authored source, tests, device schematics,
keyboard layout, translations and original SVG icons. Dependencies retain
their own licenses; the source package does not bundle their libraries.

| Dependency | Use | License information |
| --- | --- | --- |
| Qt 6 | Core, Gui, Widgets, Quick, QuickControls2, Qml, DBus, Concurrent, Svg, Xml, Test and build tools | [Qt licensing](https://doc.qt.io/qt-6/licensing.html) |
| libudev | Read-only Linux USB inventory | System package copyright notices |
| ALSA libasound | Mixer control | System package copyright notices |
| PipeWire | External audio process and filter graph | System package copyright notices |

The app dynamically links system Qt libraries. Redistributors who bundle Qt
must satisfy the licenses of those Qt components and include the appropriate
notices, source availability and replacement/relinking provisions. The MIT
license for this project does not replace those obligations.

Corsair and iCUE are trademarks of their owners. This is an independent project
and is not affiliated with Corsair.

The optional local-resource build accepts files from a user's own Windows iCUE
installation. Those files, including RCC, fonts, images, factory EQ XML and
decompiled material, are **not covered by this project's MIT license** and are
excluded from the public source package. A license for our code does not grant
permission to redistribute them. The default CMake build uses OpenQUI's
independent schematics and starter EQ presets.

`resources/audio/kemar-7.1.wav` is derived from measurements by Bill Gardner
and Keith Martin, MIT Media Lab, copyright 1994 MIT Media Laboratory. Their
permission allows unrestricted use with author attribution. See
[the original dataset](https://sound.media.mit.edu/resources/KEMAR.html) and
`resources/audio/kemar-attribution.md` for provenance and reproduction.

The HID implementation is independently authored from observed device replies,
protocol constants and published references. ckb-next and OpenRGB are research
references, not linked or bundled drivers. V2W protocol behavior was cross-checked
against VirtuosoControl by Alejandro Socas (MIT, copyright 2026). Its repository
and all Windows research files stay outside this source distribution.
