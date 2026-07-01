#!/usr/bin/env python3
"""Quantify LCD preview consistency against a reference image.

This tool is intentionally lightweight:
- no OpenCV/scipy dependency
- works on the existing PNG diagnostic dumps in this project

Typical use in this repo:
    python tools/analyze_lcd_color_consistency.py \
      --reference diag_vi0_lcdfixed_current.png \
      --candidates diag_vi2_lcdfixed_current.png \
                   diag_vi2_lcdfixed_as_nv12.png \
                   diag_vi2_lcdfixed_as_nv21.png
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

from PIL import Image


def load_rgb(path: Path, size: tuple[int, int] | None = None) -> list[tuple[int, int, int]]:
    img = Image.open(path).convert("RGB")
    if size is not None and img.size != size:
        img = img.resize(size, Image.BILINEAR)
    return list(img.getdata())


def mean_rgb(pixels: list[tuple[int, int, int]]) -> tuple[float, float, float]:
    n = len(pixels)
    r = sum(p[0] for p in pixels) / n
    g = sum(p[1] for p in pixels) / n
    b = sum(p[2] for p in pixels) / n
    return (r, g, b)


def std_rgb(pixels: list[tuple[int, int, int]], mean: tuple[float, float, float]) -> tuple[float, float, float]:
    n = len(pixels)
    vr = sum((p[0] - mean[0]) ** 2 for p in pixels) / n
    vg = sum((p[1] - mean[1]) ** 2 for p in pixels) / n
    vb = sum((p[2] - mean[2]) ** 2 for p in pixels) / n
    return (math.sqrt(vr), math.sqrt(vg), math.sqrt(vb))


def compare(
    reference_pixels: list[tuple[int, int, int]],
    candidate_pixels: list[tuple[int, int, int]],
) -> dict[str, float]:
    assert len(reference_pixels) == len(candidate_pixels)
    n = len(reference_pixels)
    abs_err = 0.0
    sq_err = 0.0
    per_channel_abs = [0.0, 0.0, 0.0]
    for ref, cur in zip(reference_pixels, candidate_pixels):
        for i in range(3):
            d = float(cur[i] - ref[i])
            per_channel_abs[i] += abs(d)
            abs_err += abs(d)
            sq_err += d * d
    mae = abs_err / (n * 3)
    rmse = math.sqrt(sq_err / (n * 3))
    return {
        "mae_rgb": mae,
        "rmse_rgb": rmse,
        "mae_r": per_channel_abs[0] / n,
        "mae_g": per_channel_abs[1] / n,
        "mae_b": per_channel_abs[2] / n,
    }


def analyze(reference: Path, candidate: Path) -> dict[str, object]:
    ref_img = Image.open(reference).convert("RGB")
    ref_pixels = list(ref_img.getdata())
    cand_pixels = load_rgb(candidate, ref_img.size)
    ref_mean = mean_rgb(ref_pixels)
    cand_mean = mean_rgb(cand_pixels)
    ref_std = std_rgb(ref_pixels, ref_mean)
    cand_std = std_rgb(cand_pixels, cand_mean)
    result = compare(ref_pixels, cand_pixels)
    result.update(
        {
            "reference": str(reference),
            "candidate": str(candidate),
            "size": {"width": ref_img.size[0], "height": ref_img.size[1]},
            "reference_mean_rgb": [round(x, 3) for x in ref_mean],
            "candidate_mean_rgb": [round(x, 3) for x in cand_mean],
            "reference_std_rgb": [round(x, 3) for x in ref_std],
            "candidate_std_rgb": [round(x, 3) for x in cand_std],
        }
    )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", required=True, type=Path)
    parser.add_argument("--candidates", required=True, nargs="+", type=Path)
    parser.add_argument("--json", action="store_true", help="Emit JSON array")
    args = parser.parse_args()

    results = [analyze(args.reference, candidate) for candidate in args.candidates]
    if args.json:
        print(json.dumps(results, ensure_ascii=False, indent=2))
        return 0

    for item in results:
        print(Path(item["candidate"]).name)
        print(
            "  mae_rgb={:.3f} rmse_rgb={:.3f} "
            "mean_rgb={} std_rgb={}".format(
                item["mae_rgb"],
                item["rmse_rgb"],
                item["candidate_mean_rgb"],
                item["candidate_std_rgb"],
            )
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
