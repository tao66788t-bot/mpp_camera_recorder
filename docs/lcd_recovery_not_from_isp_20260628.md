# LCD 恢复记录（2026-06-28）

## 结论先说

这次 LCD 恢复正常，**不是靠“重新找回 ISP context 文件”完成的**。

按当前现场结论：

- 板子上的 `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin` **存在**
- 文件大小为 **34300 字节**
- MP4 录像链路正常
- Windows 侧 RTSP 解码正常

因此这次问题不能再定性成“ISP context 缺失导致 LCD 白屏/黄屏”。  
更准确的结论是：

> **ISP 主链路是通的，LCD 异常主要是 VI2→VO 预览链路配置与板端运行态管理问题。**

---

## 为什么可以排除“靠 ISP 文件找回恢复”

### 1. 当前 ISP context 已经是正常状态

现场已确认：

- 路径：`/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`
- 大小：`34300` 字节

这说明当前板子并不是“没有 ISP context 文件”的状态。

### 2. MP4 画面正常

录像链路：

- `VI0(dev=0) -> VENC(H.265) -> MUX -> MP4`

如果 ISP/sensor 主链路真的还处在严重错误状态，录像结果通常也会一起发白、过曝、全白。  
但当前 MP4 结果正常，说明：

- sensor 输出正常
- ISP 主处理链路可用
- 编码链路可用

### 3. RTSP 到 Windows 也能正常解码

当前主线代码已经验证过：

- 板端 RTSP server 能稳定送出 H.265 码流
- Windows VLC 能正常解码并出图

这进一步说明：

- `VI0 -> VENC -> RTSP` 主链路是健康的
- 不是“整套 ISP 都坏了”

### 4. 当前 LCD 关键代码本身保持着已知正确基线

项目里已有已知可工作基线：`backup_20260627_lcd_fixed/`

从现有代码和该备份的对照可以确认，LCD 成功依赖的关键点仍然保持为：

- `VI2(dev=4)`
- `480x800`
- `MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 (NV12)`
- `V4L2_COLORSPACE_JPEG`
- `use_current_win = 0`
- `VI2 -> VO layer 4` 绑定

也就是说，后续加音频、RTSP 时，**LCD 的关键配置并没有被重新改回错误值**。

---

## 这次真正起作用的东西是什么

### 1. 用已知正确的 LCD 关键参数作为基线

LCD 预览链路能正常的前提是：

- `VI2` 必须是 `NV12`
- `VI2.color_space` 必须保持 `V4L2_COLORSPACE_JPEG`
- `VI2.use_current_win` 必须保持 `0`
- `VO` 必须继续使用 `layer 4 / chn 0 / 480x800`

这部分是**代码基线**，不是靠猜。

### 2. 真正完整地执行“push -> reboot -> 等待 -> ADB 重连 -> 再 push -> 单实例启动”

这次反复现象不稳定，核心不是“代码总在变”，而是**板端运行态非常脏**：

- ISP/VE/VO 这类硬件模块会残留状态
- reboot 后 `UDISK` 挂载不稳，第一次 push 的程序不一定就是最终跑起来的程序
- 固件重复启动会占住 `video0`，直接把 LCD 搞到物理异常

所以这类问题里，下面这条流程不是“建议”，而是**恢复条件**：

1. `adb push ipc_firmware /mnt/UDISK/ipc_firmware`
2. `adb shell reboot`
3. `sleep 15`
4. `adb kill-server && adb start-server && sleep 2`
5. `adb devices` 确认 `20080411 device`
6. 因 `UDISK` 不稳，再补一次 `adb push`
7. 只启动一次固件
8. `adb shell "ps | grep ipc"` 确认只有一个实例

### 3. 区分“代码问题”和“板端状态问题”

这次最大的经验不是某个参数，而是判断顺序：

- 如果 **MP4/RTSP 正常，只有 LCD 异常**
  - 先看 `VI2->VO`
  - 再看部署/reboot/实例数
  - 不要第一反应就是 ISP context
- 如果 **MP4、RTSP、LCD 一起白/一起异常**
  - 再回头检查 ISP context、sensor、AE/ISP 主链路

---

## 为什么之前会反反复复像“没恢复”

### 1. 把“历史 ISP 事故”误当成“当前唯一根因”

这个项目确实发生过 ISP context 被误删。  
但后面现场条件已经变了：

- context 文件已经恢复存在
- MP4/RTSP 主链路正常

如果这时还一直围绕 ISP 打转，就会偏离真正的问题面。

### 2. 板子重启后的运行态不干净

这个板子不是“代码一改、现象就立刻可信”的平台。

必须承认几个现实：

- 不 reboot，很多现象不可信
- reboot 后不重连 ADB、不补 push，现象也不可信
- 重复启动固件，现象同样不可信

### 3. 看到 LCD 发黄/发白/没图，不能立刻等于“代码又坏了”

在这个项目里，下面三种现象都可能由**运行态**引起：

- 发黄
- 发白
- 黑屏/没图

所以先做运行态清理，再判断代码，是必须的。

---

## 后续排查 LCD 时的标准判断顺序

### 先看现象分层

#### 情况 A：MP4 正常，RTSP 正常，只有 LCD 不正常

优先检查：

1. 当前是否真的跑的是目标二进制
2. reboot 是否完整执行
3. reboot 后是否补了第二次 `adb push`
4. 是否只启动了一个 `ipc_firmware`
5. `VI2` 是否还是 `NV12 + JPEG + use_current_win=0`
6. `VO` 是否仍是 `layer 4 / 480x800`

#### 情况 B：MP4 也白，RTSP 也白

这时再检查：

1. `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`
2. `vi_module.c` 里是否引入了 AE 强改逻辑
3. sensor/ISP 初始化路径是否变化

---

## 当前建议的基线文档

以后只要碰 LCD/ISP/VI/VO，先看这几份：

1. `AI交接文档.md`
2. `docs/lcd_preview_porting_record_20260627.md`
3. `docs/lcd_recovery_not_from_isp_20260628.md`
4. `docs/backup_restore_guide_20260627.md`

---

## 一句话沉淀

> 这次 LCD 恢复成功，靠的不是“重新找 ISP 文件”，而是确认 ISP 主链路本来就正常，然后把 VI2→VO 的正确基线和板端的严格 reboot/单实例启动流程真正执行到位。
