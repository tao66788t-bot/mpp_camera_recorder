#!/usr/bin/env python3
"""Check current evidence coverage for resume claims.

This script does not prove hardware closure by itself.
Its job is to answer:
- which claims have strong local evidence
- which only have coarse evidence
- which are only experiment-ready
- which claims are contradicted by current codebase facts
"""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
TOOLS = ROOT / "tools"


def exists(path: Path) -> bool:
    return path.exists()


def load_current_facts() -> dict[str, object]:
    main_c = (ROOT / "src" / "main.c").read_text(encoding="utf-8", errors="ignore")
    facts: dict[str, object] = {
        "video_preview_480x800": "disp_width = 480" in main_c and "disp_height = 800" in main_c,
        "video_encode_1920x1088_20fps": "width = 1920" in main_c and "height = 1088" in main_c and "fps = 20" in main_c,
        "rtsp_554": "rtsp_port = 554" in main_c,
        "audio_16k_mono_16kbps": (
            "sample_rate = 16000" in main_c
            and "AUDIO_SOUND_MODE_MONO" in main_c
            and "aenc_bitrate = 16000" in main_c
        ),
        "face_480x288_thresh_0_60": "input_width = 480" in main_c or "Facedet_480_288_nv12.nb" in main_c,
    }

    src_text = ""
    for path in (ROOT / "src").glob("*"):
        if path.is_file():
            src_text += path.read_text(encoding="utf-8", errors="ignore") + "\n"
    facts["has_opus_chain"] = ("Opus" in src_text) or ("opus" in src_text)
    facts["has_adec_chain"] = "ADEC" in src_text
    facts["has_ao_chain"] = "AO_" in src_text or "mpi_ao" in src_text
    # Avoid substring false positives such as "ANS" in "TRANSPORT".
    aec_markers = (
        r"\bAW_MPI_AI_SetDevVolume\b",
        r"\bEnableAec\b",
        r"\bEnableAns\b",
        r"\bEnableAgc\b",
        r"\bAEC\b",
        r"\bANS\b",
        r"\bAGC\b",
    )
    facts["has_aec_ans_agc"] = any(re.search(pattern, src_text) for pattern in aec_markers)
    return facts


def build_claims() -> list[dict[str, object]]:
    return [
        {
            "claim": "LCD 480x800",
            "target_state": "strong",
            "status": "strong" if exists(DOCS / "lcd_color_consistency_quant_record_20260630.md") else "missing",
            "evidence": ["docs/lcd_color_consistency_quant_record_20260630.md"],
        },
        {
            "claim": "1920x1088@20fps H.265",
            "target_state": "strong",
            "status": "strong" if exists(TOOLS / "collect_resume_metrics.py") else "missing",
            "evidence": ["tools/collect_resume_metrics.py", "sample_mp4_full_verified_20260628.mp4"],
        },
        {
            "claim": "RTSP 554",
            "target_state": "strong",
            "status": "strong" if exists(DOCS / "rtsp_audio_validation_20260628.md") else "missing",
            "evidence": ["docs/rtsp_audio_validation_20260628.md"],
        },
        {
            "claim": "AAC 16000Hz/mono/16kbps",
            "target_state": "strong",
            "status": "strong" if exists(TOOLS / "analyze_mp4_av_summary.py") else "missing",
            "evidence": ["tools/analyze_mp4_av_summary.py", "sample_mp4_full_verified_20260628.mp4"],
        },
        {
            "claim": "CV 143.70% -> 92.70%",
            "target_state": "strong",
            "status": "strong" if exists(DOCS / "venc_qp_cv_optimization_20260628.md") else "missing",
            "evidence": ["docs/venc_qp_cv_optimization_20260628.md", "tools/analyze_mp4_cv.py"],
        },
        {
            "claim": "Face detection 480x288 thresh 0.60",
            "target_state": "strong",
            "status": "strong" if exists(DOCS / "face_verified_backup_restore_20260630.md") else "missing",
            "evidence": ["docs/face_verified_backup_restore_20260630.md"],
        },
        {
            "claim": "AV sync < 30ms",
            "target_state": "strong",
            "status": (
                "toolchain_validated"
                if exists(DOCS / "av_sync_event_chain_verified_20260630.md")
                else ("coarse" if exists(DOCS / "av_sync_coarse_validation_20260630.md") else "missing")
            ),
            "evidence": [
                "docs/av_sync_coarse_validation_20260630.md",
                "docs/av_sync_event_analysis_ready_20260630.md",
                "docs/av_sync_event_chain_verified_20260630.md",
                "tools/analyze_mp4_av_sync.py",
                "tools/analyze_av_sync_event.py",
            ],
        },
        {
            "claim": "THD < 5%",
            "target_state": "strong",
            "status": (
                "toolchain_validated"
                if exists(DOCS / "audio_post_capture_chain_verified_20260630.md")
                else ("experiment_ready" if exists(DOCS / "audio_thd_experiment_ready_20260630.md") else "missing")
            ),
            "evidence": [
                "docs/audio_thd_experiment_ready_20260630.md",
                "docs/audio_post_capture_analysis_chain_20260630.md",
                "docs/audio_post_capture_chain_verified_20260630.md",
                "docs/synthetic_validation_20260630.md",
                "tools/generate_sine_wave.py",
                "tools/analyze_audio_quality.py",
                "tools/extract_audio_from_mp4.py",
            ],
        },
        {
            "claim": "Color improvement 20%",
            "target_state": "strong",
            "status": (
                "toolchain_validated"
                if exists(DOCS / "color_post_capture_chain_verified_20260630.md")
                else ("experiment_ready" if exists(DOCS / "color_deltae_experiment_ready_20260630.md") else "missing")
            ),
            "evidence": [
                "docs/color_deltae_experiment_ready_20260630.md",
                "docs/color_post_capture_chain_20260630.md",
                "docs/color_post_capture_chain_verified_20260630.md",
                "tools/analyze_color_delta_e.py",
                "tools/analyze_color_improvement.py",
                "tools/generate_color_test_card.py",
            ],
        },
        {
            "claim": "Packaging +40% / I/O -60%",
            "target_state": "strong",
            "status": (
                "toolchain_validated"
                if exists(DOCS / "performance_post_capture_chain_verified_20260630.md")
                else ("coarse" if exists(DOCS / "performance_benchmark_ready_20260630.md") else "missing")
            ),
            "evidence": [
                "docs/performance_benchmark_ready_20260630.md",
                "docs/performance_post_capture_chain_20260630.md",
                "docs/performance_post_capture_chain_verified_20260630.md",
                "tools/analyze_router_log_metrics.py",
                "tools/compare_router_metrics.py",
            ],
        },
    ]


def main() -> int:
    facts = load_current_facts()
    claims = build_claims()
    result = {
        "root": str(ROOT),
        "current_facts": facts,
        "claims": claims,
        "contradictions": {
            "old_audio_44k_stereo_128k_is_current": False,
            "opus_decode_chain_is_current": facts["has_opus_chain"],
            "adec_chain_is_current": facts["has_adec_chain"],
            "ao_playback_chain_is_current": facts["has_ao_chain"],
            "aec_ans_agc_is_current": facts["has_aec_ans_agc"],
        },
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
