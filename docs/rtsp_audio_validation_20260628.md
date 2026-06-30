# RTSP 音频并行验证记录（2026-06-28）

## 目标

确认当前固件已经不是：

- LCD 正常
- MP4 有音频
- RTSP 只有视频

而是已经真正实现：

- `VI2 -> VO -> LCD`
- `VI0 -> VENC -> MUX -> MP4`
- `AI -> AENC -> MUX -> MP4`
- `VI0 -> VENC -> RTSP`
- `AI -> AENC -> RTSP`

即 **LCD + MP4(A/V) + RTSP(A/V) 同时工作**。

---

## 先看的证据

1. 项目文档
   - `AI交接文档.md`
   - `docs/audio_capture_validation_20260628.md`
   - `docs/rtsp_windows_view_20260628.md`
2. 当前主线源码
   - `src/main.c`
   - `src/audio_module.c`
   - `src/rtsp_module.c`
   - `include/rtsp_module.h`
   - `include/mux_module.h`
3. 已验证基线
   - `backup_20260628_audio_verified/`

---

## 对照后发现的缺口

当前主线在本轮修复前，音频模块本身还在，但主线入口被摘掉了：

- `main.c`
  - `#include "audio_module.h"` 被移除
  - `audio_init/audio_start/audio_stop/audio_deinit` 没有调用
  - `c_rt.aenc_chn = -1`
  - `rtsp_set_router_refs(..., -1, ...)`
- `rtsp_module.c`
  - SDP 只发布视频 `track0`
  - router 线程不再拉 `AW_MPI_AENC_GetStream`
  - 不再向 `AW_MPI_MUX_SendAudioStream` 和 `rtsp_push_aac()` 分发音频

也就是说，问题不是“音频模块没写”，而是**当前主线把已经做好的音频 RTSP 通路断开了**。

---

## 本次恢复的最小改动

### 1. 恢复 main 主线的音频接入

恢复内容：

- 加回 `audio_module.h`
- 加回 `audio_handle_t *au` 和 `audio_config_t c_au`
- 初始化：
  - `AI(dev=0, chn=0)`
  - `AENC(chn=0, AAC)`
  - `16000Hz / mono / 16bit / 1024 samples`
- 启动顺序中加回：
  - `audio_init()`
  - `audio_start()`
  - `audio_stop()`
  - `audio_deinit()`
- `c_mx` 恢复音频参数
- `c_rt` 恢复：
  - `aenc_chn`
  - `audio_enabled`
  - `audio_sample_rate`
  - `audio_channels`
- `rtsp_set_router_refs(g.ve->chn, g.au->aenc_chn, g.mx->grp, g.rt)`

### 2. 恢复 RTSP 的 AAC SDP 与音频分发

恢复内容：

- `DESCRIBE` 中为 SDP 增加：
  - `m=audio`
  - `a=rtpmap: MPEG4-GENERIC`
  - `a=fmtp`
  - `a=control:track1`
- `stream_router_thread` 恢复：
  - `AW_MPI_AENC_GetStream`
  - `AW_MPI_MUX_SendAudioStream(g_mx, &as, 1, -1)`
  - `rtsp_push_aac()`
- 保留视频仍走：
  - `VENC_GetStream -> MUX_SendVideoStream + rtsp_push_h265`

### 3. 将 AAC 输出恢复为 raw AU

`audio_module.c` 中：

- `attachAACHeader` 改回 `0`

原因：

- MUX 写 MP4 不需要每帧带 ADTS
- RTP/AAC 的 `MPEG4-GENERIC` 也希望拿到 raw AU，而不是 ADTS 包一层再包一层

---

## 编译验证

Ubuntu 编译机执行：

```bash
cd ~/mpp_camera_recorder
make clean && make
```

结果：

- `BUILD ipc_firmware OK`

说明当前代码已经能在交叉编译环境下闭合通过。

---

## 上板验证流程

严格按项目硬规则执行：

```bash
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell reboot
sleep 15
adb kill-server && adb start-server
sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &
adb shell "ps | grep ipc"
```

板端确认：

- 设备在线：`20080411 device`
- 固件单实例启动成功

---

## 运行时证据

启动日志中出现：

- `audio init ok`
- `audio start ok`
- `rtsp start ok`
- `ALL:1080p MP4(A/V)+RTSP(TCP:554 A/V)+VO(480x800) LCD`
- `[ROUTER] start ve=0 ae=0 mx=0 audio=1`

运行中持续出现：

- `ok=...` 视频帧统计
- `audio_ok=...` 音频帧统计

这说明 router 线程不仅在取视频流，也在稳定取 AENC 音频流。

---

## 协议层验证

因为 Ubuntu 虚拟机上没有 `ffprobe`，本轮直接使用 Python socket 发起 RTSP 会话验证。

验证步骤：

1. `adb forward tcp:8554 tcp:554`
2. 向 `rtsp://127.0.0.1:8554/stream` 依次发送：
   - `OPTIONS`
   - `DESCRIBE`
   - `SETUP track0`
   - `SETUP track1`
   - `PLAY`
3. 读取 TCP interleaved RTP 包，统计 `$0/$2` 通道数量

得到结果：

- `OPTIONS_OK True`
- `DESCRIBE_OK True`
- `HAS_TRACK1 True`
- `HAS_MPEG4_GENERIC True`
- `SETUP0_OK True`
- `SETUP1_OK True`
- `PLAY_OK True`
- `INTERLEAVED_COUNTS {0: 549, 1: 0, 2: 46, 3: 0}`
- `AUDIO_RTP_SEEN True`
- `VIDEO_RTP_SEEN True`

这组证据说明：

- SDP 已经显式发布音频轨 `track1`
- RTSP 会话级协商已经允许音频 `SETUP`
- `PLAY` 后实际收到了音频 RTP 包（channel `2`）
- 不是“只有视频通了、音频只是配置项写上了”

---

## 结论

当前版本已经实现并验证：

- LCD 实时预览正常
- MP4 录制包含视频 + AAC 音频
- RTSP 会话同时发布视频 + AAC 音频
- 板端单实例启动正常

所以现在可以明确说：

> **`VI -> LCD -> MP4(A/V) -> RTSP(A/V)` 这条主线已经跑通。**

---

## 后续建议

1. Windows 侧再做一次 VLC 长时观看确认，重点看：
   - 是否稳定持续出声
   - 是否存在音画不同步
2. 当前最稳观看路径仍建议：
   - `adb forward tcp:8554 tcp:554`
   - Windows SSH tunnel
   - `VLC --rtsp-tcp rtsp://127.0.0.1:8554/stream`
3. 如果下一步要做对讲/AO，优先沿用本轮恢复后的音频主线，不要再拆掉 `audio_module` 入口
