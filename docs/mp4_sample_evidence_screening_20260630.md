# MP4 证据样本筛选记录

## 1. 结论

- 首选样本数量：`1`
- 可分析样本数量：`2`
- 无效样本数量：`2`

## 2. 样本明细

| 文件 | 状态 | 说明 |
|------|------|------|
| `old_qp_graceful.mp4` | `usable` | analyzable local MP4 sample |
| `qp_baseline_timeout25.mp4` | `invalid` | moov box not found |
| `qp_limited_graceful.mp4` | `usable` | analyzable local MP4 sample |
| `sample_mp4_full_verified_20260628.mp4` | `primary` | preferred verified sample for current resume-safe facts |
| `latest_cam.mp4` | `invalid` | moov box not found |

## 3. 可分析样本事实

| 文件 | 视频 | 音频 | CV | 码率 kbps |
|------|------|------|------|------|
| `old_qp_graceful.mp4` | 1920x1088 hvc1 | 16000Hz/1ch/16bit mp4a | `175.03` | `2015.65` |
| `qp_limited_graceful.mp4` | 1920x1088 hvc1 | 16000Hz/1ch/16bit mp4a | `151.64` | `2002.33` |
| `sample_mp4_full_verified_20260628.mp4` | 1920x1088 hvc1 | 16000Hz/1ch/16bit mp4a | `75.41` | `1981.73` |

## 4. 无效样本

- `qp_baseline_timeout25.mp4`: `moov box not found`
- `latest_cam.mp4`: `moov box not found`

## 5. 当前使用建议

- 简历与量化证据优先使用 `sample_mp4_full_verified_20260628.mp4`，因为它可完整解析，且与当前主链路事实一致。
- `old_qp_graceful.mp4`、`qp_limited_graceful.mp4` 可以保留给 QP/CV 对比实验使用，但不作为当前“最终验证样本”的首选。
- `qp_baseline_timeout25.mp4` 和任何 `moov box not found` 的文件，不能作为简历量化证据。