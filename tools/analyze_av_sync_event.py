#!/usr/bin/env python3
"""Estimate event-level A/V sync from a flash/beep style MP4 clip.

Method:
- decode frames and compute mean luma per frame
- extract audio to wav and compute short-window RMS envelope
- detect the strongest brightness jump and strongest audio onset
- report the timestamp difference in milliseconds

This is still an assistive tool, not a guarantee of final measurement rigor.
But it upgrades the project from track-level coarse timing to event-level
post-capture analysis.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import tempfile
import wave
from pathlib import Path

import cv2


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


def extract_audio_to_wav(mp4_path: Path, wav_path: Path, ffmpeg: str, sample_rate: int) -> None:
    cmd = [
        ffmpeg,
        "-y",
        "-i",
        str(mp4_path),
        "-vn",
        "-f",
        "wav",
        "-acodec",
        "pcm_s16le",
        "-ar",
        str(sample_rate),
        "-ac",
        "1",
        str(wav_path),
    ]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def detect_flash_event(mp4_path: Path) -> dict[str, object]:
    cap = cv2.VideoCapture(str(mp4_path))
    if not cap.isOpened():
        raise RuntimeError(f"cannot open video: {mp4_path}")

    frame_idx = 0
    fps = cap.get(cv2.CAP_PROP_FPS) or 0.0
    brightness: list[float] = []

    while True:
        ok, frame = cap.read()
        if not ok:
            break
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        brightness.append(float(gray.mean()))
        frame_idx += 1

    cap.release()
    if not brightness:
        raise RuntimeError("no frames decoded")

    deltas = [0.0]
    for i in range(1, len(brightness)):
        deltas.append(brightness[i] - brightness[i - 1])

    event_idx = max(range(len(deltas)), key=lambda i: deltas[i])
    event_time_sec = (event_idx / fps) if fps > 0 else 0.0
    return {
        "frame_count": len(brightness),
        "fps": fps,
        "event_frame_index": event_idx,
        "event_time_sec": event_time_sec,
        "max_brightness_step": deltas[event_idx],
        "mean_brightness": sum(brightness) / len(brightness),
    }


def read_wav_mono(path: Path) -> tuple[int, list[float]]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())

    if sample_width != 2:
        raise RuntimeError(f"unsupported sample width: {sample_width}")

    import struct

    ints = struct.unpack("<{}h".format(len(frames) // 2), frames)
    mono: list[float] = []
    for i in range(0, len(ints), channels):
        mono.append(sum(ints[i : i + channels]) / (channels * 32768.0))
    return sample_rate, mono


def moving_rms(samples: list[float], win: int) -> list[float]:
    out: list[float] = []
    if win <= 1:
        return [abs(x) for x in samples]
    sq_sum = 0.0
    buf: list[float] = []
    for sample in samples:
        val = sample * sample
        buf.append(val)
        sq_sum += val
        if len(buf) > win:
            sq_sum -= buf.pop(0)
        out.append(math.sqrt(sq_sum / len(buf)))
    return out


def detect_audio_event(wav_path: Path, window_ms: float) -> dict[str, object]:
    sample_rate, mono = read_wav_mono(wav_path)
    win = max(1, int(sample_rate * window_ms / 1000.0))
    env = moving_rms(mono, win)
    diffs = [0.0]
    for i in range(1, len(env)):
        diffs.append(env[i] - env[i - 1])

    event_idx = max(range(len(diffs)), key=lambda i: diffs[i])
    event_time_sec = event_idx / sample_rate
    return {
        "sample_rate": sample_rate,
        "sample_count": len(mono),
        "window_ms": window_ms,
        "event_sample_index": event_idx,
        "event_time_sec": event_time_sec,
        "max_envelope_step": diffs[event_idx],
        "mean_envelope": sum(env) / len(env),
    }


def build_report(mp4_path: Path, ffmpeg: str, audio_window_ms: float) -> dict[str, object]:
    flash = detect_flash_event(mp4_path)
    with tempfile.TemporaryDirectory(prefix="avsync_") as tmpdir:
        wav_path = Path(tmpdir) / "audio.wav"
        extract_audio_to_wav(mp4_path, wav_path, ffmpeg=ffmpeg, sample_rate=16000)
        audio = detect_audio_event(wav_path, window_ms=audio_window_ms)

    delta_ms = (audio["event_time_sec"] - flash["event_time_sec"]) * 1000.0
    return {
        "file": str(mp4_path),
        "video_event": flash,
        "audio_event": audio,
        "event_sync": {
            "audio_minus_video_ms": round(delta_ms, 3),
            "abs_audio_minus_video_ms": round(abs(delta_ms), 3),
            "interpretation": (
                "event-level alignment looks promising"
                if abs(delta_ms) <= 100.0
                else "event-level offset is large or event detection is unstable"
            ),
            "note": (
                "Use a clip with a strong flash/beep or clap event. "
                "Weak events can reduce reliability."
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mp4", type=Path)
    parser.add_argument("--ffmpeg", type=str)
    parser.add_argument("--audio-window-ms", type=float, default=5.0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    ffmpeg = resolve_ffmpeg(args.ffmpeg)
    report = build_report(args.mp4, ffmpeg=ffmpeg, audio_window_ms=args.audio_window_ms)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0

    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
