# IPC 固件项目 AI 交接文档

## 硬件环境
- **板子**: Allwinner V853, 100ask 开发板
- **Sensor**: GC2053 MIPI-CSI2, 1920×1088@20fps
- **LCD**: 480×800 竖屏
- **编译机**: Ubuntu VM, IP 192.168.73.135, 用户 ubuntu/ubuntu, SSH 密码 ubuntu
- **板子连接**: USB ADB, 编译机通过 ADB 连板子 (串号 20080411)
- **SDK 路径**: `/home/ubuntu/tina-v853-100ask/`
- **项目路径**: `/home/ubuntu/mpp_camera_recorder/` (Ubuntu) / `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\` (Windows)

## 部署流程（硬性规则）
每次改代码后必须照此执行：
1. 编译: `cd ~/mpp_camera_recorder && make clean && make`
2. 推送: `adb push ipc_firmware /mnt/UDISK/ipc_firmware`
3. 重启板子: `adb shell reboot`
4. 等 15 秒: `sleep 15`
5. 重置 ADB: `adb kill-server && adb start-server && sleep 2`
6. 确认连接: `adb devices` (看到 20080411 device)
7. UDISK 不稳定——reboot 后可能挂不上，需要额外 `adb push` 一次
8. 启动固件: `adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware" &`
9. 关键：只启动一次，不要重复启动（video0 被占会导致 LCD 物理关闭）
10. 验证: `adb shell "ps | grep ipc"` 看到进程

## 当前工作状态

### ✅ 2026-06-28 新增验证通过
- **音频采集 + AAC 编码 + MP4 封装已验证成功**
  - 链路: `AI(dev=0, chn=0) -> AENC(chn=0, AAC) -> MUX -> /mnt/UDISK/cam_1.mp4`
  - 运行日志可见: `audio init ok`、`audio start ok`、`audio_ok=5972`
  - 优雅退出后用 `ffprobe` 验证结果:
    - `stream#0`: `hevc / hvc1 / 1920x1088`
    - `stream#1`: `aac / mp4a / 16000Hz / mono`
    - `duration`: 约 `382s`
- **开机乱响问题已压住**
  - 板子 `/etc/init.d/S03audio` 已替换为静音脚本版本，reboot 时优先关闭 `SPK/LINEOUT`
  - 脚本来源: `conf/S03audio_boot_mute.sh`
- **LCD + MP4(A/V) + RTSP(A/V) 四路并行已再次验证**
  - 当前主线已恢复 `audio_module` 接入、`AENC_GetStream -> MUX_SendAudioStream + rtsp_push_aac` 路由
  - Ubuntu 编译机 `make clean && make` 编译通过
  - 板端按标准流程完成 `push -> reboot -> 等15s -> adb重连 -> 再push -> 单实例启动`
  - RTSP 协议层验证结果：
    - `DESCRIBE` 返回 SDP 中包含 `track1`
    - SDP 中包含 `MPEG4-GENERIC`
    - `PLAY` 后 3 秒内收到 interleaved RTP：
      - 视频通道 `0`: `549` 包
      - 音频通道 `2`: `46` 包
  - 详细记录见: `docs/rtsp_audio_validation_20260628.md`
- **LCD 恢复结论已更正**
  - 当前板子上的 `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin` 已确认存在，大小为 **34300 字节**
  - 这次 LCD 恢复**不是**靠“重新找回 ISP context 文件”完成的
  - 当前现象更符合：**MP4/RTSP 主链路正常，LCD 异常由 VI2→VO 预览链路或板端运行态引起**
  - 详细记录见: `docs/lcd_recovery_not_from_isp_20260628.md`

### ✅ 正常工作的
- LCD 全屏预览: VI2(dev=4) 480×800 NV12 → VO layer 4, 全屏正常
- MP4 录制: VI0(dev=0) 1920×1088 NV21 + AI(dev=0) → AENC(AAC) → MUX → /mnt/UDISK/cam_*.mp4
- RTSP 推流: VI0(dev=0) → VENC(H.265) + AI(dev=0) → AENC(AAC) → RTSP(TCP:554)
- 硬件零拷贝: VI→VENC SYS_Bind, VENC→MUX SYS_Bind, VI2→VO SYS_Bind

### ⚠️ 历史故障（不要再默认当成当前结论）
- 曾经发生过 **ISP context 文件被误删** 的事故，路径为 `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`
- 该事故会导致 ISP 使用默认参数，可能出现全局过曝、全白等现象
- 但**按 2026-06-28 当前现场**，这个文件已经存在且大小正常，因此：
  - 如果 **MP4/RTSP 画面正常、只有 LCD 异常**，不要先把锅甩给 ISP context
  - 这种情况应先检查 `VI2(dev=4) -> VO` 链路配置、部署流程、reboot 是否完整、是否重复启动实例
- 以前尝试过的 AE 强行覆盖（`AE_SetMode/SetExposure/SetGain`）已证明风险高，不能当作 LCD 恢复常规手段

### 🔧 已做但未完成的
- RTSP 音频已打通并验证 A/V 双流
- 后续若继续做，可往这些方向展开：
  - VLC/ffplay 侧长期稳定性验证
  - AO 播放/对讲
  - 局域网 Wi-Fi 直连观看（当前最稳路径仍是 adb forward + SSH tunnel）

## ISP 问题的解决思路

### 当前 vi_module.c 状态
已回滚到备份版本: 纯 `AW_MPI_ISP_Run()` + `AW_MPI_ISP_SetSaveCTX()` (退出时保存 context)
- 无 AE 覆盖代码
- 备份源码: `backup_20260627_working/` 目录
- 备份二进制: `backup_ipc_firmware_working_20260627`

### 修复 ISP context 的方向
1. **首选**: 从同学/老师的正常板子上 `adb pull /mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`，放到板子上重启即可
2. **次选**: 让固件长时间运行(>5分钟)，AE 自动校准后 `killall ipc_firmware`(SIGTERM) 触发 SetSaveCTX 保存 context
3. **不推荐**: 手动调用 AE API（已证明很难调对，AE_SetMode: 0=auto, 1=manual）

### ISP API 速查（来自 mpi_isp.h）
```c
AW_MPI_ISP_AE_SetMode(dev, 0);          // 0=auto, 1=manual
AW_MPI_ISP_AE_SetExposureBias(dev, 1);  // [1,8], lower=darker
AW_MPI_ISP_AE_SetExposure(dev, 10000);  // [0, 65535*16]
AW_MPI_ISP_AE_SetGain(dev, 256);        // 256=1x gain
AW_MPI_ISP_SetSaveCTX(dev);             // 保存 context 到 /mnt/extsd/
```

## RTSP 继续开发的方向
参考 `/home/ubuntu/tina-v853-100ask/external/eyesee-mpp/middleware/sun8iw21/sample/` 中的 USB camera 示例 (sample_uvcout):
- VENC 需要 `VeAttr.mOnlineEnable = 1` 才能从 GetStream 出帧
- 已去掉 VENC→MUX SYS_Bind，改为 router 线程手动分发给 MUX + RTSP
- 学长的文档确认了这种架构可行性（多线程生产者-消费者）
- 当前主线代码已经是 `VENC_GetStream -> MUX_SendVideoStream + RTSP push`、`AENC_GetStream -> MUX_SendAudioStream + RTSP push` 架构
- 当前已完成 **MP4 音频封装闭环验证 + RTSP A/V 会话级联调验证**

## Windows 端 SSH 连接编译机
```bash
/tmp/sshpass.exe -p "ubuntu" ssh -o StrictHostKeyChecking=accept-new ubuntu@192.168.73.135 "命令"
/tmp/sshpass.exe -p "ubuntu" scp 本地文件 ubuntu@192.168.73.135:远程路径
```

## 关键教训
1. 永远不要删 /mnt/extsd/ 下的 ISP context 文件
2. VI2 必须用 YUV_SEMIPLANAR_420 (NV12)，VI0 用 YVU_SEMIPLANAR_420 (NV21)
3. 改代码后必须 reboot，否则 ISP/VE 硬件状态残留
4. 不要同时启动两个固件实例——会导致 LCD 物理关闭
5. UDISK 挂载不稳定，reboot 后可能需要重新 push
6. **如果 MP4/RTSP 正常但 LCD 不正常，优先怀疑 VI2→VO 或板端运行态，不要先怀疑 ISP context**
