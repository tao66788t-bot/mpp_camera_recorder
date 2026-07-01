#!/usr/bin/env python3
"""Extract audio from MP4 into WAV using a discovered ffmpeg executable."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
from pathlib import Path


KNOWN_FFMPEG_CANDIDATES = [
    Path(r"C:\Trae CN\resources\app\bin\ffmpeg.exe"),
    Path(r"C:\ffmpeg\bin\ffmpeg.exe"),
]


def resolve_ffmpeg(explicit: str | None) -> str:
    if explicit:
        return explicit

    env_ffmpeg = os.environ.get("FFMPEG")
    if env_ffmpeg:
        return env_ffmpeg

    try:
        import imageio_ffmpeg

        bundled = imageio_ffmpeg.get_ffmpeg_exe()
        if bundled:
            return bundled
    except Exception:
        pass

    from_path = shutil.which("ffmpeg")
    if from_path:
        return from_path

    for candidate in KNOWN_FFMPEG_CANDIDATES:
        if candidate.exists():
            return str(candidate)

    raise FileNotFoundError(
        "ffmpeg executable not found. Set --ffmpeg, FFMPEG env, or install ffmpeg."
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="Input MP4 path")
    parser.add_argument("--output", required=True, type=Path, help="Output WAV path")
    parser.add_argument("--sample-rate", type=int, default=16000)
    parser.add_argument("--channels", type=int, default=1)
    parser.add_argument("--ffmpeg", type=str, help="Path to ffmpeg executable")
    args = parser.parse_args()

    ffmpeg = resolve_ffmpeg(args.ffmpeg)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        ffmpeg,
        "-y",
        "-i",
        str(args.input),
        "-vn",
        "-f",
        "wav",
        "-acodec",
        "pcm_s16le",
        "-ar",
        str(args.sample_rate),
        "-ac",
        str(args.channels),
        str(args.output),
    ]
    subprocess.run(cmd, check=True)
    print(f"extracted wav: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
