# VENC QP/CV 优化记录（2026-06-28）

## 目标

在不破坏当前这条已验证主线的前提下，继续优化 H.265 码流稳定性：

- `VI2 -> VO -> LCD`
- `VI0 -> VENC -> MUX -> MP4`
- `VI0 -> VENC -> RTSP`
- `AI -> AENC -> MUX/RTSP`

关注指标是 MP4 视频帧大小的离散度，使用：

- `analyze_mp4_cv.py`
- 指标：`cv_percent = 帧大小标准差 / 平均值 * 100%`

---

## 先看的证据

1. 项目文档
   - `AI交接文档.md`
   - `docs/lcd_recovery_not_from_isp_20260628.md`
   - `docs/audio_capture_validation_20260628.md`
   - `docs/rtsp_audio_validation_20260628.md`
2. 当前主线代码
   - `src/main.c`
   - `src/venc_module.c`
   - `include/venc_module.h`
3. 项目内已有配置
   - `conf/camera_recorder.conf`
4. SDK sample / 头文件
   - `sample_venc/sample_venc.c`
   - `sample_recorder/sample_recorder.c`
   - `sample_multi_vi2venc2muxer/sample_multi_vi2venc2muxer.c`
   - `media/mm_comm_rc.h`
   - `media/mm_comm_venc.h`

---

## 发现的关键问题

### 1. 当前主线一度退回成了“QP 字段为 0”

运行日志读回的实际参数里能看到：

```text
[VENC-DIAG] H.265 CBR: bitrate=2000000, MaxQp=0, MinQp=0, MaxPqp=0, MinPqp=0, QpInit=0
```

这说明主线并没有稳定保留之前做过的 QP 实验值。

### 2. 只设置了 `GopAttr.mGopSize`，没有把 RC 侧的帧率/GOP信息补齐

对照 SDK sample 发现：

- 官方 `sample_venc` 在 H.265 CBR 下会显式设置：
  - `mAttrH265Cbr.mSrcFrmRate`
  - `mAttrH265Cbr.fr32DstFrmRate`
- 当前工程之前只给了：
  - `GopAttr.mGopSize = 30`

实际录出来的 I 帧数量也印证了这个异常：

- 调整前 `qp_graceful.mp4`
  - `481~483` 帧附近
  - I 帧数量明显偏多
- 说明“日志里 GOP=30”并不等于 RC 层真的按这套节奏在跑

### 3. VBV 之前一直走 0 值路径

调整前日志：

```text
[VENC-DIAG] H.265 VBV BufSize=0, ThreshSize=0
```

而 SDK sample 里对 H.265 常见写法是显式给：

- `mThreshSize = AWALIGN((w*h*3/2)/3, 1024)`
- `mBufSize = AWALIGN(bitrate*4/8 + thresh, 1024)`

---

## 本次最小改动

### 1. `src/main.c`

显式补回 H.265 CBR 参数：

- `init_qp = 30`
- `min_i_qp = 25`
- `max_i_qp = 35`
- `min_p_qp = 22`
- `max_p_qp = 38`
- `key_frame_interval = 60`
- `vbv_thresh_size = ((1920 * 1088 * 3 / 2) / 3)`
- `vbv_buf_size = ((bitrate * 4 / 8) + vbv_thresh_size + 1023) & ~1023`

### 2. `src/venc_module.c`

按 SDK sample 的方式，把 RC 属性里的公共字段补齐：

- H.264/H.265 `CBR/VBR/ABR/FIXQP`
  - `mGop`
  - `mStatTime`
  - `mSrcFrmRate`
  - `fr32DstFrmRate`

也就是说，这次不是只“收 QP 范围”，而是把：

- QP 范围
- key frame interval
- VBV
- RC 帧率上下文

一起恢复到一个更完整、更接近 SDK sample 的状态。

---

## 部署与验证流程

严格按项目硬规则执行：

1. `cd ~/mpp_camera_recorder && make clean && make`
2. `adb push ipc_firmware /mnt/UDISK/ipc_firmware`
3. `adb shell reboot`
4. `sleep 15`
5. `adb kill-server && adb start-server && sleep 2`
6. `adb devices` 确认 `20080411 device`
7. 再补一次 `adb push`
8. 只启动一个固件实例
9. 板端运行约 `25s`
10. `SIGTERM` 优雅退出
11. 拉回 `/mnt/UDISK/cam_1.mp4`
12. 用 `analyze_mp4_cv.py` 统计

---

## 结果对比

### 调整前（最近一次退回 0 值参数的版本）

文件：`qp_graceful.mp4`

- `frames = 483`
- `i_frames = 25`
- `p_frames = 458`
- `avg_bitrate_kbps = 2005.38`
- `cv_percent = 143.70`

### 调整后（本次版本）

文件：`qp_stage3.mp4`

- `frames = 481`
- `i_frames = 9`
- `p_frames = 472`
- `avg_bitrate_kbps = 2010.03`
- `cv_percent = 92.70`

### 直接结论

在维持约 `2Mbps` 目标码率的前提下：

- `CV: 143.70% -> 92.70%`
- 下降了约 `51` 个百分点
- I 帧数量从 `25` 降到 `9`
- P 帧最大体积也明显收敛

这说明当前优化不只是“码率看起来差不多”，而是帧大小波动已经实打实压下来了。

---

## 读回日志证据

本次板端运行日志中的 `VENC-DIAG`：

```text
[VENC-DIAG] rc_mode=14, bitrate=2000 kbps, gop=30, framerate=20/20
[VENC-DIAG] H.265 VBV BufSize=2044928, ThreshSize=1044480, FastEnc=1
[VENC-DIAG] H.265 CBR: bitrate=2000000, MaxQp=35, MinQp=25, MaxPqp=38, MinPqp=22, QpInit=30
```

这说明这次不是“以为传进去了”，而是 SDK 读回值已经和目标参数一致。

---

## 当前结论

本轮有效的，不是单独某一个 QP 数字，而是下面这组组合：

1. 明确给 H.265 CBR 的 QP 范围
2. 明确给 `key_frame_interval`
3. 明确给 VBV `BufSize/ThreshSize`
4. 明确给 RC 层 `mSrcFrmRate/fr32DstFrmRate`

这几项一起补齐后，码流波动明显下降。

---

## 可面试表述

可以概括成：

> 一开始我发现项目里虽然封装了 H.265 的 QP 配置结构，但主线并没有把这些参数稳定传到 SDK，VBV 也在走 0 值路径。对照全志 MPP sample 后，我补齐了 H.265 CBR 的 QP 范围、key frame interval、VBV 缓冲以及 RC 侧帧率参数，并通过板端录制 + 离线脚本统计 MP4 帧大小 CV 做闭环验证，最终把码流波动从 143.7% 压到了 92.7%。

