#!/usr/bin/env python3
"""Compute Lab/DeltaE statistics for color patches on one or two images.

Patch JSON example:
[
  {"name": "gray", "x": 120, "y": 80, "w": 32, "h": 32,
   "ref_rgb": [128, 128, 128]},
  {"name": "red", "x": 220, "y": 80, "w": 32, "h": 32,
   "ref_rgb": [180, 40, 30]}
]
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image


def srgb_to_linear(v: float) -> float:
    v = v / 255.0
    if v <= 0.04045:
        return v / 12.92
    return ((v + 0.055) / 1.055) ** 2.4


def rgb_to_xyz(rgb: tuple[float, float, float]) -> tuple[float, float, float]:
    r = srgb_to_linear(rgb[0])
    g = srgb_to_linear(rgb[1])
    b = srgb_to_linear(rgb[2])
    x = r * 0.4124564 + g * 0.3575761 + b * 0.1804375
    y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750
    z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041
    return (x, y, z)


def f_lab(t: float) -> float:
    delta = 6 / 29
    if t > delta**3:
        return t ** (1 / 3)
    return t / (3 * delta * delta) + 4 / 29


def xyz_to_lab(xyz: tuple[float, float, float]) -> tuple[float, float, float]:
    # D65 reference white
    xn, yn, zn = 0.95047, 1.0, 1.08883
    fx = f_lab(xyz[0] / xn)
    fy = f_lab(xyz[1] / yn)
    fz = f_lab(xyz[2] / zn)
    l = 116 * fy - 16
    a = 500 * (fx - fy)
    b = 200 * (fy - fz)
    return (l, a, b)


def rgb_to_lab(rgb: tuple[float, float, float]) -> tuple[float, float, float]:
    return xyz_to_lab(rgb_to_xyz(rgb))


def delta_e76(lab1: tuple[float, float, float], lab2: tuple[float, float, float]) -> float:
    return math.sqrt(sum((a - b) ** 2 for a, b in zip(lab1, lab2)))


def patch_mean_rgb(image: Image.Image, patch: dict[str, object]) -> tuple[float, float, float]:
    x = int(patch["x"])
    y = int(patch["y"])
    w = int(patch["w"])
    h = int(patch["h"])
    crop = image.crop((x, y, x + w, y + h)).convert("RGB")
    pixels = list(crop.getdata())
    n = len(pixels)
    return (
        sum(p[0] for p in pixels) / n,
        sum(p[1] for p in pixels) / n,
        sum(p[2] for p in pixels) / n,
    )


def analyze_image(image_path: Path, patches: list[dict[str, object]]) -> list[dict[str, object]]:
    image = Image.open(image_path).convert("RGB")
    rows = []
    for patch in patches:
        mean_rgb = patch_mean_rgb(image, patch)
        mean_lab = rgb_to_lab(mean_rgb)
        row: dict[str, object] = {
            "name": patch["name"],
            "mean_rgb": [round(v, 3) for v in mean_rgb],
            "mean_lab": [round(v, 3) for v in mean_lab],
        }
        if "ref_rgb" in patch:
            ref_rgb = tuple(float(v) for v in patch["ref_rgb"])
            ref_lab = rgb_to_lab(ref_rgb)
            row["ref_rgb"] = list(ref_rgb)
            row["ref_lab"] = [round(v, 3) for v in ref_lab]
            row["delta_e76"] = round(delta_e76(mean_lab, ref_lab), 3)
        rows.append(row)
    return rows


def summarize(rows: list[dict[str, object]]) -> dict[str, object]:
    de_values = [row["delta_e76"] for row in rows if "delta_e76" in row]
    if not de_values:
        return {"patch_count": len(rows)}
    return {
        "patch_count": len(rows),
        "avg_delta_e76": round(sum(de_values) / len(de_values), 3),
        "max_delta_e76": round(max(de_values), 3),
        "min_delta_e76": round(min(de_values), 3),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--patches", required=True, type=Path)
    parser.add_argument("--compare-image", type=Path)
    args = parser.parse_args()

    patches = json.loads(args.patches.read_text(encoding="utf-8"))
    primary_rows = analyze_image(args.image, patches)
    result: dict[str, object] = {
        "image": str(args.image),
        "summary": summarize(primary_rows),
        "patches": primary_rows,
    }

    if args.compare_image:
        compare_rows = analyze_image(args.compare_image, patches)
        result["compare_image"] = str(args.compare_image)
        result["compare_summary"] = summarize(compare_rows)
        result["compare_patches"] = compare_rows

    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
