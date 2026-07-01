#!/usr/bin/env python3
"""Basic WAV/PCM spectral analysis and THD estimation.

This is meant for resume evidence building:
- summarize sample rate / channels / duration
- estimate dominant frequency
- estimate THD if the input is a near-sine waveform

PCM usage example:
    python tools/analyze_audio_quality.py \
      --pcm input.pcm --sample-rate 44100 --channels 2 --sample-width 2
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import wave
from pathlib import Path


def read_wav(path: Path) -> tuple[int, int, int, list[float]]:
    with wave.open(str(path), "rb") as wav:
        channels = wav.getnchannels()
        sample_width = wav.getsampwidth()
        sample_rate = wav.getframerate()
        frames = wav.readframes(wav.getnframes())
    samples = decode_pcm_bytes(frames, sample_width, channels)
    return sample_rate, channels, sample_width, samples


def decode_pcm_bytes(data: bytes, sample_width: int, channels: int) -> list[float]:
    if sample_width == 2:
        fmt = "<{}h".format(len(data) // 2)
        ints = struct.unpack(fmt, data)
        scale = 32768.0
    elif sample_width == 1:
        fmt = "<{}B".format(len(data))
        ints = [x - 128 for x in struct.unpack(fmt, data)]
        scale = 128.0
    else:
        raise ValueError(f"unsupported sample_width={sample_width}")

    mono = []
    for i in range(0, len(ints), channels):
        mono.append(sum(ints[i : i + channels]) / (channels * scale))
    return mono


def read_pcm(path: Path, sample_rate: int, channels: int, sample_width: int) -> tuple[int, int, int, list[float]]:
    data = path.read_bytes()
    samples = decode_pcm_bytes(data, sample_width, channels)
    return sample_rate, channels, sample_width, samples


def hann(n: int, i: int) -> float:
    if n <= 1:
        return 1.0
    return 0.5 - 0.5 * math.cos((2.0 * math.pi * i) / (n - 1))


def dft_magnitude(samples: list[float], sample_rate: int, max_bins: int | None = None) -> list[tuple[float, float]]:
    n = len(samples)
    bins = n // 2
    if max_bins is not None:
        bins = min(bins, max_bins)
    out: list[tuple[float, float]] = []
    for k in range(bins):
        real = 0.0
        imag = 0.0
        for i, sample in enumerate(samples):
            w = hann(n, i)
            angle = 2.0 * math.pi * k * i / n
            real += sample * w * math.cos(angle)
            imag -= sample * w * math.sin(angle)
        mag = math.sqrt(real * real + imag * imag)
        freq = k * sample_rate / n
        out.append((freq, mag))
    return out


def estimate_metrics(sample_rate: int, samples: list[float]) -> dict[str, float | str]:
    if not samples:
        raise ValueError("empty audio samples")

    peak = max(abs(x) for x in samples)
    rms = math.sqrt(sum(x * x for x in samples) / len(samples))

    # Keep runtime bounded: analyze up to ~8192 mono samples.
    window = samples[: min(len(samples), 8192)]
    spectrum = dft_magnitude(window, sample_rate, max_bins=2048)
    if len(spectrum) < 4:
        return {
            "peak": peak,
            "rms": rms,
            "dominant_freq_hz": 0.0,
            "thd_percent": 0.0,
        }

    usable = spectrum[1:]
    total_mag = sum(item[1] for item in usable)
    fundamental_freq, fundamental_mag = max(usable, key=lambda item: item[1])
    fundamental_ratio = (fundamental_mag / total_mag) if total_mag > 0 else 0.0
    if fundamental_mag <= 0:
        thd = 0.0
    else:
        harmonic_sq = 0.0
        for harmonic in range(2, 6):
            target = fundamental_freq * harmonic
            best = min(usable, key=lambda item: abs(item[0] - target))
            harmonic_sq += best[1] * best[1]
        thd = math.sqrt(harmonic_sq) / fundamental_mag * 100.0

    note = (
        "dominant tone is clear enough for rough THD estimation"
        if fundamental_ratio >= 0.35
        else "broadband or complex audio; THD value is not suitable as a resume metric"
    )

    return {
        "peak": peak,
        "rms": rms,
        "dominant_freq_hz": fundamental_freq,
        "dominant_ratio": fundamental_ratio,
        "thd_percent": thd,
        "analysis_note": note,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wav", type=Path)
    parser.add_argument("--pcm", type=Path)
    parser.add_argument("--sample-rate", type=int)
    parser.add_argument("--channels", type=int)
    parser.add_argument("--sample-width", type=int, help="bytes per sample, e.g. 2 for s16le")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    if bool(args.wav) == bool(args.pcm):
        raise SystemExit("provide exactly one of --wav or --pcm")

    if args.wav:
        sample_rate, channels, sample_width, samples = read_wav(args.wav)
        source = args.wav
    else:
        if not all([args.sample_rate, args.channels, args.sample_width]):
            raise SystemExit("--pcm requires --sample-rate --channels --sample-width")
        sample_rate, channels, sample_width, samples = read_pcm(
            args.pcm, args.sample_rate, args.channels, args.sample_width
        )
        source = args.pcm

    metrics = estimate_metrics(sample_rate, samples)
    result = {
        "source": str(source),
        "sample_rate": sample_rate,
        "channels": channels,
        "sample_width_bytes": sample_width,
        "samples_per_channel": len(samples),
        "duration_sec": len(samples) / sample_rate,
        **metrics,
    }

    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0

    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
