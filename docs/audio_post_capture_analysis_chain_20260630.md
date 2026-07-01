# 音频录制后处理链补充记录（2026-06-30）

## 1. 目标

前面我已经补齐了：

- 标准 `1kHz` 音频输入素材
- `THD` 分析脚本

这次继续补“录完板端 MP4 之后，怎么把音频取出来继续分析”这一段，让 `THD` 实验从：

- 只有输入素材

推进到：

- 输入素材
- 板端录制
- MP4 导出 WAV
- WAV 跑 THD 分析

---

## 2. 新增脚本

- `tools/extract_audio_from_mp4.py`

用途：

- 从板端录出来的 MP4 中提取音频
- 输出为 `16kHz / mono / pcm_s16le` WAV
- 后续可直接喂给 `tools/analyze_audio_quality.py`

---

## 3. 本机环境结论

这次顺手把本机后处理能力也确认了一遍：

- `PIL` 可用
- `numpy` 可用
- `scipy` 可用
- 系统 `PATH` 里没有 `ffmpeg`
- 但本机存在可用 `ffmpeg.exe`：
  - `C:\Trae CN\resources\app\bin\ffmpeg.exe`

因此当前本机并不是“做不了 MP4 音频提取”，而是“不能假设 `ffmpeg` 在 PATH 里”，所以脚本里加了自动探测逻辑。

---

## 4. 建议命令

```powershell
python .\tools\extract_audio_from_mp4.py `
  --input .\sample_mp4_full_verified_20260628.mp4 `
  --output .\artifacts\sample_mp4_full_verified_20260628_audio.wav
```

然后继续跑：

```powershell
python .\tools\analyze_audio_quality.py .\artifacts\sample_mp4_full_verified_20260628_audio.wav
```

---

## 5. 对剩余 Claim 的意义

这一步还没有把 `THD < 5%` 变成已达成结果，因为我仍然缺“针对标准单频输入、由板端实际录回来的专用样本”。

但它已经把 `THD` 的后处理链继续补完整了：

1. 标准输入 WAV：已有
2. 板端录制 MP4：已有流程
3. MP4 导出 WAV：已补
4. WAV 做 THD 分析：已有

也就是说，`THD` 现在离真正做实，差的已经不是脚本链条，而是一次真实板端采样。

---

## 6. 后续进展

在这份记录之后，我又继续把这条链做了实跑验证，见：

- `docs/audio_post_capture_chain_verified_20260630.md`

结论：

- `MP4 -> WAV -> 音频分析` 已经真实跑通
- 但现有 MP4 音频不是标准单频输入，因此不能拿它来证明 `THD < 5%`
