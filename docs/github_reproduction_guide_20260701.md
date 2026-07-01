# GitHub 克隆后复现说明（2026-07-01）

## 1. 先说结论

这个仓库现在已经包含：

- 最终源码
- 编译脚本
- 运行脚本
- 分析工具
- 适配与验证文档

但是 **仅仅 `git clone` 还不能 100% 直接跑起来**，因为板端运行还依赖两个不在 Git 仓库里自动生成的外部文件：

1. `Facedet_480_288_nv12.nb`
2. `isp0_1920_1088_20_0_ctx_saved.bin`

## 2. clone 下来之后你手里会有什么

你会拿到：

- `src/` 和 `include/` 的完整源码
- `Makefile`
- `docs/` 里的适配/验证/面试沉淀文档
- `tools/` 里的分析与测试工具
- `assets/` 里的测试输入素材

你不会自动拿到：

- 板端 ISP context 文件
- NPU 模型包 `.nb`
- 板子当前已经运行好的 `/mnt/UDISK/ipc_firmware`

## 3. 还缺的两个关键文件是什么

### 3.1 `Facedet_480_288_nv12.nb`

作用：

- 人脸检测 NPU 模型

没有它会怎样：

- 程序主体仍可编译
- 但 `--enable-odet` 这条人脸检测链跑不起来

### 3.2 `isp0_1920_1088_20_0_ctx_saved.bin`

作用：

- 板端 ISP runtime context

没有它会怎样：

- 不一定完全不能跑
- 但图像效果、曝光、颜色和 LCD/预览稳定性可能跟最终验证状态不一致

## 4. 复现到“最终验证版本”需要准备什么

### 4.1 Ubuntu 侧

需要：

- Tina SDK
- 可用的 `make`
- `adb`
- 项目源码目录

### 4.2 板端

需要：

- V853 + GC2053 + 480x800 LCD
- `/mnt/UDISK`
- `/mnt/extsd`

### 4.3 外部文件

至少准备：

- `Facedet_480_288_nv12.nb`
- `isp0_1920_1088_20_0_ctx_saved.bin`

推荐从你自己的本地备份中拿：

- `backup_Facedet_480_288_nv12_20260701.nb`
- `backup_isp0_1920_1088_20_0_ctx_saved_20260701.bin`

## 5. 外部文件应该放到哪里

### 5.1 Ubuntu 编译目录

建议把模型文件放在：

```bash
~/mpp_camera_recorder/Facedet_480_288_nv12.nb
```

### 5.2 板端运行目录

运行时需要：

```bash
/mnt/UDISK/ipc_firmware
/mnt/UDISK/Facedet_480_288_nv12.nb
/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin
```

## 6. clone 后的推荐复现步骤

### 6.1 获取源码

```bash
git clone <your-repo-url>
cd mpp_camera_recorder
```

### 6.2 补齐外部文件

把下面两个文件复制进来：

```bash
Facedet_480_288_nv12.nb
isp0_1920_1088_20_0_ctx_saved.bin
```

其中 ISP context 最终要放到板端 `/mnt/extsd/`。

### 6.3 编译

```bash
cd ~/mpp_camera_recorder
make clean
make
```

### 6.4 部署

```bash
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb
adb push isp0_1920_1088_20_0_ctx_saved.bin /mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin
adb shell reboot
sleep 15
adb kill-server && adb start-server && sleep 2
adb devices
adb push ipc_firmware /mnt/UDISK/ipc_firmware
adb push Facedet_480_288_nv12.nb /mnt/UDISK/Facedet_480_288_nv12.nb
adb shell "cd /mnt/UDISK && chmod +x ipc_firmware && LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb" &
adb shell "ps | grep ipc"
```

## 7. 如果没有 `.nb` 或 `.bin` 怎么办

### 7.1 没有 `.nb`

你可以：

- 先不启用 `--enable-odet`
- 先验证 LCD / MP4 / RTSP 主链

### 7.2 没有 ISP `.bin`

你可以：

- 先跑通程序主链
- 但不要承诺图像效果和最终验证版完全一致

## 8. 最适合公开仓库的表述

如果你要告诉别人怎么复现，最稳的说法是：

“仓库提供完整源码、脚本和文档；其中 `ipc_firmware` 可由源码直接编译生成，但 NPU 模型文件和板端 ISP context 属于外部运行依赖，需要另行准备后才能完全复现最终验证效果。”

## 9. 一句话总结

GitHub 仓库现在足够让别人理解、编译和继续开发这个项目；但如果想 1:1 跑出你当前这块板子上的最终效果，仍然要额外带上 `.nb` 模型文件和 ISP context `.bin`。 
