# IPC 项目 Claim 状态自动报告

## 1. 当前代码事实

- `video_preview_480x800`: `True`
- `video_encode_1920x1088_20fps`: `True`
- `rtsp_554`: `True`
- `audio_16k_mono_16kbps`: `True`
- `face_480x288_thresh_0_60`: `True`
- `has_opus_chain`: `False`
- `has_adec_chain`: `False`
- `has_ao_chain`: `False`
- `has_aec_ans_agc`: `False`

## 2. Claim 状态

| Claim | 当前状态 | 目标状态 | 证据 |
|------|------|------|------|
| LCD 480x800 | `strong` | `strong` | `docs/lcd_color_consistency_quant_record_20260630.md` |
| 1920x1088@20fps H.265 | `strong` | `strong` | `tools/collect_resume_metrics.py`, `sample_mp4_full_verified_20260628.mp4` |
| RTSP 554 | `strong` | `strong` | `docs/rtsp_audio_validation_20260628.md` |
| AAC 16000Hz/mono/16kbps | `strong` | `strong` | `tools/analyze_mp4_av_summary.py`, `sample_mp4_full_verified_20260628.mp4` |
| CV 143.70% -> 92.70% | `strong` | `strong` | `docs/venc_qp_cv_optimization_20260628.md`, `tools/analyze_mp4_cv.py` |
| Face detection 480x288 thresh 0.60 | `strong` | `strong` | `docs/face_verified_backup_restore_20260630.md` |
| AV sync < 30ms | `toolchain_validated` | `strong` | `docs/av_sync_coarse_validation_20260630.md`, `docs/av_sync_event_analysis_ready_20260630.md`, `docs/av_sync_event_chain_verified_20260630.md`, `tools/analyze_mp4_av_sync.py`, `tools/analyze_av_sync_event.py` |
| THD < 5% | `toolchain_validated` | `strong` | `docs/audio_thd_experiment_ready_20260630.md`, `docs/audio_post_capture_analysis_chain_20260630.md`, `docs/audio_post_capture_chain_verified_20260630.md`, `docs/synthetic_validation_20260630.md`, `tools/generate_sine_wave.py`, `tools/analyze_audio_quality.py`, `tools/extract_audio_from_mp4.py` |
| Color improvement 20% | `toolchain_validated` | `strong` | `docs/color_deltae_experiment_ready_20260630.md`, `docs/color_post_capture_chain_20260630.md`, `docs/color_post_capture_chain_verified_20260630.md`, `tools/analyze_color_delta_e.py`, `tools/analyze_color_improvement.py`, `tools/generate_color_test_card.py` |
| Packaging +40% / I/O -60% | `toolchain_validated` | `strong` | `docs/performance_benchmark_ready_20260630.md`, `docs/performance_post_capture_chain_20260630.md`, `docs/performance_post_capture_chain_verified_20260630.md`, `tools/analyze_router_log_metrics.py`, `tools/compare_router_metrics.py` |

## 3. 与旧文档冲突或需警惕项

- `old_audio_44k_stereo_128k_is_current`: `False`
- `opus_decode_chain_is_current`: `False`
- `adec_chain_is_current`: `False`
- `ao_playback_chain_is_current`: `False`
- `aec_ans_agc_is_current`: `False`

## 4. 自动结论

- 已有强证据：LCD 480x800, 1920x1088@20fps H.265, RTSP 554, AAC 16000Hz/mono/16kbps, CV 143.70% -> 92.70%, Face detection 480x288 thresh 0.60
- 只有粗证据：无
- 工具链已验证、但仍缺板端标准化采样：AV sync < 30ms, THD < 5%, Color improvement 20%, Packaging +40% / I/O -60%
- 仅实验准备完成：无