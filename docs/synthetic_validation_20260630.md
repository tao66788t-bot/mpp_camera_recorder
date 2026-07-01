# 后处理工具合成验证记录（2026-06-30）

## 1. 目标

前面我已经把三条还没完全做实的弱项链路补到了可执行状态：

- `THD`
- `AV sync`
- `ΔE`

但如果只在普通真实样本上运行，还不足以证明工具本身的检测逻辑靠谱。所以我又补了一层“合成可控样本验证”，目标是：

- 在我能控制事件位置和输入信号的前提下
- 验证工具是否能给出合理结果

---

## 2. 新增脚本

- `tools/generate_synthetic_av_sync_clip.py`

用途：

- 生成一段可控的 `flash + beep` 合成 MP4
- 让事件级同步分析脚本在理想样本上做自检

---

## 3. 这层验证的意义

这一步仍然不能替代真实板端数据，但它能证明：

- 工具逻辑本身不是拍脑袋
- 事件检测、提取、分析链在“理想条件”下是可工作的
- 以后如果板端实测结果怪异，更容易区分是“工具问题”还是“样本问题”

---

## 4. 实际验证结果

### 4.1 THD 工具链在标准 1kHz 正弦波上验证通过

输入样本：

- `assets/thd_1khz_16k_mono.wav`

分析结果：

- `dominant_freq_hz = 1000.0`
- `dominant_ratio ≈ 0.4999`
- `thd_percent ≈ 0.00149`
- `analysis_note = dominant tone is clear enough for rough THD estimation`

说明：

- 对标准单频输入，`THD` 分析脚本能给出合理且接近理想值的结果

### 4.2 AV sync 工具链在合成 flash+beep MP4 上验证通过

合成样本：

- `artifacts/synthetic_flash_beep.mp4`

脚本输出：

- `video_event_time_sec = 1.0`
- `audio_event_time_sec = 1.000125`
- `audio_minus_video_ms = 0.125`
- `interpretation = event-level alignment looks promising`

说明：

- 在事件位置可控的理想样本上，事件级同步分析脚本能给出接近 0 的偏差

---

## 5. 当前结论

这两组结果不能直接替代真实板端数字，但它们已经证明：

- `THD` 工具链本身在标准正弦波上工作正常
- `AV sync` 工具链本身在标准 flash+beep 样本上工作正常

因此后面如果真实板端数据结果不理想，优先应怀疑：

- 采样条件
- 板端链路本身
- 事件设计

而不是先怀疑分析脚本完全不可信
