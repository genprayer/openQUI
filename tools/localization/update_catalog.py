#!/usr/bin/env python3
"""Validate the authored language catalog and export reproducible Qt Linguist sources."""
from __future__ import annotations

import argparse
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
LANGUAGES = {"de": "de_DE", "en": "en_US", "ru": "ru_RU"}
PLACEHOLDER = re.compile(r"%(?:[1-9][0-9]*|n)")
TRANSLATION_CALL = re.compile(r'qsTranslate\(\s*"OpenICUE"\s*,\s*("(?:[^"\\]|\\.)*")')
NATIVE_ERROR = re.compile(
    r'throw std::(?:runtime_error|invalid_argument)\(\s*("(?:[^"\\]|\\.)*")\s*\)'
)


def validate_catalog(catalog: dict) -> None:
    """Reject incomplete languages and changed formatting arguments before compilation."""
    if catalog.get("schemaVersion") != 1 or catalog.get("context") != "OpenICUE":
        raise ValueError("Unsupported translation catalog schema")
    if set(catalog.get("languages", [])) != set(LANGUAGES):
        raise ValueError("The catalog must contain exactly de, en and ru")
    for source, values in catalog["messages"].items():
        if set(values) != set(LANGUAGES):
            raise ValueError(f"Incomplete languages for {source!r}")
        expected = sorted(PLACEHOLDER.findall(source))
        for language, translation in values.items():
            if not isinstance(translation, str) or not translation.strip():
                raise ValueError(f"Empty {language} translation for {source!r}")
            if sorted(PLACEHOLDER.findall(translation)) != expected:
                raise ValueError(f"Changed placeholders in {language}: {source!r}")


def qt_source(catalog: dict, language: str) -> bytes:
    """Serialize a complete Qt TS file without unfinished or vanished entries."""
    root = ET.Element("TS", version="2.1", language=LANGUAGES[language])
    context = ET.SubElement(root, "context")
    ET.SubElement(context, "name").text = catalog["context"]
    for source, values in sorted(catalog["messages"].items()):
        message = ET.SubElement(context, "message")
        ET.SubElement(message, "source").text = source
        ET.SubElement(message, "translation").text = values[language]
    ET.indent(root, space="  ")
    return ET.tostring(root, encoding="utf-8", xml_declaration=True) + b"\n"


def validate_qml_coverage(catalog: dict) -> None:
    """Check every literal application translation call against the authored catalog."""
    missing = []
    for file in (PROJECT_ROOT / "qml").glob("*.qml"):
        for literal in TRANSLATION_CALL.findall(file.read_text()):
            source = json.loads(literal)
            if source not in catalog["messages"]:
                missing.append(f"{file.name}: {source!r}")
    if missing:
        raise ValueError("Missing UI translations:\n" + "\n".join(missing))


def validate_native_error_coverage(catalog: dict) -> None:
    """Require translations for literal errors that the controller can display."""
    missing = []
    for file in (PROJECT_ROOT / "src").rglob("*.cpp"):
        for literal in NATIVE_ERROR.findall(file.read_text()):
            source = json.loads(literal)
            if source not in catalog["messages"]:
                missing.append(f"{file.relative_to(PROJECT_ROOT)}: {source!r}")
    if missing:
        raise ValueError("Missing native error translations:\n" + "\n".join(missing))


def main() -> None:
    """Write TS sources, or check that committed sources still match the catalog."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true", help="Validate without modifying files"
    )
    args = parser.parse_args()
    catalog = json.loads((PROJECT_ROOT / "translations/catalog.json").read_text())
    validate_catalog(catalog)
    validate_qml_coverage(catalog)
    validate_native_error_coverage(catalog)
    for language in LANGUAGES:
        file = PROJECT_ROOT / f"translations/open-icue_{language}.ts"
        content = qt_source(catalog, language)
        if args.check:
            if not file.exists() or file.read_bytes() != content:
                raise ValueError(f"Qt TS source is out of date: {file}")
        else:
            file.write_bytes(content)
    print(
        f"Translation catalog: {len(catalog['messages'])} complete messages in de/en/ru; coverage PASS"
    )


if __name__ == "__main__":
    main()
