#!/usr/bin/env python3
"""Generate a standard sine-wave WAV file for future THD experiments."""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path


def build_samples(
    sample_rate: int,
    duration_sec: float,
    freq_hz: float,
    amplitude: float,
    channels: int,
) -> bytes:
    frame_count = int(sample_rate * duration_sec)
    out = bytearray()
    amplitude = max(0.0, min(amplitude, 0.999))
    peak = int(32767 * amplitude)
    for i in range(frame_count):
        t = i / sample_rate
        sample = int(peak * math.sin(2.0 * math.pi * freq_hz * t))
        packed = struct.pack("<h", sample)
        for _ in range(channels):
            out.extend(packed)
    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sample-rate", type=int, default=16000)
    parser.add_argument("--duration-sec", type=float, default=10.0)
    parser.add_argument("--freq-hz", type=float, default=1000.0)
    parser.add_argument("--amplitude", type=float, default=0.7)
    parser.add_argument("--channels", type=int, default=1)
    args = parser.parse_args()

    payload = build_samples(
        sample_rate=args.sample_rate,
        duration_sec=args.duration_sec,
        freq_hz=args.freq_hz,
        amplitude=args.amplitude,
        channels=args.channels,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(args.output), "wb") as wav:
        wav.setnchannels(args.channels)
        wav.setsampwidth(2)
        wav.setframerate(args.sample_rate)
        wav.writeframes(payload)

    print(
        f"generated {args.output} "
        f"sample_rate={args.sample_rate} "
        f"channels={args.channels} "
        f"duration_sec={args.duration_sec:.3f} "
        f"freq_hz={args.freq_hz:.3f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
