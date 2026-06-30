# 2026-06-30 Face Verified 完整备份与回滚说明

## 1. 这份备份代表什么

这是一份已经在板子上实际验证通过的可工作基线，覆盖以下能力：

- LCD 本地预览正常
- MP4 录制正常
- RTSP 视频传输正常
- RTSP 音频传输正常
- 端侧人脸检测正常

本次恢复 LCD 正常显示，并不是靠重新找回 ISP context 完成的。
当前板子上的 ISP context 文件本身就是正常的：

- `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`
- 文件大小：`34300` 字节

本次最终跑通，核心是：

- 保持 ISP context 正常，不再误删
- 保持 LCD 已验证通过的 VI2 -> VO 路径
- 保持 MP4 / RTSP 的编码与路由链路
- 将端侧检测切换为课程和 SDK 中真实存在的人脸模型

## 2. 已验证的人脸检测基线

本次不是“猜一个模型名”去跑，而是对照了现有资料和 SDK 里的真实证据：

- 课程资料：`第三期源码/20-11_人脸与人形检测模型测试/det_demo/testcase.txt`
- SDK 模型：`/home/ubuntu/tina-v853-100ask/package/allwinner/libawnn_full/models/fdet/Facedet_480_288_nv12.nb`
- SDK 头文件：`awnn.h`、`awnn_det.h`

最终落地参数为：

- 模型：`Facedet_480_288_nv12.nb`
- 输入格式：`NV12`
- 输入尺寸：`480x288`
- 阈值：`0.60`

板子上通过日志已实际看到连续检测结果，说明不是“程序启动了但没真跑”，而是人脸检测链路真的在工作。

## 3. 备份清单

### 3.1 Windows 本地备份

源码快照目录：

- `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260630_face_verified_clean`

本地固件二进制：

- `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_ipc_firmware_face_verified_20260630`

### 3.2 Ubuntu VM 备份

源码快照目录：

- `/home/ubuntu/backup_20260630_face_verified`

Ubuntu 侧固件二进制：

- `/home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_face_verified_20260630`

### 3.3 板子 UDISK 备份

板子侧固件备份：

- `/mnt/UDISK/backup_ipc_firmware_face_verified_20260630`

板子侧人脸模型备份：

- `/mnt/UDISK/backup_Facedet_480_288_nv12_20260630.nb`

## 4. 为什么这份备份重要

这份备份比之前的 `2026-06-28 full verified` 更强，因为它不只是：

- LCD 正常
- MP4 正常
- RTSP 正常

它还额外包含：

- 音频已并入 RTSP
- 真实人脸检测已接入并验证通过

所以后面如果继续做模型叠框、联动拍照、事件上报等功能，应该从这份基线继续开发，而不是回退到更早的仅 LCD / 仅 RTSP 版本。

## 5. 回滚原则

如果后面再次改坏，优先按下面顺序回滚：

1. 先回滚源码到 `backup_20260630_face_verified_clean`
2. 再恢复固件二进制到 `backup_ipc_firmware_face_verified_20260630`
3. 确认板子上的模型文件仍在
4. 严格按部署流程重新 push + reboot + 重新启动

不要一上来就怀疑 ISP context，也不要先乱改 ISP / AE API。

如果出现：

- LCD 偏黄
- LCD 发白
- LCD 黑屏
- RTSP 有声音没画面
- 人脸检测程序启动但没有检测日志

都应该先对照这份基线的源码和部署流程查差异，而不是重新猜调用顺序。

## 6. 标准恢复步骤

### 6.1 如需恢复本地源码

把下面目录中的内容作为恢复基线：

- `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260630_face_verified_clean`

重点文件包括：

- `src/`
- `include/`
- `docs/`
- `conf/`
- `tools/`
- `Makefile`
- `AI交接文档.md`

### 6.2 如需恢复 Ubuntu 固件

在 Ubuntu 上把这份二进制作为已验证版本使用：

- `/home/ubuntu/mpp_camera_recorder/backup_ipc_firmware_face_verified_20260630`

### 6.3 如需恢复板子 UDISK 文件

板子侧至少保证这两个文件存在：

- `/mnt/UDISK/ipc_firmware`
- `/mnt/UDISK/Facedet_480_288_nv12.nb`

必要时可用下面备份覆盖：

- `/mnt/UDISK/backup_ipc_firmware_face_verified_20260630`
- `/mnt/UDISK/backup_Facedet_480_288_nv12_20260630.nb`

## 7. 部署硬规则

后面每次改完代码，仍然必须按这个顺序来：

1. `cd ~/mpp_camera_recorder && make clean && make`
2. `adb push ipc_firmware /mnt/UDISK/ipc_firmware`
3. 如模型有改动，再 push 模型
4. `adb shell reboot`
5. 等 `15` 秒
6. `adb kill-server && adb start-server`
7. 确认 `adb devices` 里有 `20080411 device`
8. reboot 后 UDISK 可能没挂稳，必要时再 `adb push` 一次
9. 只启动一次固件，不要重复启动
10. 用 `ps` 和日志确认程序真的起来了

## 8. 启动命令基线

板子侧启动时，确保模型路径正确：

```sh
cd /mnt/UDISK
chmod +x ipc_firmware
LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb
```

如果要走现有的远端自动部署方式，仍以项目里已经沉淀好的脚本为准，不要手敲一长串 SSH 套 ADB 的命令。

## 9. 后续开发建议

后面新增功能时，建议始终遵守：

- LCD / ISP / VI / VO 问题先看已验证基线
- SSH 操作优先短命令，避免长命令被吞
- 先看现有文档、备份和 SDK 示例，再改代码
- 每做成一个稳定节点，就立刻做源码、二进制、板子三处备份

这样后面即使再改音频、检测框、抓拍、事件联动，也能很快回到这份稳定版本。
