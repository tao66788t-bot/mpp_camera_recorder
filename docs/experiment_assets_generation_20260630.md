# IPC 项目实验素材生成记录（2026-06-30）

## 1. 目标

为了把剩余几个还没完全做实的 Claim 继续往前推，我补了三类可复用实验素材生成能力：

- `THD`：标准单频正弦波输入文件
- `ΔE / 色彩`：标准色卡图与配套 ROI 模板
- `AV sync`：可直接显示在屏幕上的同步测试页

这些素材本身不等于“指标已经达成”，但它们把后续实验从“口头方法”推进成了“可以直接开测”的状态。

---

## 2. 新增脚本

### 2.1 音频正弦波

- `tools/generate_sine_wave.py`

用途：

- 生成 `16kHz / mono / 16bit` 的标准正弦波 WAV
- 用于板端 `AI -> AENC -> MP4/RTSP` 链路的 THD 实验

### 2.2 色卡与 ROI 模板

- `tools/generate_color_test_card.py`

用途：

- 生成一张标准色卡 PNG
- 同时生成配套 ROI JSON
- 方便直接喂给 `tools/analyze_color_delta_e.py`

### 2.3 音视频同步测试页

- `tools/generate_av_sync_test_page.py`

用途：

- 生成可直接全屏显示的同步测试页
- 用于“闪光 + 蜂鸣”或“拍手”一类事件级同步实验

---

## 3. 建议生成命令

### 3.1 THD 音频源

```powershell
python .\tools\generate_sine_wave.py `
  --output .\assets\thd_1khz_16k_mono.wav `
  --sample-rate 16000 `
  --duration-sec 10 `
  --freq-hz 1000 `
  --amplitude 0.5 `
  --channels 1
```

### 3.2 色卡图与 ROI

```powershell
python .\tools\generate_color_test_card.py `
  --image-output .\assets\color_test_card_1080p.png `
  --patches-output .\assets\color_test_card_1080p_patches.json `
  --width 1920 `
  --height 1080
```

### 3.3 AV 同步测试页

```powershell
python .\tools\generate_av_sync_test_page.py `
  --output .\assets\av_sync_test_page_1080p.png `
  --width 1920 `
  --height 1080
```

---

## 4. 这些素材分别服务哪些 Claim

| Claim | 素材 | 配套分析工具 |
|------|------|------|
| `THD < 5%` | `thd_1khz_16k_mono.wav` | `tools/analyze_audio_quality.py` |
| 色彩提升 `20%` | `color_test_card_1080p.png` + ROI JSON | `tools/analyze_color_delta_e.py` |
| `AV sync < 30ms` | `av_sync_test_page_1080p.png` | 现有粗分析 + 后续事件级对齐 |

---

## 5. 当前结论

到这一步为止：

- `THD`、`ΔE`、`AV sync` 仍然没有被我“伪装成已经完成”
- 但后续板端实验已经不再缺标准输入素材
- 这让剩余 Claim 的状态，从“只有口头方案”进一步推进成了“有现成实验资产、可以直接采样”
