#!/usr/bin/env python3
"""Repack the attributed MIT diffuse-field KEMAR measurements into 8 stereo HRIR pairs."""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import wave
import zipfile

SOURCE_SHA256 = "a48e73137390626ef1e45d1de6a4d98530c6aa570e71b40f7c4442241ed5fa22"
# MIT azimuth is clockwise. Mirror positive-angle stereo measurements for left speakers.
CHANNELS = ((30, True), (30, False), (0, False), (0, False),
            (150, True), (150, False), (90, True), (90, False))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    content = args.archive.read_bytes()
    if hashlib.sha256(content).hexdigest() != SOURCE_SHA256:
        raise ValueError("Unexpected MIT KEMAR source archive")
    pairs = []
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        for angle, is_mirrored in CHANNELS:
            with wave.open(io.BytesIO(archive.read(f"elev0/H0e{angle:03d}a.wav"))) as source:
                if (source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getnframes()) != (2, 2, 44100, 128):
                    raise ValueError("Unexpected KEMAR impulse format")
                values = struct.unpack("<256h", source.readframes(128))
                pairs.append([(values[i+1], values[i]) if is_mirrored else (values[i], values[i+1])
                              for i in range(0, 256, 2)])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(args.output), "wb") as output:
        output.setnchannels(16)
        output.setsampwidth(2)
        output.setframerate(44100)
        output.writeframes(b"".join(struct.pack("<16h", *(value for pair in pairs for value in pair[index]))
                                  for index in range(128)))
    print(f"KEMAR 7.1: {args.output}, sha256={hashlib.sha256(args.output.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
