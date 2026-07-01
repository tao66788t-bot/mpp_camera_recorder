# 性能 / I/O 对比后处理链补充记录（2026-06-30）

## 1. 目标

当前 `Packaging +40% / I/O -60%` 这条 Claim 仍然是最弱的一条。

它和 `THD / AV sync / ΔE` 不一样，主要缺的不是单个分析脚本，而是：

- 缺统一条件下的真实板端采样
- 缺一份标准化的“版本 A vs 版本 B”对比输出

所以这次我补的是“日志对比后处理链”，目标不是直接宣称 `+40% / -60%`，而是让未来一旦拿到两份统一条件下的日志，就能立即出结构化对比结果。

---

## 2. 新增脚本

- `tools/compare_router_metrics.py`

功能：

- 读取两份 router 日志
- 复用 `tools/analyze_router_log_metrics.py`
- 输出：
  - `video_fps_change_percent`
  - `video_fail_ratio_change_percent`
  - `video_ok_change_percent`
  - `audio_ok_change_percent`

---

## 3. 当前意义

到这一步为止，这条性能链已经从：

- 只有几份历史 log

推进成了：

- 有统一解析脚本
- 有统一对比脚本
- 只差板端同条件采样

所以它虽然还不能升级成“工具链已验证、只差采样”那么强，但至少已经不再是纯人工比日志。

---

## 4. 后续进展

在这份记录之后，我又拿现有两份历史日志做了一次对比脚本实跑验证，见：

- `docs/performance_post_capture_chain_verified_20260630.md`

结论：

- 对比脚本已经能稳定输出结构化差异
- 但现有样本仍然不足以支撑 `+40% / -60%` 这类最终量化结论
