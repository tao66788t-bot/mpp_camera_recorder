# IPC 项目简历量化证据总索引（2026-06-30）

优先阅读顺序：

1. `docs/resume_claim_master_control_20260630.md`
2. `docs/resume_claim_status_auto_20260630.md`
3. `docs/resume_claim_ledger_strict_20260630.md`
4. `docs/ipc_metrics_fulfillment_matrix_20260630.md`

## 1. 这份索引的作用

这份文档不是再写一份项目总结，而是把我这段时间为“简历量化项做实”补出来的所有关键材料，按证据强弱统一归档。

这样后面不管是：

- 改简历
- 准备自我介绍
- 准备面试追问
- 继续补实验

我都不需要再到处翻文档，而是直接从这份索引里定位。

---

## 2. 第一层：已经做实、可直接写进简历

这些内容已经有当前工程的代码、日志、样本文件或分析脚本支撑，可以直接写进简历。

### 2.1 主链路事实

- LCD 本地预览：`480x800`
- 编码录制：`1920x1088@20fps`
- RTSP 服务端口：`554`
- 音频真实参数：`16000Hz / 16bit / 单声道 / 16kbps`
- 人脸检测输入：`480x288 NV12`
- 人脸检测阈值：`0.60`

对应材料：

- `docs/ipc_resume_bullet_safe_version_20260630.md`
- `docs/ipc_metrics_fulfillment_matrix_20260630.md`
- `tools/collect_resume_metrics.py`
- `tools/analyze_mp4_av_summary.py`

### 2.2 已闭环优化项

- H.265 帧大小离散系数：`CV 143.70% -> 92.70%`

对应材料：

- `docs/venc_qp_cv_optimization_20260628.md`
- `tools/analyze_mp4_cv.py`

### 2.3 LCD 修复量化证据

- 修复图 vs 正确 NV12 参考图：
  - `MAE = 0.000`
  - `RMSE = 0.000`
- 错误 NV21 解释 vs 参考图：
  - `MAE = 15.034`
- 旧异常图 vs 参考图：
  - `MAE = 69.199`

对应材料：

- `docs/lcd_preview_porting_record_20260627.md`
- `docs/lcd_color_consistency_quant_record_20260630.md`
- `tools/analyze_lcd_color_consistency.py`

---

## 3. 第二层：已有粗粒度证据，但还不能夸大

这些内容已经不是“完全没证据”，但也还没到能写成强结论的程度。

### 3.1 MP4 音画同步

当前我能证明的是：

- 音频轨和视频轨在容器级时长上是接近的
- 没有出现明显轨道漂移

当前样本结果：

- `sample_mp4_full_verified_20260628.mp4`：时长差约 `394ms`
- `qp_limited_graceful.mp4`：时长差约 `453ms`

对应材料：

- `docs/av_sync_coarse_validation_20260630.md`
- `tools/analyze_mp4_av_sync.py`

当前不能直接写成：

- `AV sync < 30ms`

---

## 4. 第三层：实验底座已经补齐，但还缺真实闭环样本

这部分是我这次推进得最关键的一类。

### 4.1 音频 THD

当前状态：

- 标准 `1kHz` 正弦波生成工具已补齐
- 音频质量 / 粗略 THD 分析工具已补齐
- 对标准 `1kHz` WAV，脚本可以稳定识别：
  - `dominant_freq_hz = 1000.0`
  - `dominant_ratio ≈ 0.4999`

对应材料：

- `docs/audio_thd_boundary_record_20260630.md`
- `docs/audio_thd_experiment_ready_20260630.md`
- `tools/generate_sine_wave.py`
- `tools/analyze_audio_quality.py`

当前不能直接写成：

- `THD < 5%`

因为还缺：

- 标准正弦波真正经过板端采集/编码链路后的闭环样本

### 4.2 色彩 ΔE / “提升 20%”

当前状态：

- Lab / ΔE 分析脚本已补齐
- ROI 模板已补齐
- 工具链已经可以对任意色块做 `RGB -> Lab -> ΔE76` 分析

对应材料：

- `docs/color_deltae_experiment_ready_20260630.md`
- `tools/analyze_color_delta_e.py`
- `tools/color_patches_template.json`

当前不能直接写成：

- `色彩还原提升 20%`

因为还缺：

- 真实色卡实拍样本
- 修复前/修复后同场景对照

### 4.3 性能类数字

当前状态：

- 固件日志里已有 router 性能入口
- 路由日志解析工具已补齐
- 已对 3 份真实历史运行日志完成第一次离线解析
- 已能从日志里提取：
  - 视频推进情况
  - 音频推进情况
  - `fail ratio`
  - 基于 `PTS` 的粗吞吐估算

当前已拿到的粗证据：

- `old_qp_console.log`、`qp_limited_console.log`、`qp_baseline_timeout25_console.log`
- 都出现 `ALL:1080p MP4(A/V)+RTSP(TCP:554 A/V)+VO(480x800) LCD`
- 视频 router 侧按 `PTS` 反推的粗速率约为 `19.818 fps`
- 两份完整退出日志可提取：
  - `video_ok / audio_ok / fail`
  - `video_fail_ratio ≈ 0.4109` 与 `0.3929`

对应材料：

- `docs/performance_benchmark_ready_20260630.md`
- `tools/analyze_router_log_metrics.py`
- `tools/router_log_sample.txt`

当前不能直接写成：

- `封装速度 +40%`
- `I/O 调用次数 -60%`

因为还缺：

- 一个同条件可比的基线版本
- 统一采样下的 CPU / I/O / 吞吐对照

---

## 5. 第四层：简历和面试可直接引用的成品

这几份已经不是“原始证据”，而是我整理好的直接可用输出。

### 5.1 简历条目

- `docs/ipc_resume_bullet_safe_version_20260630.md`

作用：

- 直接告诉我简历里该怎么写，哪些数字可保留

### 5.2 自我介绍 / 投递讲稿

- `docs/ipc_resume_pitch_final_20260630.md`

作用：

- 自我介绍时的 30 秒 / 1 分钟讲法

### 5.3 面试深挖问答

- `docs/ipc_interview_qa_final_20260630.md`

作用：

- 回答“你怎么证明是真的”“LCD 为什么这样修”“为什么不写更多漂亮数字”这类高压问题

### 5.4 第一人称总复盘

- `docs/ipc_resume_project_full_record_20260630.md`

作用：

- 以第一人称把整段“数字纠偏、逐项做实”的过程串起来

### 5.5 严格 claim 核销清单

- `docs/resume_claim_ledger_strict_20260630.md`

作用：

- 专门区分：
  - 当前代码里真实存在的功能
  - 旧文档数字与当前工程冲突的部分
  - 学长原稿里提过、但当前工程根本没有实现的子系统

这份文档特别适合防止面试时把：

- `Opus`
- `ADEC/AO`
- `AEC/ANS/AGC`
- `44.1kHz / 双声道 / 128kbps`

这些旧版本内容误说成当前工程实现。

### 5.6 现场实验剧本与自动 gatecheck

- `docs/resume_claim_field_playbook_20260630.md`
- `tools/resume_claim_gatecheck.py`

作用：

- 前者把剩余 claim 压成“拿到板子就能执行”的实验步骤
- 后者一键输出当前 claim 的证据层级：
  - `strong`
  - `coarse`
  - `experiment_ready`

也会同时提示当前代码里哪些旧模块其实并不存在，例如：

- `Opus`
- `ADEC/AO`
- `AEC/ANS/AGC`

### 5.7 自动状态页与实验记录模板

- `docs/resume_claim_status_auto_20260630.md`
- `docs/resume_claim_experiment_record_template_20260630.md`
- `tools/generate_resume_claim_status_report.py`

### 5.7 MP4 证据样本筛选

- `docs/mp4_sample_evidence_screening_20260630.md`
- `tools/screen_mp4_evidence_samples.py`

用途：

- 区分当前哪些 MP4 可以当作强证据样本
- 自动标记 `moov box not found` 一类坏样本
- 避免把损坏录制文件误用到简历量化分析中

作用：

- 自动状态页把当前所有 claim 汇总成可直接阅读的 Markdown 报告
- 实验记录模板用于后续每跑完一轮现场实验后，按统一结构沉淀结果
- 这样后面可以形成：
  - 跑实验
  - 出脚本结果
  - 生成状态页
  - 填实验记录

的完整闭环

---

## 6. 当前最重要的结论

如果只看“简历量化项做实”这件事，我现在已经完成了三件很核心的工作：

1. 把当前工程里真实已经做实的数字和旧模板数字彻底分开了。
2. 把一部分原本只能靠主观描述的问题，做成了可量化证据。
3. 对暂时还不能写死的指标，也补齐了实验底座，而不是留在空泛承诺阶段。

这意味着后面我再讲这个项目时，已经不再是：

- “我记得大概提升过很多”

而是：

- “哪些数字我已经做实了，哪些还在实验准备阶段，我都能拿证据说清楚。”
### 5.8 实验素材资产

- `docs/experiment_assets_generation_20260630.md`
- `assets/thd_1khz_16k_mono.wav`
- `assets/color_test_card_1080p.png`
- `assets/color_test_card_1080p_patches.json`
- `assets/av_sync_test_page_1080p.png`

用途：

- 为 `THD / ΔE / AV sync` 的板端闭环实验提供标准输入素材

### 5.9 音频录制后处理链

- `docs/audio_post_capture_analysis_chain_20260630.md`
- `docs/audio_post_capture_chain_verified_20260630.md`
- `tools/extract_audio_from_mp4.py`

用途：

- 验证 `MP4 -> WAV -> 音频质量分析` 这条后处理链已经可用
- 说明现有 MP4 样本为何不能直接拿来证明 `THD < 5%`

### 5.10 AV Sync 事件级后处理链

- `docs/av_sync_event_analysis_ready_20260630.md`
- `docs/av_sync_event_chain_verified_20260630.md`
- `tools/analyze_av_sync_event.py`

用途：

- 把 `AV sync` 从 track-duration 粗验证推进到事件级后处理
- 说明现有普通 MP4 样本为什么还不能直接证明 `<30ms`

### 5.11 色彩 ΔE 后处理链

- `docs/color_post_capture_chain_20260630.md`
- `docs/color_post_capture_chain_verified_20260630.md`
- `tools/analyze_color_improvement.py`
- `tools/generate_color_test_card_variants.py`

用途：

- 把色彩分析从“单张 ΔE”推进到“before/after 改善比例”
- 说明当前为什么还缺真实板端 before/after 样本

### 5.12 合成理想样本验证

- `docs/synthetic_validation_20260630.md`
- `tools/generate_synthetic_av_sync_clip.py`

用途：

- 用标准正弦波和合成 `flash + beep` 样本验证 `THD / AV sync` 工具链本身工作正常
- 将“工具可信性”和“板端真实结果”区分开

### 5.13 自动 Ready 报告

- `docs/resume_claim_readiness_auto_20260630.md`
- `tools/generate_resume_claim_readiness_report.py`

用途：

- 自动汇总哪些 Claim 已可直接写进简历
- 自动区分“工具链已验证但仍缺板端标准化采样”的项
- 避免后续再把弱证据和强证据混写
