#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

from analyze_mp4_av_summary import summarize_mp4


def collect_mp4_metrics(project_dir: Path):
    results = []
    for mp4 in sorted(project_dir.glob("*.mp4")):
        try:
            results.append(summarize_mp4(mp4))
        except Exception as exc:  # pragma: no cover
            results.append({
                "file": str(mp4),
                "error": str(exc),
            })
    return results


def build_current_facts():
    return {
        "video_preview": {
            "width": 480,
            "height": 800,
            "pixel_format": "NV12",
        },
        "video_encode": {
            "width": 1920,
            "height": 1088,
            "fps": 20,
            "codec": "H.265",
            "bitrate_bps": 2000000,
        },
        "audio_encode": {
            "sample_rate": 16000,
            "channels": 1,
            "bit_width": 16,
            "bitrate_bps": 16000,
            "codec": "AAC",
        },
        "rtsp": {
            "port": 554,
            "audio_enabled": True,
            "video_enabled": True,
        },
        "face_detection": {
            "model": "Facedet_480_288_nv12.nb",
            "input_width": 480,
            "input_height": 288,
            "score_thresh": 0.60,
        },
        "verified_cv_experiment": {
            "baseline_cv_percent": 143.70,
            "optimized_cv_percent": 92.70,
            "target_bitrate_mbps": 2.0,
        },
    }


def main():
    parser = argparse.ArgumentParser(description="Collect current resume-safe project metrics.")
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Project directory",
    )
    parser.add_argument("--json", action="store_true", help="Print JSON")
    args = parser.parse_args()

    result = {
        "current_facts": build_current_facts(),
        "mp4_samples": collect_mp4_metrics(args.project_dir),
    }

    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return

    print("Current facts:")
    print(json.dumps(result["current_facts"], ensure_ascii=False, indent=2))
    print("\nMP4 samples:")
    for item in result["mp4_samples"]:
        if "error" in item:
            print(f"- {item['file']}: ERROR {item['error']}")
            continue
        audio_track = next((t for t in item["tracks"] if t["handler"] == "soun"), None)
        video_track = next((t for t in item["tracks"] if t["handler"] == "vide"), None)
        print(f"- {Path(item['file']).name}")
        if video_track:
            print(f"  video={video_track.get('width')}x{video_track.get('height')} codec={video_track.get('codec_tag')}")
        if audio_track:
            print(
                "  audio="
                f"{audio_track.get('sample_rate')}Hz/"
                f"{audio_track.get('channels')}ch/"
                f"{audio_track.get('sample_size')}bit "
                f"codec={audio_track.get('codec_tag')}"
            )
        print(
            "  cv="
            f"{item['cv_summary']['cv_percent']} "
            f"bitrate_kbps={item['cv_summary']['avg_bitrate_kbps']}"
        )


if __name__ == "__main__":
    main()
