#!/usr/bin/env python3
"""Parse ipc_firmware router logs and summarize throughput / failure metrics.

Expected log patterns include:
  [ROUTER] ok=100 packs=1 pts=... len0=...
  [ROUTER] audio_ok=100 last_len=... last_pts=...
  [ROUTER] exit, video_ok=7575 audio_ok=5972 fail=4945
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


VIDEO_RE = re.compile(
    r"\[ROUTER\]\s+ok=(?P<count>\d+)\s+packs=(?P<packs>\d+)\s+pts=(?P<pts>\d+)"
)
AUDIO_RE = re.compile(
    r"\[ROUTER\]\s+audio_ok=(?P<count>\d+)\s+last_len=(?P<last_len>\d+)\s+last_pts=(?P<pts>\d+)"
)
EXIT_RE = re.compile(
    r"\[ROUTER\]\s+exit,\s+video_ok=(?P<video>\d+)\s+audio_ok=(?P<audio>\d+)\s+fail=(?P<fail>\d+)"
)


def parse_log(path: Path) -> dict[str, object]:
    video_samples: list[tuple[int, int]] = []
    audio_samples: list[tuple[int, int]] = []
    exit_summary: dict[str, int] | None = None

    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = VIDEO_RE.search(line)
        if m:
            video_samples.append((int(m.group("count")), int(m.group("pts"))))
            continue

        m = AUDIO_RE.search(line)
        if m:
            audio_samples.append((int(m.group("count")), int(m.group("pts"))))
            continue

        m = EXIT_RE.search(line)
        if m:
            exit_summary = {
                "video_ok": int(m.group("video")),
                "audio_ok": int(m.group("audio")),
                "fail": int(m.group("fail")),
            }

    result: dict[str, object] = {
        "file": str(path),
        "video_log_samples": len(video_samples),
        "audio_log_samples": len(audio_samples),
    }

    if video_samples:
        first_count, first_pts = video_samples[0]
        last_count, last_pts = video_samples[-1]
        delta_count = last_count - first_count
        delta_pts_us = last_pts - first_pts
        fps = (delta_count * 1_000_000.0 / delta_pts_us) if delta_pts_us > 0 else 0.0
        result["video"] = {
            "first_count": first_count,
            "last_count": last_count,
            "first_pts_us": first_pts,
            "last_pts_us": last_pts,
            "estimated_fps_from_pts": round(fps, 3),
        }

    if audio_samples:
        first_count, first_pts = audio_samples[0]
        last_count, last_pts = audio_samples[-1]
        delta_count = last_count - first_count
        delta_pts_us = last_pts - first_pts
        fps = (delta_count * 1_000_000.0 / delta_pts_us) if delta_pts_us > 0 else 0.0
        result["audio"] = {
            "first_count": first_count,
            "last_count": last_count,
            "first_pts_us": first_pts,
            "last_pts_us": last_pts,
            "estimated_packets_per_sec_from_pts": round(fps, 3),
        }

    if exit_summary:
        total = exit_summary["video_ok"] + exit_summary["fail"]
        fail_ratio = (exit_summary["fail"] / total) if total > 0 else 0.0
        result["exit_summary"] = {
            **exit_summary,
            "video_fail_ratio": round(fail_ratio, 6),
        }

    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    print(json.dumps(parse_log(args.log), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
