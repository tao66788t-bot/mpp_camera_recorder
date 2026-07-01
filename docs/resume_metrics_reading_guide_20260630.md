# IPC 项目量化材料阅读导航（2026-06-30）

## 1. 这份导航的作用

这次我围绕简历里的量化项，额外补了一批审计文档、证据索引和分析脚本。

为了避免后面回看时出现：

- 文档很多
- 每份都能看
- 但不知道应该先看哪份

我单独整理这份阅读导航，按“先总览、再落简历、再备面试、最后补实验”的顺序给出入口。

---

## 2. 如果我现在要改简历，先看哪几份

### 第一份：总索引

先看：

- `docs/ipc_resume_evidence_index_20260630.md`

作用：

- 总览所有量化项当前属于哪一类
- 快速区分：
  - 已验证可写
  - 只有粗证据
  - 还只是实验准备完成

### 第二份：安全版简历条目

然后看：

- `docs/ipc_resume_bullet_safe_version_20260630.md`

作用：

- 这是最适合直接摘到简历里的版本
- 已经把不稳妥的数字剔掉了

### 第三份：量化兑现矩阵

接着看：

- `docs/ipc_metrics_fulfillment_matrix_20260630.md`

作用：

- 一条条看每个数字现在到底能不能写
- 如果面试前想快速确认边界，这份最直接

### 第四份：严格 claim 核销清单

再看：

- `docs/resume_claim_ledger_strict_20260630.md`

作用：

- 这份不是看“数字有没有”，而是看“当前工程里到底有没有这个子系统”
- 特别适合防止把学长版本中的：
  - `Opus`
  - `ADEC/AO`
  - `AEC/ANS/AGC`
  - `44.1kHz / 双声道 / 128kbps`
  误说成当前实现

---

## 3. 如果我要准备面试，先看哪几份

### 第一份：第一人称完整项目记录

- `docs/ipc_resume_project_full_record_20260630.md`

作用：

- 用“我做了什么、为什么这样做、怎么验证”的方式串起来
- 适合转成自己的项目故事

### 第二份：面试问答稿

- `docs/ipc_interview_qa_final_20260630.md`

作用：

- 针对深挖问题准备回答
- 适合面试前快速过一遍

### 第三份：项目讲述稿

- `docs/ipc_resume_pitch_final_20260630.md`

作用：

- 适合自我介绍或项目介绍时直接参考表述

---

## 4. 如果我要查某个量化项的证据，看哪几份

### LCD 预览 / 偏色修复

- `docs/lcd_color_consistency_quant_record_20260630.md`

能回答的问题：

- 为什么最后判断问题不在 ISP
- 为什么说是 `VI2 -> VO` 预览链路的问题
- 为什么 `NV12/NV21` 混用会导致 LCD 偏色
- 修复后为什么能说“和正确参考图一致”

### 音频失真率 THD

- `docs/audio_thd_boundary_record_20260630.md`
- `docs/audio_thd_experiment_ready_20260630.md`

能回答的问题：

- 为什么现在还不能硬写 `THD < 5%`
- 现有样本为什么不适合做 THD 指标
- 后面如果真要闭环，实验该怎么做

### 色彩提升 / ΔE

- `docs/color_deltae_experiment_ready_20260630.md`

能回答的问题：

- 为什么现在不能直接写“色彩还原提升 20%”
- 如果后面补色卡实验，怎么把它做实

### 音视频同步

- `docs/av_sync_coarse_validation_20260630.md`

能回答的问题：

- 为什么当前只能说“没有明显大漂移”
- 为什么还不能写 `< 30ms`

### 性能 / 封装吞吐 / I/O

- `docs/performance_benchmark_ready_20260630.md`

能回答的问题：

- 为什么现在不能直接写 `+40%` 或 `-60%`
- 已经准备好了哪些日志和分析方法

### 剩余 claim 现场闭环

- `docs/resume_claim_field_playbook_20260630.md`
- `tools/resume_claim_gatecheck.py`

能回答的问题：

- 剩余几项 claim 现场到底该怎么测
- 当前每项 claim 属于 `strong / coarse / experiment_ready` 哪一层
- 哪些旧模块当前代码里其实并不存在

### 实验结果沉淀

- `docs/resume_claim_status_auto_20260630.md`
- `docs/resume_claim_experiment_record_template_20260630.md`
- `tools/generate_resume_claim_status_report.py`

能回答的问题：

- 当前所有 claim 的自动汇总状态是什么
- 现场跑完实验之后，结果该按什么格式沉淀
- 怎么把脚本输出快速变成可读的 Markdown 状态页

---

## 5. 如果我要看脚本，在哪

脚本都在：

- `tools/`

重点如下：

- `tools/collect_resume_metrics.py`
  - 从代码里抽当前真实配置
- `tools/analyze_mp4_av_summary.py`
  - 看 MP4 中音视频真实参数
- `tools/analyze_lcd_color_consistency.py`
  - 做 LCD 修复前后图像一致性对比
- `tools/analyze_audio_quality.py`
  - 做正弦波/音频质量分析
- `tools/generate_sine_wave.py`
  - 生成标准测试音
- `tools/analyze_color_delta_e.py`
  - 做色卡 ΔE 计算
- `tools/analyze_mp4_av_sync.py`
  - 做 MP4 粗粒度音视频同步分析
- `tools/analyze_router_log_metrics.py`
  - 做 RTSP/MUX 路由日志吞吐统计

---

## 6. 当前最推荐的阅读顺序

如果时间很少，我建议只按下面顺序看：

1. `docs/ipc_resume_evidence_index_20260630.md`
2. `docs/ipc_resume_bullet_safe_version_20260630.md`
3. `docs/resume_claim_ledger_strict_20260630.md`
4. `docs/ipc_resume_project_full_record_20260630.md`
5. `docs/ipc_interview_qa_final_20260630.md`

如果后面要继续把剩余量化项彻底做实，再补看：

1. `docs/audio_thd_experiment_ready_20260630.md`
2. `docs/color_deltae_experiment_ready_20260630.md`
3. `docs/av_sync_coarse_validation_20260630.md`
4. `docs/performance_benchmark_ready_20260630.md`
5. `docs/resume_claim_field_playbook_20260630.md`
6. `docs/resume_claim_status_auto_20260630.md`

---

## 7. 当前结论

这批材料里，真正应该当成“主入口”的不是零散单篇文档，而是：

- `ipc_resume_evidence_index_20260630.md`
- `ipc_resume_bullet_safe_version_20260630.md`
- `ipc_resume_project_full_record_20260630.md`

后面无论是继续改简历、准备面试，还是继续补实验，我都应该从这三份往下展开，而不是重新在整个 `docs/` 目录里翻。
