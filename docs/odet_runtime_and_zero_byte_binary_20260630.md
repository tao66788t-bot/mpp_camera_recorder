# ODET 运行状态与 0 字节固件故障记录

## 目的

记录 2026-06-30 这次 ODET 运行排查中两个容易混淆的问题：

1. 绿色框已经能出来，但它不是“人脸框”，而是当前模型的“目标检测框”
2. 一次看似“启动命令不生效”的故障，真实原因不是参数错误，而是板子上的 `ipc_firmware` 被部署成了 0 字节

这份记录的作用是：

- 后面面试或复盘时，能说清楚问题分层
- 后续再遇到“程序起不来”时，先检查部署产物，不要先怀疑代码

## 现象 1：LCD / RTSP 上能看到绿色框，但不跟脸

这不是启动失败，而是模型语义问题。

当前 ODET 接入使用的是：

- `network_binary-yolov3-tiny.nb`
- 接入来源：`第三期源码/21-3_视频流目标检测部署/sample_odet_demo/`

这个模型是通用目标检测，不是专门的人脸检测模型。

因此当前框的含义是：

- 人
- 物体
- 其他 sample 支持的目标类别

而不是“人脸关键区域”。

所以如果看到：

- 框大致跟着整个人或物体走
- 但不紧贴脸

这属于**当前模型能力边界**，不是 LCD 叠框映射逻辑已经损坏。

## 现象 2：程序看起来像“已经执行了启动命令”，但实际上没有跑起来

这次最关键的排查结论是：

- Ubuntu 上的 `ipc_firmware` 是正常的
- 板子上的 `/mnt/UDISK/ipc_firmware` 却被部署成了 0 字节

### 实际证据

Ubuntu 侧：

- `ls -l /home/ubuntu/mpp_camera_recorder/ipc_firmware`
- 大小正常：`13501192`

板子侧：

- `adb shell ls -l /mnt/UDISK/ipc_firmware`
- 当时看到大小是：`0`

同时：

- `/tmp/ipc_run.log` 也是空文件
- `ps | grep ipc` 查不到进程

这说明：

- 启动命令本身已经送到了板子
- 但板子执行的目标文件本身就是坏的

所以故障本质是**部署产物损坏**，不是参数问题、也不是 ODET 初始化逻辑本身有问题。

## 修复过程

按最小动作修复，不改代码：

1. 先确认 Ubuntu 编译产物大小正常
2. 重新执行：

```bash
adb push ipc_firmware /mnt/UDISK/ipc_firmware
```

3. 再次检查板子侧大小：

```bash
adb shell ls -l /mnt/UDISK/ipc_firmware
```

确认恢复为正常大小：

- `13501192`

4. 再重新启动：

```bash
cd /mnt/UDISK
killall ipc_firmware 2>/dev/null
chmod +x ipc_firmware
LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib ./ipc_firmware --enable-odet --odet-model /mnt/UDISK/network_binary-yolov3-tiny.nb >/tmp/ipc_run.log 2>&1 &
```

5. 再检查：

```bash
adb shell ps
adb shell ls -l /tmp/ipc_run.log
```

本次确认到：

- `ipc_firmware` 进程恢复驻留
- `/tmp/ipc_run.log` 开始持续增大

## 为什么会误判成“启动命令有问题”

因为从现象上看：

- `killall`
- `chmod`
- `LD_LIBRARY_PATH=... ./ipc_firmware ... &`

这些命令都执行了

但最后没有进程、也没有有效日志。

如果不检查板子上的文件大小，很容易误以为：

- 启动参数写错了
- ODET 模型路径不对
- 代码又崩了

实际上这次不是。

## 后续固定检查项

以后只要出现“启动命令执行了，但程序没驻留”，固定按这个顺序查：

1. `adb devices`
2. `adb shell ls -l /mnt/UDISK/ipc_firmware`
3. `ls -l ~/mpp_camera_recorder/ipc_firmware`
4. 对比 Ubuntu 和板子上的二进制大小
5. `adb shell ls -l /tmp/ipc_run.log`
6. `adb shell ps`

不要先改代码。

## 一句话结论

> 这次 ODET 运行问题最终分成两层：框不贴脸是因为当前模型是通用目标检测；程序起不来则是因为板子上的 `ipc_firmware` 一度被部署成了 0 字节，重新 push 正常二进制后恢复。
