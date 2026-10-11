#!/usr/bin/env python3
"""Import every available iCUE RCC and static asset without running Windows code."""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import shutil
import struct
import tempfile
import zlib
from pathlib import Path

MAX_BUNDLE_BYTES = 512 * 1024 * 1024
MAX_FILE_BYTES = 256 * 1024 * 1024
MAX_LIBRARY_BYTES = 2 * 1024 * 1024 * 1024
MAX_ENTRIES = 100_000
RUNTIME_FONTS = frozenset({"OpenSans-Regular.ttf", "OpenSans-SemiBold.ttf", "OpenSans-Bold.ttf"})
ASSET_EXTENSIONS = frozenset({
    ".png", ".jpg", ".jpeg", ".svg", ".bmp", ".ico", ".gif",
    ".ttf", ".otf", ".wav", ".pcm", ".xml", ".json", ".cuebmf",
    ".glb", ".gltf", ".obj", ".fbx", ".mesh", ".dae",
})


def checked_name(name: str) -> str:
    """Reject resource names that could escape the destination directory."""
    if not name or name in {".", ".."} or any(c in name for c in "/\\\x00"):
        raise ValueError("Invalid resource name")
    return name


def read_bounded(path: Path, limit: int = MAX_FILE_BYTES) -> bytes:
    """Read a regular file without following symlinks or allocating beyond the limit."""
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Not a regular input file: {path.name}")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f"Input exceeds size limit: {path.name}")
    return data


def decompress(payload: bytes, flags: int) -> bytes:
    """Decode the public Qt compression flags with a decompressed size bound."""
    if flags & 1:
        if len(payload) < 4:
            raise ValueError("Truncated zlib resource")
        expected = struct.unpack_from(">I", payload)[0]
        if expected > MAX_FILE_BYTES:
            raise ValueError("Oversized zlib resource")
        decoder = zlib.decompressobj()
        result = decoder.decompress(payload[4:], expected + 1)
        if len(result) != expected or not decoder.eof or decoder.unused_data:
            raise ValueError("Invalid zlib resource")
        return result
    if flags & 4:
        try:
            import zstandard
        except ImportError as error:
            raise ValueError("This RCC needs the optional Python zstandard package") from error
        try:
            with zstandard.ZstdDecompressor().stream_reader(payload) as stream:
                result = stream.read(MAX_FILE_BYTES + 1)
        except zstandard.ZstdError as error:
            raise ValueError("Invalid zstd resource") from error
        if len(result) > MAX_FILE_BYTES:
            raise ValueError("Oversized zstd resource")
        return result
    return payload


def resource_files(raw: bytes):
    """Yield named RCC v1-v3 payloads, rejecting invalid offsets and cyclic trees."""
    if len(raw) < 20 or raw[:4] != b"qres":
        raise ValueError("Input is not a Qt RCC")
    version, tree_offset, data_offset, names_offset = struct.unpack_from(">IIII", raw, 4)
    if version not in {1, 2, 3}:
        raise ValueError("Unsupported RCC version")
    if any(offset < 20 or offset >= len(raw) for offset in (tree_offset, data_offset, names_offset)):
        raise ValueError("Invalid RCC section offset")
    entry_size = 14 if version == 1 else 22
    visited: set[int] = set()
    paths: set[str] = set()
    pending = [(0, "")]
    decoded_bytes = 0
    while pending:
        index, prefix = pending.pop()
        if index in visited or len(visited) >= MAX_ENTRIES:
            raise ValueError("Cyclic or oversized RCC tree")
        visited.add(index)
        offset = tree_offset + index * entry_size
        if offset + entry_size > len(raw):
            raise ValueError("Truncated RCC tree")
        name_offset, flags = struct.unpack_from(">IH", raw, offset)
        if index:
            name_position = names_offset + name_offset
            if name_position + 6 > len(raw):
                raise ValueError("Invalid RCC name offset")
            length = struct.unpack_from(">H", raw, name_position)[0]
            end = name_position + 6 + length * 2
            if end > len(raw):
                raise ValueError("Truncated RCC name")
            name = checked_name(raw[name_position + 6:end].decode("utf-16-be"))
            prefix = f"{prefix}/{name}" if prefix else name
        if flags & 2:
            count, first = struct.unpack_from(">II", raw, offset + 6)
            if count > MAX_ENTRIES or tree_offset + (first + count) * entry_size > len(raw):
                raise ValueError("Invalid RCC directory")
            pending.extend((child, prefix) for child in reversed(range(first, first + count)))
            continue
        data_position = data_offset + struct.unpack_from(">I", raw, offset + 10)[0]
        if data_position + 4 > len(raw):
            raise ValueError("Invalid RCC data offset")
        size = struct.unpack_from(">I", raw, data_position)[0]
        if size > MAX_FILE_BYTES or data_position + 4 + size > len(raw):
            raise ValueError("Invalid RCC payload size")
        payload = decompress(raw[data_position + 4:data_position + 4 + size], flags)
        decoded_bytes += len(payload)
        if decoded_bytes > MAX_LIBRARY_BYTES or prefix in paths or not prefix:
            raise ValueError("Oversized RCC or duplicate resource path")
        paths.add(prefix)
        yield prefix, payload


class LibraryWriter:
    """Write static assets and portable provenance, independently of hardware support."""

    def __init__(self, root: Path):
        self.root = root
        self.files: dict[str, dict] = {}
        self.models: dict[str, dict] = {}
        self.total_bytes = 0

    def add(self, path: Path, payload: bytes, container: str) -> None:
        """Keep deterministic entries and reject conflicting destination paths."""
        for part in path.parts:
            checked_name(part)
        name = path.as_posix()
        digest = hashlib.sha256(payload).hexdigest()
        if name in self.files:
            if self.files[name]["sha256"] != digest:
                raise ValueError(f"Conflicting asset: {name}")
            return
        self.total_bytes += len(payload)
        if self.total_bytes > MAX_LIBRARY_BYTES or len(self.files) >= MAX_ENTRIES:
            raise ValueError("Library exceeds configured limits")
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)
        self.files[name] = {"path": name, "container": container, "bytes": len(payload), "sha256": digest}
        if path.name.endswith("_manifest.cuebmf"):
            common = json.loads(payload).get("common", {})
            if not isinstance(common, dict):
                raise ValueError("Invalid device manifest")
            if common.get("DeviceType", "Unknown") == "Unknown":
                return
            model_id = path.name.removesuffix("_manifest.cuebmf")
            self.models[model_id] = {
                "id": model_id, "name": common.get("UserModelName", model_id),
                "kind": common.get("DeviceType", "Unknown"), "usbIds": common.get("UsbIds", []),
                "manifest": name, "container": container,
                "hardwareSupport": "See docs/compatibility.md; assets do not enable HID commands",
            }

    def import_bundle(self, source: Path) -> None:
        """Retain the bundle and index every static resource it actually contains."""
        raw = read_bounded(source, MAX_BUNDLE_BYTES)
        self.add(Path("bundles") / source.name, raw, source.name)
        for name, payload in resource_files(raw):
            if Path(name).suffix.lower() in ASSET_EXTENSIONS:
                self.add(Path("assets") / source.name / name, payload, source.name)

    def import_extracted(self, source: Path) -> None:
        """Accept static DLL resources already extracted by a separate analysis tool."""
        for container in sorted(source.iterdir()):
            if not container.is_dir() or container.is_symlink():
                continue
            for registration in sorted(container.glob("registration-*")):
                if not registration.is_dir() or registration.is_symlink():
                    continue
                for path in sorted(registration.rglob("*")):
                    if path.is_symlink():
                        raise ValueError("Symlink in extracted resource tree")
                    if not path.is_file() or path.suffix.lower() not in ASSET_EXTENSIONS:
                        continue
                    relative = path.relative_to(registration)
                    self.add(Path("assets") / container.name / relative, read_bounded(path), container.name)
                    parts = relative.parts
                    if parts[0] == "fonts" and path.name in RUNTIME_FONTS:
                        self.add(Path("runtime") / relative, read_bounded(path), container.name)
                    elif parts[:2] == ("cue-icons", "4.0"):
                        self.add(Path("runtime/cue-icons") / Path(*parts[2:]), read_bounded(path), container.name)

    def import_runtime_data(self, installation: Path) -> None:
        """Keep optional factory presets and opaque voice payloads in the local library."""
        presets = installation / "predefined_data/defaultUnlimitedEqualizerPresets"
        if presets.exists():
            self.add(Path("runtime/equalizer-presets.xml"), read_bounded(presets), "predefined_data")
        for name in ("SurroundOn", "SurroundOff"):
            source = installation / "sounds/default" / f"{name}.wav"
            if not source.exists():
                continue
            payload = read_bounded(source)
            # The verified snapshot uses raw PCM despite the WAV suffix. Do not reinterpret RIFF files.
            if not payload.startswith(b"RIFF"):
                self.add(Path("runtime/voice") / f"{name}.pcm", payload, "sounds/default")

    def index(self) -> dict:
        """Return reproducible metadata without host paths, timestamps or credentials."""
        return {
            "schemaVersion": 1, "isCompleteCorsairCatalog": False,
            "scope": "All resources present in the supplied installation snapshot",
            "fileCount": len(self.files), "bytes": self.total_bytes,
            "models": [self.models[key] for key in sorted(self.models)],
            "files": [self.files[key] for key in sorted(self.files)],
        }


def import_library(installation: Path, output: Path, extracted: Path | None = None) -> dict:
    """Build a complete replacement in staging; preserve the previous import on failure."""
    bundles = sorted((installation / "devices").glob("*.rcc"))
    if not bundles:
        raise ValueError("No device RCC packages found in installation/devices")
    if output.is_symlink():
        raise ValueError("Output must not be a symlink")
    output.parent.mkdir(parents=True, exist_ok=True)
    with (output.parent / f".{output.name}.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        backup = output.parent / f".{output.name}.previous"
        if backup.exists():
            if not output.exists():
                backup.rename(output)
            else:
                shutil.rmtree(backup)
        with tempfile.TemporaryDirectory(prefix=f".{output.name}-", dir=output.parent) as temporary:
            stage = Path(temporary) / "library"
            writer = LibraryWriter(stage)
            for bundle in bundles:
                writer.import_bundle(bundle)
            global_bundle = installation / "cueres.bin"
            if global_bundle.exists():
                writer.import_bundle(global_bundle)
            if extracted is not None:
                writer.import_extracted(extracted)
            writer.import_runtime_data(installation)
            index = writer.index()
            (stage / "index.json").write_text(json.dumps(index, ensure_ascii=False, indent=2) + "\n")
            if output.exists():
                output.rename(backup)
            stage.rename(output)
        if backup.exists():
            shutil.rmtree(backup)
        return index


def main() -> None:
    """Import a user-supplied installation; print counts rather than binary payloads."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installation", type=Path)
    parser.add_argument("--output", type=Path, default=Path("resources/vendor/library"))
    parser.add_argument("--extracted", type=Path)
    args = parser.parse_args()
    try:
        index = import_library(args.installation, args.output, args.extracted)
    except (OSError, ValueError, UnicodeError, struct.error, zlib.error) as error:
        parser.exit(1, f"Import failed: {error}\n")
    print(json.dumps({key: index[key] for key in ("fileCount", "bytes", "scope")}))
    print(f"Device/integration manifests: {len(index['models'])}; full Corsair catalog: unverified")


if __name__ == "__main__":
    main()
