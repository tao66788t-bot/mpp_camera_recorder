# V853 端侧目标检测接入记录（进行中）

## 证据来源

1. `第三期源码/21-3_视频流目标检测部署/sample_odet_demo/readme.txt`
2. `第三期源码/21-3_视频流目标检测部署/sample_odet_demo/sample_odet_demo.c`
3. SDK 同目录下 `yolov3-tiny_nbg_viplite/` 与 `post/` 源码
4. 当前工程已经验证通过的 `VI0 -> VENC -> MP4/RTSP` 与 `VI2 -> VO`

## 接入思路

不是重写 sample 主程序，而是给当前 `ipc_firmware` 增量接入第三路 VI：

- `VI0(dev=0)`：1920x1088，继续给编码/MP4/RTSP
- `VI2(dev=4)`：480x800，继续给 LCD 预览
- `VI8(dev=8)`：416x416，专门给 NPU

检测线程流程：

1. `AW_MPI_VI_GetFrame(dev=8)`
2. 按 sample 现有逻辑拷贝 `Y + UV`
3. 调 `odet_init()/yolov3_tiny_odet()/odet_deinit()`
4. 按比例映射检测框到：
   - `480x800` LCD
   - `1920x1088` 编码链路
5. 用 `AW_MPI_RGN` 叠加矩形框

## 代码改动

- 新增：
  - `include/npu_module.h`
  - `include/npu_det_res.h`
  - `src/npu_module.c`
- 新增 vendor wrapper 源文件，直接复用 SDK sample：
  - `src/npu_vendor_vnn_main.c`
  - `src/npu_vendor_vnn_pre_process.c`
  - `src/npu_vendor_vnn_post_process.c`
  - `src/npu_vendor_post_process.cpp`
  - `src/npu_vendor_yolo_layer.cpp`
  - `src/npu_vendor_box.cpp`
- `Makefile` 增加了 C++ 编译支持与 sample 头文件路径
- `main.c` 增加可选参数：
  - `--enable-odet`
  - `--odet-model /mnt/UDISK/network_binary-yolov3-tiny.nb`

## 默认策略

为了不影响当前稳定基线：

- 默认 **不开启** 端侧检测
- 只有显式传 `--enable-odet` 才会启动 `VI8 + NPU 线程`

## 运行方式

普通模式：

```sh
./ipc_firmware
```

端侧检测模式：

```sh
./ipc_firmware --enable-odet --odet-model /mnt/UDISK/network_binary-yolov3-tiny.nb
```

## 当前阻塞点

代码已经接入本地工程，但还没完成 Ubuntu VM 交叉编译和板端验证。

当前从 Windows 到编译机 `192.168.73.135:22` 的 SSH 超时，说明 VM 当前不可达；等编译机恢复后，需要继续走：

1. `cd ~/mpp_camera_recorder && make clean && make`
2. 模型文件同步到板子 `/mnt/UDISK/`
3. 按硬性流程：
   - `adb push`
   - `adb shell reboot`
   - `sleep 15`
   - `adb kill-server && adb start-server`
   - 必要时二次 `adb push`
   - 只启动一次固件
4. 逐项确认：
   - LCD 正常
   - RTSP 正常
   - MP4 正常
   - 开启 ODET 后 LCD / RTSP / MP4 是否出现叠框
