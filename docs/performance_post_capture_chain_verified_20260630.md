# 性能 / I/O 对比后处理链验证记录（2026-06-30）

## 1. 这次做了什么

为了验证新增的“router 日志对比脚本”不是空壳，我直接拿当前项目里已有的两份历史日志做了一次对比验证：

- baseline: `old_qp_console.log`
- candidate: `qp_limited_console.log`

使用脚本：

- `tools/compare_router_metrics.py`

---

## 2. 实际输出结果

脚本成功输出了结构化对比结果，核心字段如下：

- `video_fps_change_percent = 0.0`
- `video_fail_ratio_change_percent = -4.38`
- `video_ok_change_percent = 0.451`
- `audio_ok_change_percent = 0.287`

同时保留了两份原始日志各自的解析结果：

- `estimated_fps_from_pts`
- `video_ok / audio_ok / fail`
- `video_fail_ratio`

---

## 3. 如何解释这个结果

这个结果**不能**被拿来写成：

- `封装速度提升 40%`
- `I/O 调用次数下降 60%`

因为当前对比仍然存在两个本质限制：

1. 不是统一设计出来的基线版本与目标版本
2. 没有同步采到 CPU / I/O 系统调用层数据

但它已经能够说明：

- 这条性能对比后处理链已经真实可用
- 只要后面拿到两份统一条件下的板端日志，就能立刻出结构化比较

---

## 4. 当前真正剩下的缺口

到这一步，性能 / I/O 这条 Claim 剩下的主要缺口是：

- 一次严格统一条件下的双版本板端采样
- 同步保留 CPU / I/O 统计

也就是说，现在缺的主要是实验设计与实采样，而不是日志解析或对比工具。
