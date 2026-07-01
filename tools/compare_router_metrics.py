#!/usr/bin/env python3
"""Compare two router-log metric summaries and compute relative deltas."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_router_log_metrics import parse_log


def rel_change(old: float, new: float) -> float | None:
    if old == 0:
        return None
    return (new - old) / old * 100.0


def nested_get(d: dict[str, object], *keys: str):
    cur = d
    for key in keys:
        if not isinstance(cur, dict) or key not in cur:
            return None
        cur = cur[key]
    return cur


def build_report(baseline_log: Path, candidate_log: Path) -> dict[str, object]:
    baseline = parse_log(baseline_log)
    candidate = parse_log(candidate_log)

    base_fps = nested_get(baseline, "video", "estimated_fps_from_pts")
    cand_fps = nested_get(candidate, "video", "estimated_fps_from_pts")
    base_fail = nested_get(baseline, "exit_summary", "video_fail_ratio")
    cand_fail = nested_get(candidate, "exit_summary", "video_fail_ratio")
    base_vok = nested_get(baseline, "exit_summary", "video_ok")
    cand_vok = nested_get(candidate, "exit_summary", "video_ok")
    base_aok = nested_get(baseline, "exit_summary", "audio_ok")
    cand_aok = nested_get(candidate, "exit_summary", "audio_ok")

    return {
        "baseline_log": str(baseline_log),
        "candidate_log": str(candidate_log),
        "baseline": baseline,
        "candidate": candidate,
        "comparison": {
            "video_fps_change_percent": (
                round(rel_change(float(base_fps), float(cand_fps)), 3)
                if base_fps is not None and cand_fps is not None and rel_change(float(base_fps), float(cand_fps)) is not None
                else None
            ),
            "video_fail_ratio_change_percent": (
                round(rel_change(float(base_fail), float(cand_fail)), 3)
                if base_fail is not None and cand_fail is not None and rel_change(float(base_fail), float(cand_fail)) is not None
                else None
            ),
            "video_ok_change_percent": (
                round(rel_change(float(base_vok), float(cand_vok)), 3)
                if base_vok is not None and cand_vok is not None and rel_change(float(base_vok), float(cand_vok)) is not None
                else None
            ),
            "audio_ok_change_percent": (
                round(rel_change(float(base_aok), float(cand_aok)), 3)
                if base_aok is not None and cand_aok is not None and rel_change(float(base_aok), float(cand_aok)) is not None
                else None
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    args = parser.parse_args()

    report = build_report(args.baseline, args.candidate)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
