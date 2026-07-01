#!/usr/bin/env python3
"""Estimate coarse A/V sync drift from MP4 track timing metadata.

This is intentionally conservative:
- it uses track duration/timescale/sample_count
- it can estimate start/end drift bounds at track granularity
- it does NOT claim frame-accurate lip-sync validation
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_mp4_av_summary import summarize_mp4


def build_sync_report(mp4_path: Path) -> dict[str, object]:
    summary = summarize_mp4(mp4_path)
    video = next((t for t in summary["tracks"] if t["handler"] == "vide"), None)
    audio = next((t for t in summary["tracks"] if t["handler"] == "soun"), None)
    if not video or not audio:
        raise RuntimeError("video/audio tracks not found")

    video_duration = float(video["duration_sec"])
    audio_duration = float(audio["duration_sec"])
    duration_gap_sec = video_duration - audio_duration
    duration_gap_ms = duration_gap_sec * 1000.0

    video_frame_period_ms = (
        (video_duration / video["sample_count"]) * 1000.0 if video["sample_count"] else 0.0
    )
    audio_frame_period_ms = (
        (audio_duration / audio["sample_count"]) * 1000.0 if audio["sample_count"] else 0.0
    )

    return {
        "file": str(mp4_path),
        "video": {
            "duration_sec": video_duration,
            "sample_count": video["sample_count"],
            "timescale": video["timescale"],
            "frame_period_ms": round(video_frame_period_ms, 3),
        },
        "audio": {
            "duration_sec": audio_duration,
            "sample_count": audio["sample_count"],
            "timescale": audio["timescale"],
            "frame_period_ms": round(audio_frame_period_ms, 3),
            "sample_rate": audio.get("sample_rate"),
            "channels": audio.get("channels"),
        },
        "coarse_sync": {
            "duration_gap_ms": round(duration_gap_ms, 3),
            "abs_duration_gap_ms": round(abs(duration_gap_ms), 3),
            "interpretation": (
                "track durations are closely aligned at coarse container level"
                if abs(duration_gap_ms) <= 500.0
                else "track duration drift is large and needs deeper investigation"
            ),
            "note": (
                "This only proves coarse track-level alignment. "
                "Frame-accurate A/V sync still needs per-sample PTS analysis or controlled clap/beep testing."
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mp4", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    report = build_sync_report(args.mp4)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0

    print(f"file: {report['file']}")
    print(
        "video: duration_sec={:.3f} sample_count={} frame_period_ms={:.3f}".format(
            report["video"]["duration_sec"],
            report["video"]["sample_count"],
            report["video"]["frame_period_ms"],
        )
    )
    print(
        "audio: duration_sec={:.3f} sample_count={} frame_period_ms={:.3f}".format(
            report["audio"]["duration_sec"],
            report["audio"]["sample_count"],
            report["audio"]["frame_period_ms"],
        )
    )
    print(
        "coarse_sync: duration_gap_ms={:.3f} ({})".format(
            report["coarse_sync"]["duration_gap_ms"],
            report["coarse_sync"]["interpretation"],
        )
    )
    print(report["coarse_sync"]["note"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
