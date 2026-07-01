#!/usr/bin/env python3
"""Generate a synthetic flash+beep MP4 clip for AV sync tool validation."""

from __future__ import annotations

import argparse
import math
import os
import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


KNOWN_FFMPEG_CANDIDATES = [
    Path(r"C:\Users\86150\AppData\Roaming\Python\Python311\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe"),
    Path(r"C:\Trae CN\resources\app\bin\ffmpeg.exe"),
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
    raise FileNotFoundError("No usable ffmpeg executable found.")


def make_frames(frame_dir: Path, width: int, height: int, fps: int, duration_sec: float, flash_time_sec: float) -> None:
    total_frames = int(round(fps * duration_sec))
    flash_frame = int(round(flash_time_sec * fps))
    font = ImageFont.load_default()

    for idx in range(total_frames):
        flashed = idx >= flash_frame
        bg = (255, 255, 255) if flashed else (20, 20, 20)
        fg = (0, 0, 0) if flashed else (230, 230, 230)
        image = Image.new("RGB", (width, height), bg)
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, 0, width - 1, height - 1), outline=fg, width=5)
        label = "FLASH ON" if flashed else "WAIT"
        draw.text((width // 2 - 40, height // 2 - 10), label, fill=fg, font=font)
        draw.text((40, 40), f"frame={idx}", fill=fg, font=font)
        draw.text((40, 58), f"time={idx / fps:.3f}s", fill=fg, font=font)
        image.save(frame_dir / f"frame_{idx:04d}.png")


def make_beep_wav(path: Path, sample_rate: int, duration_sec: float, beep_time_sec: float, beep_len_sec: float, freq_hz: float) -> None:
    frame_count = int(sample_rate * duration_sec)
    beep_start = int(sample_rate * beep_time_sec)
    beep_end = min(frame_count, beep_start + int(sample_rate * beep_len_sec))
    peak = int(32767 * 0.8)
    payload = bytearray()

    for i in range(frame_count):
        if beep_start <= i < beep_end:
            t = i / sample_rate
            sample = int(peak * math.sin(2.0 * math.pi * freq_hz * t))
        else:
            sample = 0
        payload.extend(struct.pack("<h", sample))

    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(bytes(payload))


def mux_clip(ffmpeg: str, frame_dir: Path, wav_path: Path, output: Path, fps: int) -> None:
    cmd = [
        ffmpeg,
        "-y",
        "-framerate",
        str(fps),
        "-i",
        str(frame_dir / "frame_%04d.png"),
        "-i",
        str(wav_path),
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        "-shortest",
        str(output),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=360)
    parser.add_argument("--fps", type=int, default=20)
    parser.add_argument("--duration-sec", type=float, default=3.0)
    parser.add_argument("--flash-time-sec", type=float, default=1.0)
    parser.add_argument("--beep-time-sec", type=float, default=1.0)
    parser.add_argument("--beep-len-sec", type=float, default=0.15)
    parser.add_argument("--beep-freq-hz", type=float, default=1000.0)
    parser.add_argument("--ffmpeg", type=str)
    args = parser.parse_args()

    ffmpeg = resolve_ffmpeg(args.ffmpeg)
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="synthetic_avsync_") as tmpdir:
        tmp = Path(tmpdir)
        frame_dir = tmp / "frames"
        frame_dir.mkdir(parents=True, exist_ok=True)
        wav_path = tmp / "beep.wav"

        make_frames(frame_dir, args.width, args.height, args.fps, args.duration_sec, args.flash_time_sec)
        make_beep_wav(
            wav_path,
            sample_rate=16000,
            duration_sec=args.duration_sec,
            beep_time_sec=args.beep_time_sec,
            beep_len_sec=args.beep_len_sec,
            freq_hz=args.beep_freq_hz,
        )
        mux_clip(ffmpeg, frame_dir, wav_path, args.output, fps=args.fps)

    print(f"generated synthetic av sync clip: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
