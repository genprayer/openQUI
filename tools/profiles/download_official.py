#!/usr/bin/env python3
"""Download the current public Corsair gallery and record byte-exact provenance."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from datetime import datetime, timezone
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urljoin, urlparse
import xml.etree.ElementTree as ET

GALLERY_URL = "https://www.corsair.com/us/en/explorer/icue/icue-profiles/"
MAX_PAGES = 256
MAX_PROFILE_BYTES = 32 * 1024 * 1024
MAX_TOTAL_BYTES = 512 * 1024 * 1024
REQUEST_TIMEOUT_SECONDS = 40
PROJECT_ROOT = Path(__file__).resolve().parents[2]


class Links(HTMLParser):
    """Read links without executing the publisher's scripts."""

    def __init__(self) -> None:
        super().__init__()
        self.urls: set[str] = set()

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag == "a":
            href = dict(attrs).get("href")
            if href:
                self.urls.add(href)


def fetch(url: str, byte_limit: int) -> bytes:
    """Fetch a bounded HTTPS response with an explicit timeout and no shell."""
    parsed = urlparse(url)
    if parsed.scheme != "https" or parsed.hostname != "www.corsair.com":
        raise ValueError(f"Unexpected publisher URL: {url}")
    with tempfile.TemporaryDirectory(prefix="openqui-profile-") as directory:
        output = Path(directory) / "response"
        subprocess.run(
            ["curl", "--silent", "--show-error", "--fail", "--location",
             "--proto", "=https", "--proto-redir", "=https", "--max-time",
             str(REQUEST_TIMEOUT_SECONDS), "--max-filesize", str(byte_limit),
             "--retry", "1", "--user-agent", "Mozilla/5.0", "--output",
             str(output), url],
            check=True, timeout=REQUEST_TIMEOUT_SECONDS * 2 + 5,
        )
        if output.stat().st_size > byte_limit:
            raise ValueError("Publisher response exceeds size limit")
        return output.read_bytes()


def links(html: bytes, base: str) -> list[str]:
    parser = Links()
    parser.feed(html.decode("utf-8"))
    return sorted({urljoin(base, href) for href in parser.urls})


def profile_metadata(content: bytes) -> tuple[str, str]:
    """Accept a Cereal profile, excluding DTDs and external entities."""
    if b"<!DOCTYPE" in content.upper() or b"<!ENTITY" in content.upper():
        raise ValueError("DTD/entity declarations are not accepted")
    root = ET.fromstring(content)
    profile = root.find("profile")
    if profile is None:
        profile = root.find("./value0/profile")
    if root.tag != "cereal" or profile is None or not profile.findtext("name", "").strip():
        raise ValueError("Download is not a named Cereal profile")
    return profile.findtext("name", ""), profile.findtext("exporterVersion", "")


def sync(directory: Path) -> dict:
    """Preserve earlier downloads; replace the manifest only after a complete sync."""
    pages = [url for url in links(fetch(GALLERY_URL, MAX_PROFILE_BYTES), GALLERY_URL)
             if url.startswith(GALLERY_URL) and url != GALLERY_URL]
    if not pages or len(pages) > MAX_PAGES:
        raise ValueError("Unexpected gallery size; previous manifest preserved")
    downloads = directory / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []
    seen: set[str] = set()
    total_bytes = 0
    failures: list[str] = []
    for index, page in enumerate(pages, 1):
        try:
            documents = [url for url in links(fetch(page, MAX_PROFILE_BYTES), page)
                         if urlparse(url).path.lower().endswith(".cueprofile")]
            if not documents:
                raise ValueError("No cueprofile download found")
            for url in documents:
                if url in seen:
                    continue
                seen.add(url)
                remaining_bytes = MAX_TOTAL_BYTES - total_bytes
                if remaining_bytes <= 0:
                    raise RuntimeError("Gallery exceeds total download budget")
                content = fetch(url, min(MAX_PROFILE_BYTES, remaining_bytes))
                total_bytes += len(content)
                if total_bytes > MAX_TOTAL_BYTES:
                    raise ValueError("Gallery exceeds total download budget")
                name, exporter = profile_metadata(content)
                slug = urlparse(page).path.strip("/").split("/")[-1]
                original_name = unquote(urlparse(url).path.split("/")[-1])
                filename = re.sub(r"[^A-Za-z0-9_.-]", "_", slug + "--" + original_name)
                target = downloads / filename
                temporary = target.with_suffix(".part")
                try:
                    temporary.write_bytes(content)
                    temporary.replace(target)
                finally:
                    temporary.unlink(missing_ok=True)
                records.append({"name": name, "pageUrl": page, "downloadUrl": url,
                                "file": "downloads/" + filename, "bytes": len(content),
                                "sha256": hashlib.sha256(content).hexdigest(),
                                "exporterVersion": exporter})
            print(f"{index}/{len(pages)}: {urlparse(page).path.split('/')[-2]}", flush=True)
        except (OSError, ValueError, ET.ParseError, subprocess.SubprocessError) as error:
            failures.append(f"{page}: {error}")
    manifest = {"schemaVersion": 1, "source": GALLERY_URL,
                "fetchedAtUtc": datetime.now(timezone.utc).isoformat(),
                "pageCount": len(pages), "totalBytes": total_bytes, "profiles": records}
    report = directory / "download-report.json"
    report.write_text(json.dumps({**manifest, "failures": failures}, indent=2) + "\n")
    if failures:
        raise RuntimeError(f"{len(failures)} pages failed; inspect {report}; catalog preserved")
    temporary = directory / "catalog.json.part"
    try:
        temporary.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
        temporary.replace(directory / "catalog.json")
    finally:
        temporary.unlink(missing_ok=True)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, default=PROJECT_ROOT / "Profiles")
    args = parser.parse_args()
    result = sync(args.directory)
    print(f"Downloaded {len(result['profiles'])} profiles, {result['totalBytes']} bytes")


if __name__ == "__main__":
    main()
