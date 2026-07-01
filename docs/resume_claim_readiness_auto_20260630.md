# IPC 项目简历 Claim Ready 状态自动报告

## 1. 可直接写进简历

- `LCD 480x800`
- `1920x1088@20fps H.265`
- `RTSP 554`
- `AAC 16000Hz/mono/16kbps`
- `CV 143.70% -> 92.70%`
- `Face detection 480x288 thresh 0.60`

## 2. 工具链已验证，但还缺板端标准化采样

- `AV sync < 30ms`
  证据：docs/av_sync_coarse_validation_20260630.md, docs/av_sync_event_analysis_ready_20260630.md, docs/av_sync_event_chain_verified_20260630.md, tools/analyze_mp4_av_sync.py, tools/analyze_av_sync_event.py
- `THD < 5%`
  证据：docs/audio_thd_experiment_ready_20260630.md, docs/audio_post_capture_analysis_chain_20260630.md, docs/audio_post_capture_chain_verified_20260630.md, docs/synthetic_validation_20260630.md, tools/generate_sine_wave.py, tools/analyze_audio_quality.py, tools/extract_audio_from_mp4.py
- `Color improvement 20%`
  证据：docs/color_deltae_experiment_ready_20260630.md, docs/color_post_capture_chain_20260630.md, docs/color_post_capture_chain_verified_20260630.md, tools/analyze_color_delta_e.py, tools/analyze_color_improvement.py, tools/generate_color_test_card.py
- `Packaging +40% / I/O -60%`
  证据：docs/performance_benchmark_ready_20260630.md, docs/performance_post_capture_chain_20260630.md, docs/performance_post_capture_chain_verified_20260630.md, tools/analyze_router_log_metrics.py, tools/compare_router_metrics.py

## 3. 只有粗证据，不能写死

- 无

## 4. 仅实验准备完成

- 无

## 5. 仍缺失

- 无

## 6. 自动结论

- 已做实、可直接写：6 项
- 工具链已验证、只差板端标准化采样：4 项
- 只有粗证据：0 项
- 仅实验准备完成：0 项
- 完全缺失：0 项