# 项目总说明

## 1. 项目目标

这是一个基于 Allwinner V853 / Tina / Eyesee-MPP 的 IPC 固件项目。

当前保留的最终版本需要同时完成：

- `VI2 -> VO -> LCD` 本地预览
- `VI0 + AI -> VENC/AENC -> MUX -> MP4`
- `VENC/AENC -> RTSP(:554)`
- `VI(dev=8) -> NPU -> 人脸检测叠框`

## 2. 硬件环境

- 板子：Allwinner V853，100ask 开发板
- Sensor：GC2053，MIPI-CSI2，`1920x1088@20fps`
- LCD：`480x800`
- Ubuntu SDK：`/home/ubuntu/tina-v853-100ask/`
- Ubuntu 项目路径：`/home/ubuntu/mpp_camera_recorder/`

## 3. 代码结构

### 3.1 入口

- `src/main.c`

负责组装整条主链：

- VI0 录像链
- VI2 LCD 预览链
- 音频采集链
- RTSP 推流链
- NPU 检测链

### 3.2 主要模块

- `src/vi_module.c`
  VI/ISP 初始化与采集
- `src/vo_module.c`
  LCD 显示
- `src/venc_module.c`
  H.265 编码
- `src/audio_module.c`
  AI/AENC 音频
- `src/mux_module.c`
  MP4 封装
- `src/rtsp_module.c`
  RTSP server 与 RTP 打包
- `src/npu_module.c`
  人脸检测与叠框
- `src/diag_vi_dump.c`
  抓 VI 原始帧用于诊断

## 4. 两个不能通过 make 生成的文件

### 4.1 `Facedet_480_288_nv12.nb`

作用：

- NPU 人脸检测模型

特点：

- 不是源码编译产物
- 程序启用 `--enable-odet` 时必须存在

### 4.2 `isp0_1920_1088_20_0_ctx_saved.bin`

作用：

- 板端 ISP context

特点：

- 不是源码编译产物
- 会影响最终图像效果与 ISP 状态一致性

## 5. 编译

Ubuntu VM 上执行：

```bash
cd ~/mpp_camera_recorder
make clean
make
```

生成：

```bash
~/mpp_camera_recorder/ipc_firmware
```

## 6. 部署与运行

推荐直接使用：

```bash
sh deploy_final_remote.sh
```

这个脚本会完成：

1. push `ipc_firmware`
2. push `Facedet_480_288_nv12.nb`
3. push `isp0_1920_1088_20_0_ctx_saved.bin`
4. reboot 板子
5. 等待 15 秒
6. 重连 ADB
7. 二次 push
8. 启动固件

## 7. 状态检查

查看当前 Ubuntu / 板端状态：

```bash
sh check_runtime_remote.sh
```

会检查：

- `adb devices`
- Ubuntu 侧 `ipc_firmware`
- 板端 `/mnt/UDISK/ipc_firmware`
- 板端 `/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin`
- 板端 `ipc_firmware` 进程

## 8. 运行时关键路径

板端运行时涉及：

```bash
/mnt/UDISK/ipc_firmware
/mnt/UDISK/Facedet_480_288_nv12.nb
/mnt/extsd/isp0_1920_1088_20_0_ctx_saved.bin
```

## 9. 关键注意事项

- 改完代码后要完整 `make + push + reboot + 等待 + adb重连 + 二次push + 启动`
- `UDISK` reboot 后可能没挂稳，所以二次 push 很重要
- 固件不要重复启动两次
- 如果 MP4/RTSP 正常但 LCD 不正常，优先怀疑 `VI2 -> VO` 链，不要先怀疑 ISP 全局坏掉

## 10. 最终保留原则

这个仓库不再保留大量按日期拆开的阶段性文档、历史脚本、分析工具和测试资产。

当前保留的是：

- 最终版源码
- 一份总说明
- 一个部署脚本
- 一个状态检查脚本
- 两个运行必须的外部文件
