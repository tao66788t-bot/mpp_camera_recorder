#!/usr/bin/env python3
"""Generate a visual sync target page for clap/flash/beep experiments."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    args = parser.parse_args()

    image = Image.new("RGB", (args.width, args.height), (255, 255, 255))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()

    center_x = args.width // 2
    center_y = args.height // 2

    draw.rectangle((0, 0, args.width - 1, args.height - 1), outline=(0, 0, 0), width=6)
    draw.line((center_x, 80, center_x, args.height - 80), fill=(220, 0, 0), width=6)
    draw.line((80, center_y, args.width - 80, center_y), fill=(220, 0, 0), width=6)

    box_w = 420
    box_h = 140
    draw.rectangle(
        (center_x - box_w // 2, center_y - box_h // 2, center_x + box_w // 2, center_y + box_h // 2),
        outline=(0, 0, 0),
        width=5,
    )
    draw.text((center_x - 180, center_y - 30), "FLASH / BEEP SYNC", fill=(0, 0, 0), font=font)
    draw.text((center_x - 165, center_y - 10), "Use with phone flash+beep", fill=(0, 0, 0), font=font)
    draw.text((center_x - 150, center_y + 10), "or clap in front of screen", fill=(0, 0, 0), font=font)

    draw.text((80, 40), "AV Sync Test Page", fill=(0, 0, 0), font=font)
    draw.text((80, 58), "Keep page static. Trigger a simultaneous visual+audio event.", fill=(50, 50, 50), font=font)
    draw.text((80, 76), "Example: screen flash + beep, LED flash + click, or clap.", fill=(50, 50, 50), font=font)

    draw.text((80, args.height - 70), "Suggested capture: 5 repeated events in one MP4 clip.", fill=(0, 0, 0), font=font)
    draw.text((80, args.height - 52), "Then perform event-level frame/audio alignment offline.", fill=(50, 50, 50), font=font)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)
    print(f"generated sync page: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
