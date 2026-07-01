#!/usr/bin/env python3
"""Generate synthetic degraded variants of the color test card for pipeline validation."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageEnhance


def build_variant(image: Image.Image, mode: str) -> Image.Image:
    if mode == "warmer_brighter":
        r, g, b = image.split()
        r = ImageEnhance.Brightness(r).enhance(1.14)
        g = ImageEnhance.Brightness(g).enhance(1.05)
        b = ImageEnhance.Brightness(b).enhance(0.86)
        merged = Image.merge("RGB", (r, g, b))
        return ImageEnhance.Brightness(merged).enhance(1.08)

    if mode == "cooler_darker":
        r, g, b = image.split()
        r = ImageEnhance.Brightness(r).enhance(0.88)
        g = ImageEnhance.Brightness(g).enhance(0.95)
        b = ImageEnhance.Brightness(b).enhance(1.10)
        merged = Image.merge("RGB", (r, g, b))
        return ImageEnhance.Brightness(merged).enhance(0.92)

    raise ValueError(f"unsupported mode: {mode}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--mode", choices=["warmer_brighter", "cooler_darker"], default="warmer_brighter")
    args = parser.parse_args()

    image = Image.open(args.input).convert("RGB")
    out = build_variant(image, args.mode)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    out.save(args.output)
    print(f"generated variant: {args.output} mode={args.mode}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
