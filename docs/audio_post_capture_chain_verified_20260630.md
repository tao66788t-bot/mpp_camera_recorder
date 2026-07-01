# 音频录制后处理链验证记录（2026-06-30）

## 1. 这次做了什么

为了继续推进 `THD < 5%` 这个还没完全做实的 Claim，我把“板端 MP4 录制完成之后，如何在本机继续做音频分析”这条后处理链实际跑通了一遍。

这次验证的链路是：

1. 取现有 MP4 样本  
   `sample_mp4_full_verified_20260628.mp4`
2. 从 MP4 中提取音频为 WAV  
   `tools/extract_audio_from_mp4.py`
3. 对提取出的 WAV 做频谱/THD 边界分析  
   `tools/analyze_audio_quality.py`

---

## 2. 遇到的问题

### 2.1 系统 PATH 里没有 ffmpeg

一开始直接调用 `ffmpeg` 失败，因为本机 `PATH` 里没有该命令。

### 2.2 系统自带的极简 ffmpeg 只能 mux MP4

虽然本机里找到了一个：

- `C:\Trae CN\resources\app\bin\ffmpeg.exe`

但它是极简版构建，支持：

- demux `mov/mp4`
- mux `mp4`

不支持：

- mux `wav`

所以直接导出 WAV 会失败。

### 2.3 并行执行导致分析抢跑

我第一次把“提取音频”和“分析音频”并行跑了，结果分析进程在 WAV 还没落盘时就启动，直接报了：

- `FileNotFoundError`

这不是功能问题，而是时序问题。

---

## 3. 采取的修复

### 3.1 安装可用的完整 ffmpeg

我安装了：

- `imageio-ffmpeg`

它自带了一份完整的 ffmpeg，可执行文件路径为：

- `C:\Users\86150\AppData\Roaming\Python\Python311\site-packages\imageio_ffmpeg\binaries\ffmpeg-win-x86_64-v7.1.exe`

这份 ffmpeg 支持：

- `wav`
- `s16le`
- `adts`
- 多种常见音频输出格式

### 3.2 修改音频提取脚本

我在 `tools/extract_audio_from_mp4.py` 里做了两点修正：

1. 优先自动使用 `imageio-ffmpeg` 自带的完整 ffmpeg
2. 显式指定输出格式为 `wav`

### 3.3 改为串行执行

先提取 WAV，再启动分析脚本，避免并行时序误伤。

---

## 4. 实际验证结果

### 4.1 MP4 -> WAV 提取成功

输出文件：

- `artifacts/sample_mp4_full_verified_20260628_audio.wav`

提取后的格式：

- `16000 Hz`
- `mono`
- `pcm_s16le`

### 4.2 音频分析成功运行

对上述 WAV 跑 `tools/analyze_audio_quality.py` 后，得到结果：

- `sample_rate = 16000`
- `channels = 1`
- `duration_sec = 98.24`
- `dominant_freq_hz = 1.953125`
- `dominant_ratio = 0.0755`
- `thd_percent = 132.71`
- `analysis_note = broadband or complex audio; THD value is not suitable as a resume metric`

---

## 5. 如何解释这个结果

这次结果**不是**说明“项目音频链路失真率真的有 132.71%”，而是说明：

- 这份 MP4 里的音频本来就不是标准单频正弦波
- 因此它不适合拿来做 THD 指标背书
- 分析脚本也正确识别出了这一点，并给出了“不适合作为 THD 指标”的提示

这反而验证了两件事：

1. 后处理链是通的  
   `MP4 -> WAV -> THD 分析` 已经真实打通
2. 我没有把不合格样本硬说成“已经满足 THD < 5%”

---

## 6. 当前真正剩下的缺口

到这一步，`THD` 离真正做实，只差一件事：

- 用标准 `1kHz` 正弦波输入，对板端做一次真实录制，再把录回的 MP4 按这条链分析

也就是说，现在缺的已经不是工具链，而是一次真实板端采样。

---

## 7. 工具链理想样本自测补充

另外我还对 `THD` 分析工具做了一次标准正弦波理想样本自测，见：

- `docs/synthetic_validation_20260630.md`

结果表明：

- 对 `1kHz` 标准正弦波，脚本可得到接近理想值的极低 `thd_percent`

这进一步说明当前真正缺的是板端实采样，而不是分析逻辑本身。
