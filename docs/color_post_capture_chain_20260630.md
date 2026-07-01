# 色彩 ΔE 后处理链补充记录（2026-06-30）

## 1. 目标

前面我已经补了：

- 色卡图
- ROI JSON
- 基础 `ΔE76` 分析脚本

这次继续往前推，把它从“能分别分析 before/after”推进到：

- 直接计算 before/after 的平均 `ΔE` 改善比例

也就是让后处理链更接近简历里常见的表述方式：

- “色彩还原提升 X%”

---

## 2. 新增脚本

### 2.1 改善比例分析

- `tools/analyze_color_improvement.py`

功能：

- 输入 `before.jpg`
- 输入 `after.jpg`
- 输入 ROI JSON
- 输出：
  - before 平均 `ΔE`
  - after 平均 `ΔE`
  - 改善比例 `improvement_percent`

### 2.2 合成变体验证

- `tools/generate_color_test_card_variants.py`

功能：

- 基于标准色卡生成一个“更差”的合成版本
- 用来验证改善比例分析链本身是否工作正常

---

## 3. 当前意义

到这一步为止，`ΔE` 这条链已经和 `THD / AV sync` 一样，进入了“采样后直接出量化结果”的阶段：

1. 有标准输入色卡
2. 有 ROI JSON
3. 有单图 `ΔE` 分析
4. 有 before/after 改善比例分析

所以当前真正剩下的主要缺口，也进一步收敛成了：

- 一组真实板端 before/after 拍摄样本

---

## 4. 后续进展

在这份记录之后，我又用标准色卡和合成劣化变体做了一次链路自测，见：

- `docs/color_post_capture_chain_verified_20260630.md`

结论：

- `ΔE` 改善比例分析链已经真实跑通
- 但当前验证样本是合成图，所以不能直接拿它替代真实板端提升数据
