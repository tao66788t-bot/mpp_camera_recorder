# 代码结构说明（2026-07-01）

## 1. 程序入口

入口文件：

`D:\BaiduNetdiskDownload\全志mpp文档资料和源代码\mpp_camera_recorder\src\main.c`

`main.c` 负责三件事：

1. 组织所有模块配置
2. 初始化各条音视频/NPU链路
3. 绑定和启动生产者-消费者式的数据流

## 2. 整体数据流

### 2.1 视频录像与 RTSP 主链

`Sensor -> CSI -> ISP -> VI0(1920x1088) -> VENC(H.265) -> {MUX, RTSP}`

### 2.2 LCD 预览链

`Sensor -> CSI -> ISP -> VI2(480x800) -> VO -> LCD`

### 2.3 人脸检测链

`Sensor -> CSI -> ISP -> VI(dev=8, 480x288) -> NPU -> Region Overlay`

### 2.4 音频链

`MIC/AI -> AENC(AAC) -> {MUX, RTSP}`

## 3. 主要源码文件职责

### 3.1 `main.c`

负责：

- 解析启动参数
- 初始化 `VI0 / VI2 / VO / VENC / AI/AENC / MUX / RTSP / NPU`
- 建立绑定关系
- 控制程序生命周期

### 3.2 `vi_module.c` / `vi_module.h`

负责：

- 创建 VI 设备与通道
- 配置分辨率、像素格式、colorspace、buffer 数
- 启动 ISP
- 对接 MPP 的 VI 输入能力

这是视频采集入口模块。

### 3.3 `vo_module.c` / `vo_module.h`

负责：

- 打开显示设备
- 配置显示图层和通道
- 设置显示窗口尺寸
- 接收来自 VI2 的预览帧

这是 LCD 显示模块。

### 3.4 `venc_module.c` / `venc_module.h`

负责：

- 创建 H.265 编码器
- 配置码率、帧率、GOP、QP、VBV
- 获取 SPS/PPS
- 负责录像和 RTSP 公用的编码输出

这是视频编码模块。

### 3.5 `audio_module.c` / `audio_module.h`

负责：

- 初始化 AI 采集
- 初始化 AENC AAC 编码
- 周期性取音频码流

这是音频采集和编码模块。

### 3.6 `mux_module.c` / `mux_module.h`

负责：

- 创建 MP4 mux group
- 接收视频码流和音频码流
- 写入 `/mnt/UDISK/cam_*.mp4`

这是本地录像封装模块。

### 3.7 `rtsp_module.c` / `rtsp_module.h`

负责：

- 建立 RTSP server
- 处理 `OPTIONS/DESCRIBE/SETUP/PLAY`
- 将 H.265 / AAC 打成 RTP
- 对外提供 RTSP 音视频播放

这是网络推流模块。

### 3.8 `npu_module.c` / `npu_module.h`

负责：

- 加载 `Facedet_480_288_nv12.nb`
- 驱动 480x288 检测支路
- 做检测后处理
- 把检测框叠加到 LCD 预览和编码画面

这是人脸检测模块。

### 3.9 `diag_vi_dump.c` / `diag_vi_dump.h`

负责：

- 抓 VI 原始帧
- 用于判断问题出在 ISP/VI 还是 VO/LCD

这是诊断模块。

## 4. 板端运行依赖

### 4.1 编译可生成

- `ipc_firmware`

### 4.2 编译不能生成

- `Facedet_480_288_nv12.nb`
- `isp0_1920_1088_20_0_ctx_saved.bin`

原因：

- `.nb` 是 NPU 模型包
- `.bin` 是板端 ISP 运行时状态快照

## 5. 为什么回滚不能只看源码

因为这个项目真实跑通依赖四层：

1. 源码
2. 可执行固件
3. 模型文件
4. ISP context

任何一层丢失，都可能出现“代码看起来没问题，但板子跑出来不对”。

## 6. 适合面试时的讲法

可以把这个项目概括为：

“我在 V853 上把一个多路 IPC 固件链路打通了。主链是 1080p 视频编码、本地 MP4 和 RTSP 音视频推流；旁路是 480x800 LCD 预览；另外还拉了一路 480x288 的检测支路给 NPU 做人脸检测，并把结果叠回预览和编码输出。整个项目的关键难点不只是编码和推流，还包括 ISP/VI/VO 的链路诊断、板端部署稳定性和可回滚性建设。” 
