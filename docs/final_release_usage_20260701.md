# 最终版本使用说明（2026-07-01）

## 1. 这份最终版本包含什么

当前最终版本目标是同时跑通：

- LCD 本地预览
- MP4 录像
- RTSP 音视频推流
- 人脸检测叠框

推荐使用的源码快照目录：

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_20260701_all`

同时必须保留的外部文件：

- `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_Facedet_480_288_nv12_20260701.nb`
- `D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`

## 2. 从源码重新生成程序

说明：

- `ipc_firmware` 可以从源码重新生成
- `.nb` 和 ISP `.bin` 都不能通过 `make` 重新生成

Ubuntu 上执行：

```bash
cd ~/mpp_camera_recorder
make clean
make
```

生成：

```bash
~/mpp_camera_recorder/ipc_firmware
```

## 3. 板子上真正跑起来需要哪些文件

至少需要这三个：

1. `ipc_firmware`
2. `Facedet_480_288_nv12.nb`
3. `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`

其中：

- `ipc_firmware` 在 `/mnt/UDISK/` 运行
- `.nb` 模型文件也放在 `/mnt/UDISK/`
- ISP context 放在 `/mnt/extsd/`

## 4. 标准运行流程

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

## 5. 如何验证是完整成功

至少确认四件事：

1. LCD 有正常预览
2. Windows/VLC 能看到 RTSP
3. 板子能录出 MP4
4. 人脸检测框能出来

额外验证：

- `adb shell "ls -lh /mnt/UDISK/cam*.mp4"`
- `adb shell "ps | grep ipc"`

## 6. 当前版本的关键配置

### 6.1 视频主录像链

- `VI0(dev=0)`
- `1920x1088`
- `YVU420 (NV21)`
- `20fps`
- H.265 CBR 2Mbps

### 6.2 LCD 预览链

- `VI2(dev=4)`
- `480x800`
- 当前源码里 `c_vi2.pix_fmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420`
- `VO layer=4`

### 6.3 NPU 检测链

- `VI(dev=8)`
- `480x288`
- 输入模型：`Facedet_480_288_nv12.nb`

## 7. 回滚策略

如果要万无一失回滚，至少保存四份：

1. `backup_20260701_all/`
2. `backup_ipc_firmware_lcd_recovered_20260701`
3. `backup_Facedet_480_288_nv12_20260701.nb`
4. `backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`

缺一个都不叫完整回滚集。

## 8. 常见误区

- 只备份源码，不备份 `.nb` 和 ISP `.bin`
- 只 `push` 不 `reboot`
- reboot 后不重新 `adb push`
- 重复启动两个固件实例
- 看到 LCD 异常就直接怀疑 ISP context，而不先看 VI2->VO
