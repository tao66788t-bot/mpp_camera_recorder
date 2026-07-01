# mpp_camera_recorder

基于 Allwinner V853 / Tina / Eyesee-MPP 的 IPC 固件项目。

当前最终版本已经验证过以下链路可同时工作：

- LCD 本地预览：`VI2 -> VO`，`480x800`
- MP4 录像：`VI0 + AI -> VENC/AENC -> MUX -> /mnt/UDISK/cam_*.mp4`
- RTSP 推流：`VENC/AENC -> RTSP server(:554)`
- 人脸检测叠框：`VI(dev=8, 480x288) -> NPU -> preview/encoder region`

## 1. 项目目录

- `src/`
  固件源码实现
- `include/`
  头文件与公共结构体
- `docs/`
  适配记录、恢复记录、验证记录、面试沉淀文档
- `backup_20260701_all/`
  当前最终版本的源码快照与辅助脚本
- `backup_ipc_firmware_lcd_recovered_20260701`
  已编译好的固件二进制备份
- `backup_Facedet_480_288_nv12_20260701.nb`
  人脸检测模型文件备份
- `backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`
  板端 ISP context 文件备份

## 2. 关键文件分别是什么

- `ipc_firmware`
  主程序二进制。这个文件可以从源码重新 `make` 生成。
- `Facedet_480_288_nv12.nb`
  NPU 模型文件。不是源码编译产物，需要单独备份。
- `isp0_1920_1088_20_0_ctx_saved.bin`
  ISP 运行时保存出的参数快照，不是源码编译产物，需要单独备份。

## 3. 编译环境

- 板子：Allwinner V853，100ask 开发板
- Sensor：GC2053，MIPI-CSI2，`1920x1088@20fps`
- LCD：`480x800`
- Ubuntu SDK：`/home/ubuntu/tina-v853-100ask/`
- Ubuntu 项目路径：`/home/ubuntu/mpp_camera_recorder/`

## 4. 编译

在 Ubuntu VM 上执行：

```bash
cd ~/mpp_camera_recorder
make clean
make
```

生成产物：

```bash
~/mpp_camera_recorder/ipc_firmware
```

## 5. 部署和运行

这是这个项目当前确认有效的标准流程，不能省步骤：

```bash
cd ~/mpp_camera_recorder
make clean
make

adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb" &
adb shell "ps | grep ipc"
```

注意：

- reboot 后必须等待，再重连 ADB
- `UDISK` reboot 后可能没挂稳，所以通常要二次 `adb push`
- 固件只能启动一次，不能重复启动

## 6. 如何快速回滚到已验证版本

如果你只想恢复可工作状态，不想重新分析：

1. 用源码快照恢复：
   `backup_20260701_all/src`、`backup_20260701_all/include`、`backup_20260701_all/Makefile`
2. 重新 `make`
3. 把下面两个非编译产物一起带上：
   - `backup_Facedet_480_288_nv12_20260701.nb`
   - `backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`

如果你只想恢复板端运行，不想重新编译：

- 直接部署 `backup_ipc_firmware_lcd_recovered_20260701`

## 7. 当前代码模块做什么

- `src/main.c`
  系统入口，负责组装整条视频、音频、RTSP、NPU、VO 链路
- `src/vi_module.c`
  VI/ISP 初始化与采集通道创建
- `src/vo_module.c`
  LCD 显示通道与图层配置
- `src/venc_module.c`
  H.265 编码器初始化、取流
- `src/audio_module.c`
  AI/AENC 音频采集与 AAC 编码
- `src/mux_module.c`
  MP4 封装
- `src/rtsp_module.c`
  RTSP server、RTP 打包与发送
- `src/npu_module.c`
  NPU 人脸检测主逻辑与叠框
- `src/diag_vi_dump.c`
  抓 VI 原始帧做诊断

更详细的模块说明见：

- [代码结构说明](D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\docs\code_architecture_20260701.md)
- [最终版本使用说明](D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\docs\final_release_usage_20260701.md)
- [GitHub 克隆后复现说明](D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\docs\github_reproduction_guide_20260701.md)

## 8. 重要结论

- 这个项目里，`ipc_firmware` 能重新编译出来
- `.nb` 模型文件和 ISP context `.bin` 不能靠 `make` 生成
- 真正可复现，必须同时保存：
  - 源码快照
  - 可执行备份
  - 模型文件
  - ISP context 文件
