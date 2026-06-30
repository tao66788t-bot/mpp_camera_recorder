# V853 IPC 项目面试深挖

## 1. 项目最终结果

### 1.1 三路同时跑通

- LCD 本地预览
- MP4 本地录像
- RTSP 实时音视频推流

### 1.2 整体链路

- 视频
  - GC2053
  - MIPI CSI-2
  - CSI Parser
  - ISP
  - 分支A
    - VI2(dev=4, 480x800, NV12)
    - VO
    - LCD
  - 分支B
    - VI0(dev=0, 1920x1088, NV21)
    - VENC(H.265)
    - router线程
      - MUX
      - MP4
    - router线程
      - RTSP
      - VLC
- 音频
  - MIC
  - Codec/ADC
  - AI
  - AENC(AAC)
  - router线程
    - MUX
    - MP4
  - router线程
    - RTSP
    - VLC

## 2. 核心名词

### 2.1 SoC

- V853 是 SoC
- 不只是 CPU
- 内部集成
  - CSI/MIPI 接收
  - ISP
  - VIPP/scaler
  - VENC
  - VO
  - 音频模块
  - DDR 控制器

### 2.2 Sensor 到 ISP

- 控制链路
  - I2C
  - 配寄存器
    - 分辨率
    - 帧率
    - 曝光
    - 增益
- 图像数据链路
  - MIPI CSI-2
  - CSI 接收
  - CSI parser
  - ISP

### 2.3 ISP

- 图像信号处理器
- 负责
  - AE/AWB/统计
  - 去噪
  - Bayer 转 YUV/RGB
  - 颜色校正
  - Gamma

### 2.4 VIPP

- ISP 后面的输出侧硬件模块
- 负责
  - crop
  - scale/shrink
  - 输出格式整理
  - 接 DMA
  - 写 DDR buffer

### 2.5 VI

- MPP 层视频输入抽象
- 底层对应 video 节点和 capture pipeline

## 3. 为什么共用一个 ISP 却能有两路不同分辨率

### 3.1 关键结论

- 前端只有一套
  - Sensor
  - CSI
  - ISP
- 分叉发生在 ISP 后
  - VIPP/scaler0 -> video0 -> VI0
  - VIPP/scaler4 -> video4 -> VI2

### 3.2 两路实际用途

- VI0(dev=0)
  - 1920x1088
  - NV21
  - 给 VENC
- VI2(dev=4)
  - 480x800
  - NV12
  - 给 VO

## 4. ISP 后的数据怎么搬到两个 VI

### 4.1 数据搬运链路

- Sensor
  - CSI
  - ISP
  - VIPP0
    - DMA0
    - /dev/video0 buffer
    - VI0
    - VENC
  - VIPP4
    - DMA4
    - /dev/video4 buffer
    - VI2
    - VO

### 4.2 关键理解

- 不是 CPU memcpy 两份
- 是两路硬件输出口
- 每一路都有
  - scaler
  - DMA
  - buffer 队列
- 最终写进不同 DDR buffer

## 5. V4L2 用在哪

### 5.1 MPP 只是封装

- 应用层调用
  - AW_MPI_VI_CreateVipp
  - AW_MPI_VI_SetVippAttr
  - AW_MPI_VI_EnableVipp
  - AW_MPI_VI_CreateVirChn

### 5.2 底层真实工作

- 打开 video 节点
- format 协商
  - width/height
  - pixel format
  - fps
  - colorspace
- media pipeline
  - sensor subdev
  - mipi subdev
  - csi subdev
  - isp subdev
  - scaler subdev
- buffer 队列
  - vb2
- stream on/off

## 6. LCD / MP4 / RTSP 的完整视频流

### 6.1 LCD 路

- ISP
  - VI2(dev=4, 480x800 NV12)
  - VO
  - LCD

### 6.2 编码路

- ISP
  - VI0(dev=0, 1920x1088 NV21)
  - VENC(H.265)
  - router线程
    - MUX -> MP4
    - RTSP -> VLC

### 6.3 关键理解

- LCD 不经过编码
- MP4 和 RTSP 共用同一个 VENC 输出
- VENC 编码一次
- 后面消费两次

## 7. 为什么 MP4 正常而 LCD 不正常时不能先怪 ISP

### 7.1 原因

- MP4/RTSP 走
  - VI0 -> VENC
- LCD 走
  - VI2 -> VO

### 7.2 正确怀疑顺序

- VI2 -> VO 参数
- NV12/NV21 是否配错
- VO layer
- 板子运行态残留
- 未 reboot
- 重复启动

## 8. 音频怎么并到 MP4 和 RTSP

### 8.1 音频主链路

- MIC
  - Codec/ADC
  - AI
  - AENC(AAC)
  - router线程
    - MUX -> MP4
    - RTSP -> VLC

### 8.2 AI/AENC 职责

- AI
  - 采集 PCM
- AENC
  - 编成 AAC

### 8.3 为什么能同时进 MP4 和 RTSP

- AENC 产出 AAC
- router线程抓流
- 一份送 MUX
- 一份送 RTSP

## 9. 生产者-消费者模型

### 9.1 生产者

- VENC
  - 生产视频码流
- AENC
  - 生产音频码流

### 9.2 中间层

- producer线程 / router线程
  - 尽快抓流
  - 封装 packet
  - 带 PTS
  - 投递下游

### 9.3 消费者

- consumer1
  - MUX
- consumer2
  - RTSP

## 10. 共享节点 + 引用计数

### 10.1 基本思路

- producer 只生成一份 packet
- packet 带 ref_cnt
- MUX 处理完 ref_cnt -= 1
- RTSP 处理完 ref_cnt -= 1
- ref_cnt == 0 再释放

### 10.2 优点

- payload 只存一份
- 省内存
- 少一次复制
- CPU 拷贝成本低

### 10.3 缺点

- 实现复杂
- 要维护生命周期
- 慢消费者拖住整个队列
- 节点释放时机容易出错
- 难做单独丢帧策略

### 10.4 适合场景

- 码率高
- 内存紧张
- 团队能驾驭复杂并发

## 11. 拆成两条消费队列

### 11.1 基本思路

- producer 拿到一帧
  - enqueue 到 mux_queue
  - enqueue 到 rtsp_queue

### 11.2 优点

- 实现简单
- 两个消费者彻底解耦
- MUX 卡住不一定影响 RTSP
- RTSP 卡住不一定影响 MP4
- 可单独设计丢帧策略

### 11.3 缺点

- 更吃内存
- 若 payload 复制两份
  - CPU 拷贝成本更高
- 高码率时缓存成本更大

### 11.4 适合场景

- 工程优先求稳定
- 先跑通
- 后续再优化

## 12. 双缓冲与软件队列的区别

### 12.1 双缓冲

- 更偏底层
- 典型 ping-pong buffer
- 解决
  - 同时读写冲突
  - 低延迟流水

### 12.2 生产者消费者队列

- 更偏上层
- 解决
  - 上游生产快
  - 下游消费慢
  - 多消费者解耦

### 12.3 两者关系

- 双缓冲
  - 局部 buffer 轮转机制
- 软件队列
  - 全局数据调度机制

## 13. 高频拷打问题

### 13.1 协议类

- Sensor 到 ISP 走什么协议
  - I2C 配控制
  - MIPI CSI-2 传图像

### 13.2 架构类

- 为什么共用一个 ISP 还能两路不同尺寸
- VI0 和 VI2 是不是同一份帧
- V4L2 具体用在哪

### 13.3 工程类

- 为什么 MP4 正常不代表 LCD 正常
- 为什么 MP4 和 RTSP 能同时工作
- 为什么音频能同时进 MP4 和 RTSP
- 共享节点和双队列怎么选
- 双缓冲和软件队列有什么区别

## 14. 一段可直接复述的总结

### 14.1 总结主线

- 前端只有一套
  - Sensor
  - MIPI CSI-2
  - CSI
  - ISP
- ISP 后分叉
  - VI2 -> VO -> LCD
  - VI0 -> VENC -> MP4 / RTSP
- 音频侧
  - MIC -> AI -> AENC -> MP4 / RTSP
- 底层靠
  - V4L2
  - DMA
  - buffer 轮转
- 上层靠
  - producer / consumer
  - 码流分发

