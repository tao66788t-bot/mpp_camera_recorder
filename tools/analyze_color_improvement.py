#!/usr/bin/env python3
"""Compute before/after DeltaE improvement percentage from two images."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_color_delta_e import analyze_image, summarize


def build_report(before: Path, after: Path, patches_path: Path) -> dict[str, object]:
    patches = json.loads(patches_path.read_text(encoding="utf-8"))
    before_rows = analyze_image(before, patches)
    after_rows = analyze_image(after, patches)
    before_summary = summarize(before_rows)
    after_summary = summarize(after_rows)

    before_avg = float(before_summary.get("avg_delta_e76", 0.0))
    after_avg = float(after_summary.get("avg_delta_e76", 0.0))
    if before_avg > 0:
        improvement_percent = (before_avg - after_avg) / before_avg * 100.0
    else:
        improvement_percent = 0.0

    patch_pairs = []
    after_by_name = {row["name"]: row for row in after_rows}
    for before_row in before_rows:
        name = before_row["name"]
        after_row = after_by_name.get(name, {})
        patch_pairs.append(
            {
                "name": name,
                "before_delta_e76": before_row.get("delta_e76"),
                "after_delta_e76": after_row.get("delta_e76"),
                "delta_improvement": (
                    round(before_row.get("delta_e76", 0.0) - after_row.get("delta_e76", 0.0), 3)
                    if "delta_e76" in before_row and "delta_e76" in after_row
                    else None
                ),
            }
        )

    return {
        "before_image": str(before),
        "after_image": str(after),
        "patches": str(patches_path),
        "before_summary": before_summary,
        "after_summary": after_summary,
        "improvement": {
            "avg_delta_e76_before": round(before_avg, 3),
            "avg_delta_e76_after": round(after_avg, 3),
            "improvement_percent": round(improvement_percent, 3),
            "interpretation": (
                "after image is closer to reference colors"
                if improvement_percent > 0
                else "after image is not better than before by this metric"
            ),
        },
        "patch_improvement": patch_pairs,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", required=True, type=Path)
    parser.add_argument("--after", required=True, type=Path)
    parser.add_argument("--patches", required=True, type=Path)
    args = parser.parse_args()

    report = build_report(args.before, args.after, args.patches)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
