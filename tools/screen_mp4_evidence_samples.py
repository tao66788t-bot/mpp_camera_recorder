#!/usr/bin/env python3
"""Screen MP4 samples for resume-evidence suitability.

This script reuses the local MP4 analyzers and turns their output into a
simple evidence-grade report:
- usable as strong local evidence
- analyzable but not primary recommended sample
- invalid / broken as evidence
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from analyze_mp4_av_summary import summarize_mp4


def classify_sample(path: Path) -> dict[str, object]:
    try:
        summary = summarize_mp4(path)
    except Exception as exc:
        return {
            "file": str(path),
            "status": "invalid",
            "reason": str(exc),
        }

    audio_track = next((t for t in summary["tracks"] if t["handler"] == "soun"), None)
    video_track = next((t for t in summary["tracks"] if t["handler"] == "vide"), None)
    cv = summary["cv_summary"]

    primary_names = {
        "sample_mp4_full_verified_20260628.mp4",
    }

    status = "usable"
    reason = "analyzable local MP4 sample"
    if path.name in primary_names:
        status = "primary"
        reason = "preferred verified sample for current resume-safe facts"

    return {
        "file": str(path),
        "status": status,
        "reason": reason,
        "video": {
            "codec": video_track.get("codec_tag") if video_track else None,
            "width": video_track.get("width") if video_track else None,
            "height": video_track.get("height") if video_track else None,
        },
        "audio": {
            "codec": audio_track.get("codec_tag") if audio_track else None,
            "sample_rate": audio_track.get("sample_rate") if audio_track else None,
            "channels": audio_track.get("channels") if audio_track else None,
            "sample_size": audio_track.get("sample_size") if audio_track else None,
        },
        "cv_percent": cv.get("cv_percent"),
        "avg_bitrate_kbps": cv.get("avg_bitrate_kbps"),
        "frames": cv.get("frames"),
    }


def default_project_samples(project_dir: Path) -> list[Path]:
    return sorted(project_dir.glob("*.mp4"))


def build_markdown(results: list[dict[str, object]]) -> str:
    lines: list[str] = []
    lines.append("# MP4 证据样本筛选记录")
    lines.append("")
    lines.append("## 1. 结论")
    lines.append("")

    primary = [item for item in results if item["status"] == "primary"]
    usable = [item for item in results if item["status"] == "usable"]
    invalid = [item for item in results if item["status"] == "invalid"]

    lines.append(f"- 首选样本数量：`{len(primary)}`")
    lines.append(f"- 可分析样本数量：`{len(usable)}`")
    lines.append(f"- 无效样本数量：`{len(invalid)}`")
    lines.append("")

    lines.append("## 2. 样本明细")
    lines.append("")
    lines.append("| 文件 | 状态 | 说明 |")
    lines.append("|------|------|------|")
    for item in results:
        lines.append(
            f"| `{Path(item['file']).name}` | `{item['status']}` | {item['reason']} |"
        )

    analyzable = [item for item in results if item["status"] != "invalid"]
    if analyzable:
        lines.append("")
        lines.append("## 3. 可分析样本事实")
        lines.append("")
        lines.append("| 文件 | 视频 | 音频 | CV | 码率 kbps |")
        lines.append("|------|------|------|------|------|")
        for item in analyzable:
            video = item["video"]
            audio = item["audio"]
            video_str = f"{video['width']}x{video['height']} {video['codec']}"
            audio_str = (
                f"{audio['sample_rate']}Hz/{audio['channels']}ch/"
                f"{audio['sample_size']}bit {audio['codec']}"
            )
            lines.append(
                f"| `{Path(item['file']).name}` | {video_str} | {audio_str} | "
                f"`{item['cv_percent']}` | `{item['avg_bitrate_kbps']}` |"
            )

    if invalid:
        lines.append("")
        lines.append("## 4. 无效样本")
        lines.append("")
        for item in invalid:
            lines.append(f"- `{Path(item['file']).name}`: `{item['reason']}`")

    lines.append("")
    lines.append("## 5. 当前使用建议")
    lines.append("")
    lines.append(
        "- 简历与量化证据优先使用 `sample_mp4_full_verified_20260628.mp4`，"
        "因为它可完整解析，且与当前主链路事实一致。"
    )
    lines.append(
        "- `old_qp_graceful.mp4`、`qp_limited_graceful.mp4` 可以保留给 QP/CV 对比实验使用，"
        "但不作为当前“最终验证样本”的首选。"
    )
    lines.append(
        "- `qp_baseline_timeout25.mp4` 和任何 `moov box not found` 的文件，不能作为简历量化证据。"
    )

    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Project directory to scan for default MP4 samples",
    )
    parser.add_argument(
        "--extra",
        type=Path,
        action="append",
        default=[],
        help="Additional MP4 sample to screen",
    )
    parser.add_argument("--json", action="store_true", help="Print JSON")
    parser.add_argument("--markdown-out", type=Path, help="Write Markdown report")
    args = parser.parse_args()

    samples = default_project_samples(args.project_dir)
    for extra in args.extra:
        if extra not in samples:
            samples.append(extra)

    results = [classify_sample(path) for path in samples]

    if args.json:
        print(json.dumps(results, ensure_ascii=False, indent=2))
    else:
        for item in results:
            print(f"{item['status']}: {item['file']} -> {item['reason']}")

    if args.markdown_out:
        args.markdown_out.write_text(build_markdown(results), encoding="utf-8")
        print(f"Wrote markdown report to: {args.markdown_out}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
