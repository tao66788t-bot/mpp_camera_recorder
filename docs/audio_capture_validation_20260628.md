# 音频采集编码验证记录（2026-06-28）

## 目标

- 确认当前固件不只是“代码里接了音频”，而是真正完成了：
  - `AI -> AENC -> MUX`
  - 录制出的 `MP4` 内确实带有 `AAC` 音轨

## 先看的证据

1. 根目录文档：
   - `音频模块完整学习手册.md`
   - `沐一学长的项目梳理.md`
   - `mpp_camera_recorder/AI交接文档.md`
2. 当前实现：
   - `src/audio_module.c`
   - `src/main.c`
   - `src/mux_module.c`
   - `src/rtsp_module.c`
3. SDK 样例：
   - `sample_ai2aenc2muxer/sample_ai2aenc2muxer.c`

## 对照后确认的关键点

- 当前工程已经不是“未接音频”状态，而是主链路已接好：
  - `audio_module.c`
    - `AW_MPI_AI_SetPubAttr`
    - `AW_MPI_AI_Enable`
    - `AW_MPI_AI_CreateChn`
    - `AW_MPI_AENC_CreateChn`
    - `AW_MPI_SYS_Bind(AI -> AENC)`
  - `main.c`
    - `c_mx.audio_enabled = true`
    - `c_rt.audio_enabled = true`
    - 运行时执行 `audio_start()`
  - `rtsp_module.c`
    - router 线程中已有 `AW_MPI_AENC_GetStream`
    - 音频帧会被送往 `AW_MPI_MUX_SendAudioStream`
- SDK 样例 `sample_ai2aenc2muxer.c` 里 `attachAACHeader = 0`
  - 当前工程与样例保持一致，这说明给 `MUX` 的 AAC 裸流模式是合理的

## 实际验证流程

严格按项目交接文档里的硬规则执行：

1. Ubuntu 编译机重新编译
   - `cd ~/mpp_camera_recorder && make clean && make`
2. 推送新固件到板子
   - `adb push ipc_firmware /mnt/UDISK/ipc_firmware`
3. reboot 板子
   - `adb shell reboot`
4. 等待并重置 ADB
   - `adb kill-server && adb start-server`
   - 确认 `20080411 device`
5. 因 UDISK 挂载不稳，再补一次 push
6. 只启动一次固件
   - 通过板端已有脚本 `/mnt/UDISK/run_ipc_once.sh` 前台拉起，避免重复实例
7. 观察运行日志
8. 发送 `SIGTERM` 优雅退出，让 `MUX` 正常写尾、让 `ISP` 正常保存 context

## 运行时证据

启动日志中出现：

- `audio init ok`
- `audio start ok`
- `mux start ok`
- `ALL:1080p MP4(A/V)+RTSP(TCP:554 A/V)+VO(480x800) LCD`

路由线程持续输出：

- `audio_ok=100`
- `audio_ok=200`
- ...
- 最终退出前为 `audio_ok=5972`

退出日志中出现：

- `[ROUTER] exit, video_ok=7575 audio_ok=5972 fail=4945`
- `EXIT:0`

这说明：

- AENC 的音频码流在稳定产出
- 音频帧数量不是 0
- 固件不是异常崩溃退出，而是正常走完了收尾路径

## 结果文件验证

将板子上的 `/mnt/UDISK/cam_1.mp4` 拉回本机后，用 `ffprobe` 检查，结果如下：

- `stream #0`
  - `codec_name=hevc`
  - `codec_tag_string=hvc1`
  - `width=1920`
  - `height=1088`
  - `nb_frames=7575`
- `stream #1`
  - `codec_name=aac`
  - `codec_tag_string=mp4a`
  - `sample_rate=16000`
  - `channels=1`
  - `nb_frames=5972`
- `format`
  - `nb_streams=2`
  - `duration=382.174000`

## 结论

当前版本已经完成并验证：

- 视频采集 + H.265 编码 + MP4 封装
- 音频采集 + AAC 编码 + MP4 封装
- 单文件双流封装结果正确，`MP4` 内同时存在 `HEVC` 视频轨和 `AAC` 音频轨

所以“现在音频采集成功了吗”的答案是：

- **成功了，而且不是只停留在日志层面，MP4 文件结构已经验证通过。**

## 当前可回滚版本

- 源码快照目录：
  - `backup_20260628_audio_verified/`
- 二进制备份：
  - `backup_ipc_firmware_audio_verified_20260628`

## 回滚方法

如果后面继续开发 RTSP 音频、AO 播放、对讲功能把工程改坏了，可以直接回滚：

1. 用 `backup_20260628_audio_verified/` 覆盖当前工程源码
2. 或直接使用 `backup_ipc_firmware_audio_verified_20260628`
3. 然后重新执行标准部署流程：
   - `make clean && make`
   - `adb push`
   - `adb shell reboot`
   - `adb kill-server && adb start-server`
   - 仅启动一次固件
