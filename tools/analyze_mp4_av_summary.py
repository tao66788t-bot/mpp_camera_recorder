#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

from analyze_mp4_cv import summarize as summarize_cv


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


def read_u16(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 2], "big")


def read_u32(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 4], "big")


def read_u64(data: bytes, off: int) -> int:
    return int.from_bytes(data[off:off + 8], "big")


def parse_boxes(data: bytes, start: int, end: int):
    boxes = []
    off = start
    while off + 8 <= end:
        size = read_u32(data, off)
        box_type = data[off + 4: off + 8]
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
    return data[off:off + 4]


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


def read_stsz_count(data: bytes, stsz_box: Box):
    off = stsz_box.payload_start + 4
    sample_size = read_u32(data, off)
    sample_count = read_u32(data, off + 4)
    return sample_size, sample_count


def read_sample_entry(data: bytes, stsd_box: Box):
    off = stsd_box.payload_start + 4
    entry_count = read_u32(data, off)
    if entry_count <= 0:
        return None
    entry_off = off + 4
    if entry_off + 8 > stsd_box.payload_end:
        return None
    entry_size = read_u32(data, entry_off)
    entry_type = data[entry_off + 4: entry_off + 8]
    if entry_size < 8 or entry_off + entry_size > stsd_box.payload_end:
        return None
    return {
        "offset": entry_off,
        "size": entry_size,
        "type": entry_type.decode("ascii", errors="replace"),
    }


def parse_video_sample_entry(data: bytes, entry_off: int):
    # VisualSampleEntry layout:
    # size(4) type(4) reserved[6] data_ref[2] pre_defined/reserved[16] width[2] height[2]
    return {
        "width": read_u16(data, entry_off + 32),
        "height": read_u16(data, entry_off + 34),
    }


def parse_audio_sample_entry(data: bytes, entry_off: int):
    # AudioSampleEntry layout:
    # size(4) type(4) reserved[6] data_ref[2] reserved[8] channelcount[2]
    # samplesize[2] pre_defined[2] reserved[2] samplerate[4](16.16)
    sample_rate_16_16 = read_u32(data, entry_off + 32)
    return {
        "channels": read_u16(data, entry_off + 24),
        "sample_size": read_u16(data, entry_off + 26),
        "sample_rate": sample_rate_16_16 >> 16,
    }


def parse_tracks(mp4_path: Path):
    data = mp4_path.read_bytes()
    root_boxes = parse_boxes(data, 0, len(data))
    moov = next((b for b in root_boxes if b.type == b"moov"), None)
    if moov is None:
        raise RuntimeError("moov box not found")

    tracks = []
    for trak in moov.children:
        if trak.type != b"trak":
            continue
        mdia = find_child(trak, b"mdia")
        minf = find_child(mdia, b"minf") if mdia else None
        stbl = find_child(minf, b"stbl") if minf else None
        hdlr = find_child(mdia, b"hdlr") if mdia else None
        mdhd = find_child(mdia, b"mdhd") if mdia else None
        stsd = find_child(stbl, b"stsd") if stbl else None
        stsz = find_child(stbl, b"stsz") if stbl else None
        if not all([mdia, stbl, hdlr, mdhd, stsd, stsz]):
            continue

        handler = read_hdlr_type(data, hdlr).decode("ascii", errors="replace")
        timescale, duration = read_mdhd(data, mdhd)
        sample_size, sample_count = read_stsz_count(data, stsz)
        entry = read_sample_entry(data, stsd)

        info = {
            "handler": handler,
            "timescale": timescale,
            "duration": duration,
            "duration_sec": round(duration / timescale, 3) if timescale else 0.0,
            "sample_count": sample_count,
            "default_sample_size": sample_size,
            "codec_tag": entry["type"] if entry else "unknown",
        }

        if entry:
            if handler == "vide":
                info.update(parse_video_sample_entry(data, entry["offset"]))
            elif handler == "soun":
                info.update(parse_audio_sample_entry(data, entry["offset"]))

        tracks.append(info)
    return tracks


def summarize_mp4(mp4_path: Path):
    cv = summarize_cv(mp4_path)
    tracks = parse_tracks(mp4_path)
    return {
        "file": str(mp4_path),
        "tracks": tracks,
        "cv_summary": cv,
    }


def main():
    parser = argparse.ArgumentParser(description="Summarize MP4 audio/video tracks and frame CV.")
    parser.add_argument("mp4", type=Path, help="Path to MP4 file")
    parser.add_argument("--json", action="store_true", help="Print JSON only")
    args = parser.parse_args()

    result = summarize_mp4(args.mp4)
    if args.json:
        print(json.dumps(result, ensure_ascii=True, indent=2))
        return

    print(f"file: {result['file']}")
    for idx, track in enumerate(result["tracks"]):
        print(f"track[{idx}] handler={track['handler']} codec={track['codec_tag']} duration_sec={track['duration_sec']} sample_count={track['sample_count']}")
        if track["handler"] == "vide":
            print(f"  size={track.get('width')}x{track.get('height')}")
        if track["handler"] == "soun":
            print(f"  audio={track.get('sample_rate')}Hz {track.get('channels')}ch {track.get('sample_size')}bit")

    cv = result["cv_summary"]
    print(f"frames: {cv['frames']}")
    print(f"i_frames: {cv['i_frames']}")
    print(f"p_frames: {cv['p_frames']}")
    print(f"avg_bitrate_kbps: {cv['avg_bitrate_kbps']}")
    print(f"cv_percent: {cv['cv_percent']}")


if __name__ == "__main__":
    main()
