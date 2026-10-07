#!/usr/bin/env python3
"""Validate and name a native 48-view RetroArch PNG for Looking Glass."""
import argparse
import math
from pathlib import Path
import shutil
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="Native PNG screenshot, not a GPU screenshot")
    parser.add_argument("--aspect", type=float, default=4 / 3,
                        help="Intended aspect ratio of one view (default: 4/3)")
    parser.add_argument("--output-dir", type=Path, help="Destination directory (default: input directory)")
    args = parser.parse_args()
    if not math.isfinite(args.aspect) or args.aspect <= 0:
        parser.error("aspect must be a positive finite number")
    try:
        with args.image.open("rb") as stream:
            header = stream.read(33)
        if len(header) != 33 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
            parser.error("input must be a PNG screenshot")
        width, height = struct.unpack(">II", header[16:24])
        if width % 8 or height % 6:
            parser.error("image dimensions must be divisible by 8 columns and 6 rows")
        view_width, view_height = width // 8, height // 6
        if view_width not in (256, 284, 320, 348) or not 192 <= view_height <= 576:
            parser.error("dimensions do not match a native Genesis quilt; disable GPU Screenshot and filters")
        directory = args.output_dir or args.image.parent
        directory.mkdir(parents=True, exist_ok=True)
        stem = args.image.stem.split("_qs")[0]
        destination = directory / f"{stem}_qs8x6a{args.aspect:.6f}.png"
        if destination.resolve() != args.image.resolve():
            with args.image.open("rb") as source, destination.open("xb") as output:
                shutil.copyfileobj(source, output)
        print(destination)
        print(f"48 views, {view_width}x{view_height} pixels per view, aspect {args.aspect:.6f}")
        print("Pixel data is unchanged. This does not verify device calibration or view contents.")
    except OSError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
