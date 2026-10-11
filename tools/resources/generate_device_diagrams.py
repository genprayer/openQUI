#!/usr/bin/env python3
"""Generate original device schematics and an independently authored UI catalog."""
from __future__ import annotations

import json
from pathlib import Path
from xml.sax.saxutils import escape

PROJECT_ROOT = Path(__file__).resolve().parents[2]
OUTPUT = PROJECT_ROOT / "resources/devices"
KEYBOARD = PROJECT_ROOT / "resources/keyboard-layouts/de-iso.json"
LIGHTING_TYPES = [
    "Rainbow",
    "ColorPulse",
    "ColorShift",
    "StaticCustom",
    "Solid",
    "Gradient",
    "RainbowWave",
    "SpiralRainbow",
    "Type",
    "Wave",
    "Visor",
    "Rain",
    "ColorWave",
    "Ripple",
]
MOUSE_INPUTS = [
    "Mouse1",
    "Mouse2",
    "Mouse3",
    "MouseWheelTiltLeft",
    "MouseWheelTiltRight",
    "Mouse6",
    "Mouse7",
    "MouseProfileUp",
    "MouseProfileDown",
    "Mouse4",
    "Mouse5",
    "MouseSideFront1",
    "MouseSideFront2",
    "MouseSideBack1",
    "MouseSideBack2",
]


def svg(width: int, height: int, content: str) -> str:
    """Wrap original vector content without external fonts or image dependencies."""
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">\n'
        '<defs><linearGradient id="body" x2="0.7" y2="1">'
        '<stop stop-color="#424955"/><stop offset="1" stop-color="#171b23"/>'
        "</linearGradient></defs>\n" + content + "\n</svg>\n"
    )


def add_target(
    definition: dict,
    key: str,
    label: str,
    rectangle: tuple,
    view: int = 0,
    is_led: bool = True,
) -> None:
    """Create schematic contours from authored rectangles, not vendor masks."""
    x, y, width, height = rectangle
    shapes = definition["SvgStaticGeometryMap"]["RegularStatic"]
    points = definition["StaticGeometryMap"]["RegularStatic"]
    geometry = key + "-shape"
    shapes[geometry] = {
        "type": "rectangle",
        "x": x,
        "y": y,
        "width": width,
        "height": height,
        "rx": 6,
        "ry": 6,
    }
    points[geometry] = [
        {"x": x, "y": y},
        {"x": x + width, "y": y},
        {"x": x + width, "y": y + height},
        {"x": x, "y": y + height},
    ]
    indices = "," + geometry if view else geometry
    kinds = ["Key", "Led"] if is_led else ["Key"]
    for kind in kinds:
        definition["Layouts"]["US"]["keyItems"].append(
            {
                "id": key,
                "displayName": label,
                "type": kind,
                "viewIndex": view,
                "geometryIndex": {"Black": indices},
            }
        )


def base_definition(
    model: str,
    name: str,
    kind: str,
    pids: list[int],
    pages: list[str],
    width: int,
    height: int,
    views: int = 1,
) -> dict:
    """Describe known USB identities separately from unimplemented hardware control."""
    images = [f":/app/devices/{model}-{view}.svg" for view in range(views)]
    return {
        "UserModelName": name,
        "DeviceType": kind,
        "UsbIds": [{"vid": 0x1B1C, "pid": pid} for pid in pids],
        "SubScreensListData": [{"type": page} for page in pages],
        "DeviceIcon": {"Black": images[0]},
        "ImageSize": {"width": width, "height": height},
        "Layouts": {
            "US": {"layoutImages": {"default": {"Black": images}}, "keyItems": []}
        },
        "StaticGeometryMap": {"RegularStatic": {}},
        "SvgStaticGeometryMap": {"RegularStatic": {}},
        "SupportedLightingTypes": {
            "LocalPreview": {
                "Software": [
                    key + "LightingTypeData.lightings.cue.corsair.com"
                    for key in LIGHTING_TYPES
                ]
            }
        },
    }


def keyboard_definition() -> dict:
    """Draw a standard ISO keyboard using the application's own key-layout data."""
    model = "k70_core_rgb"
    definition = base_definition(
        model,
        "K70 CORE RGB",
        "Keyboard",
        [0x1BFD],
        ["Lightings", "Assignments"],
        1400,
        440,
    )
    layout = json.loads(KEYBOARD.read_text())
    drawing = [
        '<rect x="12" y="12" width="1376" height="416" rx="18" fill="url(#body)" stroke="#667080"/>'
    ]
    for group, left, top in [
        ("mainRows", 30, 25),
        ("navigationRows", 960, 25),
        ("numpadRows", 1160, 88),
    ]:
        for row_index, row in enumerate(layout[group]):
            x = left
            for key in row:
                units = float(key.get("units", 1))
                width = units * 54 - 5
                y = top + row_index * 63
                add_target(definition, key["id"], key["label"], (x, y, width, 50))
                drawing.append(
                    f'<rect x="{x}" y="{y}" width="{width}" height="50" rx="6" fill="#252b34" stroke="#707a8c"/>'
                )
                drawing.append(
                    f'<text x="{x + width / 2}" y="{y + 30}" text-anchor="middle" fill="#dde7f2" font-family="sans-serif" font-size="13">{escape(key["label"])}</text>'
                )
                x += units * 54
    (OUTPUT / f"{model}-0.svg").write_text(svg(1400, 440, "\n".join(drawing)))
    return definition


def mouse_definition() -> dict:
    """Draw two schematic views with all fifteen independent physical inputs."""
    model = "darkstar_wireless"
    definition = base_definition(
        model,
        "DARKSTAR WIRELESS",
        "Mouse",
        [0x1BB2],
        ["Lightings", "Assignments", "Gestures", "Dpi", "SurfaceCalibration"],
        640,
        360,
        2,
    )
    definition.update(
        {
            "MinDpi": 100,
            "MaxDpi": 26000,
            "MaxSupportedPollingRates": {"defaultDevice": "PR_1000Hz"},
            "Performances": [
                {
                    "defaultDpi": {
                        "settings": {
                            key: {
                                "dpiX": value,
                                "dpiY": value,
                                "independentXY": False,
                                "enabled": True,
                                "color": "#00bfff",
                            }
                            for key, value in zip(
                                ["Dpi1", "Dpi2", "Dpi3", "Dpi4", "Dpi5", "SniperDpi"],
                                [400, 800, 1200, 1600, 3200, 400],
                            )
                        }
                    }
                }
            ],
        }
    )
    views = [
        [
            '<path d="M180 40 Q320 5 460 40 Q550 180 470 295 Q320 355 170 295 Q90 180 180 40Z" fill="url(#body)" stroke="#727d90" stroke-width="3"/>'
        ],
        [
            '<path d="M75 210 Q115 70 375 85 Q510 90 565 200 L540 270 H100Z" fill="url(#body)" stroke="#727d90" stroke-width="3"/>'
        ],
    ]
    rectangles = [
        (180, 55, 90, 115),
        (370, 55, 90, 115),
        (305, 65, 30, 62),
        (275, 82, 24, 35),
        (345, 82, 24, 35),
        (300, 145, 40, 24),
        (300, 177, 40, 24),
        (300, 222, 40, 24),
        (300, 254, 40, 24),
    ]
    for index, key in enumerate(MOUSE_INPUTS):
        view = 0 if index < len(rectangles) else 1
        rectangle = (
            rectangles[index]
            if not view
            else (125 + (index - 9) % 3 * 125, 142 + (index - 9) // 3 * 63, 104, 44)
        )
        add_target(definition, key, "", rectangle, view, False)
        x, y, width, height = rectangle
        views[view].append(
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="6" fill="#242b36" stroke="#738498"/>'
        )
        views[view].append(
            f'<text x="{x + width / 2}" y="{y + height / 2 + 4}" text-anchor="middle" fill="#d2e6f5" font-family="sans-serif" font-size="11">{index + 1}</text>'
        )
    for key in [
        "Gesture_TiltLeft",
        "Gesture_TiltRight",
        "Gesture_TiltForward",
        "Gesture_TiltBackward",
    ]:
        definition["Layouts"]["US"]["keyItems"].append(
            {"id": key, "type": "Key", "geometryIndex": {"Black": ""}}
        )
    leds = [
        ("MouseLogoLed", 0, (235, 282, 150, 14)),
        ("MouseScrollLed", 0, (307, 60, 26, 9)),
        ("MouseFrontLeft", 0, (155, 285, 32, 12)),
        ("MouseFrontRight", 0, (440, 285, 32, 12)),
        *[
            (f"MouseTopPalmGlow{index + 3}Led", 0, (180 + index * 80, 316, 55, 12))
            for index in range(4)
        ],
        *[
            (f"MouseDpiIndicator{index + 1}", 1, (145 + index * 32, 113, 18, 12))
            for index in range(3)
        ],
        ("MouseTopPalmGlow1Led", 1, (375, 113, 55, 12)),
        ("MouseTopPalmGlow2Led", 1, (445, 113, 55, 12)),
    ]
    for key, view, rectangle in leds:
        add_target(definition, key, "", rectangle, view)
        definition["Layouts"]["US"]["keyItems"] = [
            entry
            for entry in definition["Layouts"]["US"]["keyItems"]
            if not (entry["id"] == key and entry["type"] == "Key")
        ]
        x, y, width, height = rectangle
        views[view].append(
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="4" fill="#477b86" stroke="#74d5e9"/>'
        )
    definition["TiltGesturesCapabilities"] = {
        "horizontalSetupTiltImage": ":/app/devices/darkstar_wireless-0.svg",
        "verticalSetupTiltImage": ":/app/devices/darkstar_wireless-1.svg",
    }
    for view, drawing in enumerate(views):
        (OUTPUT / f"{model}-{view}.svg").write_text(svg(640, 360, "\n".join(drawing)))
    return definition


def headset_definition() -> dict:
    """Draw an original headset diagram; do not bundle proprietary EQ presets."""
    model = "virtuoso_black"
    definition = base_definition(
        model,
        "VIRTUOSO",
        "Headset",
        [0x0A41],
        ["Lightings", "Assignments", "Equalizer"],
        640,
        360,
    )
    drawing = '<path d="M145 220 V160 C145 -15 495 -15 495 160 V220" fill="none" stroke="#5f6e83" stroke-width="34"/>'
    drawing += (
        '<path d="M145 190 V260 M495 190 V260" stroke="#a6b4c5" stroke-width="12"/>'
    )
    for x in (120, 465):
        drawing += f'<rect x="{x}" y="170" width="70" height="145" rx="25" fill="url(#body)" stroke="#62c9e3" stroke-width="3"/>'
    drawing += '<path d="M135 270 Q80 295 75 325 H180" fill="none" stroke="#677d93" stroke-width="8" stroke-linecap="round"/>'
    add_target(definition, "Headset_Mic", "", (65, 285, 120, 50), is_led=False)
    add_target(definition, "Led_HeadsetLogo", "", (135, 205, 35, 50))
    add_target(definition, "Led_HeadsetStatus", "", (480, 205, 35, 50))
    for key in ("Led_HeadsetLogo", "Led_HeadsetStatus"):
        definition["Layouts"]["US"]["keyItems"] = [
            entry
            for entry in definition["Layouts"]["US"]["keyItems"]
            if not (entry["id"] == key and entry["type"] == "Key")
        ]
    (OUTPUT / f"{model}-0.svg").write_text(svg(640, 360, drawing))
    return definition


def receiver_definition(model: str, name: str, pids: list[int]) -> dict:
    """Provide a USB receiver schematic without claiming RF telemetry or pairing."""
    definition = base_definition(model, name, "Dongle", pids, ["Connections"], 320, 360)
    drawing = '<rect x="110" y="50" width="100" height="130" rx="10" fill="url(#body)" stroke="#647d94" stroke-width="3"/>'
    drawing += '<rect x="121" y="180" width="78" height="120" fill="#aab5c4"/>'
    drawing += (
        '<path d="M140 204 H151 V225 H140Z M169 204 H180 V225 H169Z" fill="#333d4d"/>'
    )
    drawing += '<circle cx="160" cy="110" r="23" fill="none" stroke="#54bfdf" stroke-width="5"/>'
    (OUTPUT / f"{model}-0.svg").write_text(svg(320, 360, drawing))
    return definition


def main() -> None:
    """Write reproducible application assets without reading any Corsair installation."""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    definitions = {
        "slipstream_wireless_dongle_ss": receiver_definition(
            "slipstream_wireless_dongle_ss",
            "SLIPSTREAM WIRELESS USB Receiver",
            [0x1BDC],
        ),
        "virtuoso_black_dongle": receiver_definition(
            "virtuoso_black_dongle", "VIRTUOSO WIRELESS RECEIVER", [0x0A42, 0x0A4A]
        ),
        "darkstar_wireless": mouse_definition(),
        "virtuoso_black": headset_definition(),
        "k70_core_rgb": keyboard_definition(),
    }
    presets = [
        ("flat", "Flat", [0] * 10),
        ("bass", "Bass Boost", [3, 3, 2, 1, 0, 0, 0, 0, 0, 0]),
        ("voice", "Voice Focus", [-2, -2, -1, 0, 1, 2, 2, 1, 0, 0]),
        ("bright", "Bright", [0, 0, 0, 0, 0, 0, 1, 2, 2, 1]),
        ("balanced", "Balanced", [1, 1, 0, 0, 0, 0, 0, 0, 1, 1]),
    ]
    catalog = {
        "schemaVersion": 1,
        "models": definitions,
        "eqPresets": [
            {"id": identity, "name": name, "isFactory": True, "gains": gains}
            for identity, name, gains in presets
        ],
    }
    (OUTPUT / "open-catalog.json").write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"
    )


if __name__ == "__main__":
    main()
