#!/usr/bin/env python3
"""Generate a printable/displayable color test card for DeltaE experiments."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PATCHES = [
    ("white", (240, 240, 240)),
    ("neutral_75", (191, 191, 191)),
    ("neutral_50", (128, 128, 128)),
    ("neutral_25", (64, 64, 64)),
    ("black", (20, 20, 20)),
    ("red", (200, 45, 35)),
    ("green", (40, 160, 70)),
    ("blue", (35, 90, 200)),
    ("cyan", (60, 180, 190)),
    ("magenta", (180, 60, 160)),
    ("yellow", (225, 200, 50)),
    ("orange", (220, 120, 35)),
]


def build_card(width: int, height: int) -> tuple[Image.Image, list[dict[str, object]]]:
    bg = (245, 245, 245)
    image = Image.new("RGB", (width, height), bg)
    draw = ImageDraw.Draw(image)

    margin_x = 60
    margin_y = 80
    gap_x = 28
    gap_y = 28
    cols = 4
    rows = 3
    usable_w = width - 2 * margin_x - (cols - 1) * gap_x
    usable_h = height - 2 * margin_y - (rows - 1) * gap_y - 90
    patch_w = usable_w // cols
    patch_h = usable_h // rows

    font = ImageFont.load_default()
    patches_json: list[dict[str, object]] = []

    title = "IPC Color Test Card"
    draw.text((margin_x, 24), title, fill=(20, 20, 20), font=font)
    draw.text(
        (margin_x, 44),
        "Use the same light and same framing for before/after capture.",
        fill=(60, 60, 60),
        font=font,
    )

    for idx, (name, rgb) in enumerate(PATCHES):
        col = idx % cols
        row = idx // cols
        x = margin_x + col * (patch_w + gap_x)
        y = margin_y + row * (patch_h + gap_y)
        draw.rectangle((x, y, x + patch_w, y + patch_h), fill=rgb, outline=(40, 40, 40), width=2)
        label_y = y + patch_h + 8
        draw.text((x, label_y), f"{idx+1:02d} {name}", fill=(20, 20, 20), font=font)
        patches_json.append(
            {
                "name": name,
                "x": x + patch_w // 4,
                "y": y + patch_h // 4,
                "w": patch_w // 2,
                "h": patch_h // 2,
                "ref_rgb": list(rgb),
            }
        )

    footer = (
        "ROI template centers on each patch. If perspective changes, update the ROI JSON "
        "to match the captured image."
    )
    draw.text((margin_x, height - 36), footer, fill=(70, 70, 70), font=font)
    return image, patches_json


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image-output", required=True, type=Path)
    parser.add_argument("--patches-output", required=True, type=Path)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    args = parser.parse_args()

    image, patches_json = build_card(args.width, args.height)
    args.image_output.parent.mkdir(parents=True, exist_ok=True)
    args.patches_output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.image_output)
    args.patches_output.write_text(json.dumps(patches_json, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"generated image: {args.image_output}")
    print(f"generated patches: {args.patches_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
