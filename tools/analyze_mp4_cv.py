#!/usr/bin/env python3
import argparse
import json
import math
import statistics
import struct
from pathlib import Path


CONTAINER_BOXES = {
    b"moov",
    b"trak",
    b"mdia",
    b"minf",
    b"stbl",
    b"edts",
    b"dinf",
    b"udta",
    b"meta",
}

FULL_BOXES = {
    b"stsz",
    b"stss",
    b"mdhd",
    b"hdlr",
}


class Box:
    def __init__(self, box_type: bytes, start: int, size: int, header_size: int):
        self.type = box_type
        self.start = start
        self.size = size
        self.header_size = header_size
        self.children = []

    @property
    def payload_start(self) -> int:
        return self.start + self.header_size

    @property
    def payload_end(self) -> int:
        return self.start + self.size


def read_u32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def read_u64(data: bytes, off: int) -> int:
    return struct.unpack_from(">Q", data, off)[0]


def parse_boxes(data: bytes, start: int, end: int):
    boxes = []
    off = start
    while off + 8 <= end:
        size = read_u32(data, off)
        box_type = data[off + 4 : off + 8]
        header_size = 8
        if size == 1:
            if off + 16 > end:
                break
            size = read_u64(data, off + 8)
            header_size = 16
        elif size == 0:
            size = end - off

        if size < header_size or off + size > end:
            break

        box = Box(box_type, off, size, header_size)
        child_start = box.payload_start
        if box_type == b"meta":
            child_start += 4
        if box_type in CONTAINER_BOXES:
            box.children = parse_boxes(data, child_start, box.payload_end)
        boxes.append(box)
        off += size
    return boxes


def find_child(box: Box, box_type: bytes):
    for child in box.children:
        if child.type == box_type:
            return child
    return None


def read_hdlr_type(data: bytes, hdlr_box: Box) -> bytes:
    off = hdlr_box.payload_start + 8
    return data[off : off + 4]


def read_mdhd(data: bytes, mdhd_box: Box):
    off = mdhd_box.payload_start
    version = data[off]
    if version == 1:
        timescale = read_u32(data, off + 16)
        duration = read_u64(data, off + 20)
    else:
        timescale = read_u32(data, off + 12)
        duration = read_u32(data, off + 16)
    return timescale, duration


def read_stsz(data: bytes, stsz_box: Box):
    off = stsz_box.payload_start + 4
    sample_size = read_u32(data, off)
    sample_count = read_u32(data, off + 4)
    if sample_size != 0:
        return [sample_size] * sample_count
    sizes = []
    ptr = off + 8
    for _ in range(sample_count):
        sizes.append(read_u32(data, ptr))
        ptr += 4
    return sizes


def read_stss(data: bytes, stss_box: Box):
    if stss_box is None:
        return [1]
    off = stss_box.payload_start + 4
    entry_count = read_u32(data, off)
    sync_samples = []
    ptr = off + 4
    for _ in range(entry_count):
        sync_samples.append(read_u32(data, ptr))
        ptr += 4
    return sync_samples


def parse_video_track(mp4_path: Path):
    data = mp4_path.read_bytes()
    root_boxes = parse_boxes(data, 0, len(data))
    moov = next((b for b in root_boxes if b.type == b"moov"), None)
    if moov is None:
        raise RuntimeError("moov box not found")

    for trak in moov.children:
        if trak.type != b"trak":
            continue
        mdia = find_child(trak, b"mdia")
        if mdia is None:
            continue
        hdlr = find_child(mdia, b"hdlr")
        if hdlr is None or read_hdlr_type(data, hdlr) != b"vide":
            continue
        mdhd = find_child(mdia, b"mdhd")
        minf = find_child(mdia, b"minf")
        stbl = find_child(minf, b"stbl") if minf else None
        stsz = find_child(stbl, b"stsz") if stbl else None
        if mdhd is None or stsz is None:
            continue
        stss = find_child(stbl, b"stss")
        timescale, duration = read_mdhd(data, mdhd)
        sample_sizes = read_stsz(data, stsz)
        sync_samples = read_stss(data, stss)
        return {
            "timescale": timescale,
            "duration": duration,
            "sample_sizes": sample_sizes,
            "sync_samples": sync_samples,
        }
    raise RuntimeError("video track not found")


def kb(value_bytes: float) -> float:
    return value_bytes / 1024.0


def summarize(mp4_path: Path):
    track = parse_video_track(mp4_path)
    sizes = track["sample_sizes"]
    sync_set = set(track["sync_samples"])
    i_sizes = [sizes[i - 1] for i in sync_set if 1 <= i <= len(sizes)]
    p_sizes = [size for idx, size in enumerate(sizes, start=1) if idx not in sync_set]
    duration_sec = track["duration"] / track["timescale"] if track["timescale"] else 0.0
    mean_size = statistics.mean(sizes)
    stdev_size = statistics.pstdev(sizes) if len(sizes) > 1 else 0.0
    cv = (stdev_size / mean_size * 100.0) if mean_size else 0.0
    bitrate_kbps = (sum(sizes) * 8.0 / duration_sec / 1000.0) if duration_sec > 0 else 0.0

    def stats(values):
        if not values:
            return None
        return {
            "count": len(values),
            "avg_kb": round(kb(statistics.mean(values)), 2),
            "min_kb": round(kb(min(values)), 2),
            "max_kb": round(kb(max(values)), 2),
        }

    return {
        "file": str(mp4_path),
        "frames": len(sizes),
        "i_frames": len(i_sizes),
        "p_frames": len(p_sizes),
        "duration_sec": round(duration_sec, 3),
        "avg_bitrate_kbps": round(bitrate_kbps, 2),
        "cv_percent": round(cv, 2),
        "overall": stats(sizes),
        "i_frame_stats": stats(i_sizes),
        "p_frame_stats": stats(p_sizes),
        "first_20_frame_sizes_bytes": sizes[:20],
    }


def main():
    parser = argparse.ArgumentParser(description="Analyze MP4 video frame size CV.")
    parser.add_argument("mp4", type=Path, help="Path to MP4 file")
    parser.add_argument("--json", action="store_true", help="Print JSON only")
    args = parser.parse_args()

    result = summarize(args.mp4)
    if args.json:
        print(json.dumps(result, ensure_ascii=True, indent=2))
        return

    print(f"file: {result['file']}")
    print(f"frames: {result['frames']}")
    print(f"i_frames: {result['i_frames']}")
    print(f"p_frames: {result['p_frames']}")
    print(f"duration_sec: {result['duration_sec']}")
    print(f"avg_bitrate_kbps: {result['avg_bitrate_kbps']}")
    print(f"cv_percent: {result['cv_percent']}")
    if result["i_frame_stats"]:
        print(
            "i_frame_stats: "
            f"avg={result['i_frame_stats']['avg_kb']}KB "
            f"min={result['i_frame_stats']['min_kb']}KB "
            f"max={result['i_frame_stats']['max_kb']}KB"
        )
    if result["p_frame_stats"]:
        print(
            "p_frame_stats: "
            f"avg={result['p_frame_stats']['avg_kb']}KB "
            f"min={result['p_frame_stats']['min_kb']}KB "
            f"max={result['p_frame_stats']['max_kb']}KB"
        )


if __name__ == "__main__":
    main()
