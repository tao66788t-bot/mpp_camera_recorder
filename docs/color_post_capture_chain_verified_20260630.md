# 色彩 ΔE 后处理链验证记录（2026-06-30）

## 1. 这次做了什么

为了验证 “色彩 before/after 改善比例” 这条后处理链不仅在纸面上存在，而且真的能跑，我做了一个受控的合成验证：

1. 使用标准色卡  
   `assets/color_test_card_1080p.png`
2. 生成一个“更差”的暖色偏亮变体  
   `tools/generate_color_test_card_variants.py`
3. 用新的改善比例分析脚本计算 before/after 的 `ΔE` 改善比例  
   `tools/analyze_color_improvement.py`

---

## 2. 实际样本

### 2.1 标准参考图

- `assets/color_test_card_1080p.png`

### 2.2 合成劣化图

- `artifacts/color_test_card_warmer_brighter.png`

### 2.3 ROI 模板

- `assets/color_test_card_1080p_patches.json`

---

## 3. 实际输出结果

脚本输出了完整结果，核心字段如下：

- `before avg_delta_e76 = 16.113`
- `after avg_delta_e76 = 0.0`
- `improvement_percent = 100.0`
- `interpretation = after image is closer to reference colors`

并且每个色块都给出了：

- `before_delta_e76`
- `after_delta_e76`
- `delta_improvement`

---

## 4. 如何解释这个结果

这个结果**不能**被直接拿来写成：

- “项目真实色彩提升 100%”

因为这次用的是合成劣化图，不是真实板端 before/after 拍摄样本。

但它能够严格说明一件事：

- `ΔE` 改善比例的后处理链已经真实跑通

也就是说，现在不仅能算：

- 单张图相对参考色块的 `ΔE`

还能直接算：

- before/after 平均 `ΔE` 的改善比例

---

## 5. 当前真正剩下的缺口

到这一步，色彩提升这条 Claim 距离真正做实，只差：

- 一组真实板端 before/after 拍摄样本

然后把它们喂给这条后处理链，就能得到真实可引用的改善比例。

所以现在缺的已经不是算法和脚本，而是一次规范化的板端采样。
