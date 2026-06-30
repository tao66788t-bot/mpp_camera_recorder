# 摄像头驱动适配与调试笔记

> 适配平台：全志 V853 + OV5640，Linux 5.4
> 子方向：VI → ISP → VENC → MUX，带 RTSP 推流和 NPU 检测

---

## 1. 设备树配置（board.dts）

```dts
&csi0 {
    status = "okay";

    ov5640: camera@3c {                     // 0x3c = sensor的I2C地址
        compatible = "ovti,ov5640";         // 必须跟驱动的 of_match_table 一致

        reg = <0x3c>;                       // DTS w中 reg 是I2 C地址

        clocks = <&ccu CLK_CSI_MCLK>;
        clock-frequency = <24000000>;

        reset-gpios  = <&pio PE 9 GPIO_ACTIVE_LOW>;   // 低有效！
        powerdown-gpios = <&pio PE 10 GPIO_ACTIVE_HIGH>;

        port {
            ov5640_ep: endpoint {
                remote-endpoint = <&csi0_ep>;
                data-lanes = <1 2>;         // 用2 lane，1080p 不够4 lane
                link-frequencies = /bits/ 64 <336000000>;
            };
        };
    };
};
```

**关键坑点：**
- `reg = <0x3c>` 不是随便写的。OV5640 的 I2C 地址由硬件引脚决定（SCCB_ID 拉高→0x3c，拉低→0x3d）。看原理图或 `i2cdetect -y 0` 确认
- `GPIO_ACTIVE_LOW` 写错 → sensor 上电失败 → Chip ID 全是 0xff
- `data-lanes = <1 2>`：2 lane 够 1080p@30fps。如果 4K，必须 4 lane。lane 不够→帧率远低于设置值→DQBUF 超时


## 2. sensor 驱动适配的关键修改

### 2.1 上—电时序（对着 OV5640 datasheet Power Up Sequence 图）的

```c
static int ov5640_power_on(struct device *dev) {
    // 顺序不能错！错了 sensor 不启动
    clk_prepare_enable(sensor->xclk);            // ① MCLK 先稳定

    gpiod_set_value(sensor->pwdn_gpio, 0);       // ② PWDN低 → 上电
    usleep_range(5000, 10000);                   // ③ 等电压稳定 5-10ms

    gpiod_set_value(sensor->reset_gpio, 1);      // ④ RESET高 → 释放复位
    usleep_range(20000, 25000);                  // ⑤ 等 PLL 锁定 20ms

    // ⑥ 此时可以读 Chip ID 了
    return 0;
}
```

**坑点：** 上电时间和 datasheet 对不上 → PLL 没锁定就开始写寄存器 → 写进去的值全是0 → sensor 输出异常


### 2.2 PLL × 寄存器计算

```
MCLK = 24MHz（外部晶振）
目标 pixel_clk = 1920 × 1080 × 30fps × 1.2(blanking) ≈ 75MHz

PLL 输出 = MCLK × multiplier / (pre_div × sys_div)
pixel_clk = PLL 输出 / pixel_div

填寄存器：
  0x3035 = 0x21, 0x3036 = 0x69  → multiplier = 0x69
  0x3037 = 0x03                  → sys_div
  0x3108 = 0x01                  → pre_div
  0x3824 = 0x04                  → pixel_div

算错 → 帧率不对 / MIPI 时钟不对 → 数据乱码
```


## 3. —从点不亮到点亮的排查流程

```
Step 1: i2cdetect -y 0
  → 看到 0x3c ✓ = sensor 硬件在线
  → 看到 UU   ✓ = 驱动已经绑定
  → 什么都看不到 ✗ = 查 GPIO/电源/MCLK

Step 2: dmesg | grep ov5640
  → "chip_id = 0x5640" ✓ = sensor 型号对了
  → "chip_id = 0xffff" ✗ = sensor 没上电或 I2C 地址错了
  → "chip_id = 0x5641" ✗ = 类似的 sensor 但不是这个型号
  → 什么都没有 ✗ = 驱动没加载

Step 3: media-ctl -p
  → 看到 ov5640 entity → "ov5640 0-003c" ✓ = media controller 链路建立
  → 看不到 ✗ = v4l2_async_register_subdev 没成功

Step 4: v4l2-ctl -d /dev/video0 --list-formats
  → 看到 NV21/YU12 等格式 ✓ = sensor 和 CSI 控制器连接成功
  → 报错 ✗ = CSI 控制器没找到 sensor 或者 stream 没启动

Step 5: 实际抓一帧
  v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=NV21 --stream-mmap=3 --stream-to=/tmp/test.yuv --stream-count=1
```


## 4. MPP 应用层的 VI 配置坑

### 4.1 VI buffer 数量

```c
// 3 块buffer够 VI → VENC 一条线
// 但 VI → VENC + VI → VO 两条线同时跑，3块不够！
// 表现为：偶尔丢帧，VI GetFrame 返回 timeout
// 解决：nbufs 从 3 改成 6
v->attr.nbufs = 6;
```

### 4.2 SYS_Bind 顺序

```c
// 错误：先开始编码，再绑定 → VENC 收不到帧 → start 卡住
// 正确顺序：
// 1. 创建所有模块（VI/VENC/MUX 的 Create 和 SetAttr）
// 2. SYS_Bind 绑定通路
// 3. 启动（顺序：下游先启动，最后开 VI）
mux_start();
venc_start();
vi_start();   // VI 最后启动，一启动就是满通路，数据开始流
```

### 4.3 在线模式 vs 离线模式

```c
// 在线模式（online=1）：VI 的 buffer 直接给 VENC，CPU 完全碰不到帧
//   → 延迟低，但 VI 和 VENC 必须同频、同buffer池 → 调试困难
// 离线模式（online=0）：VI 出 V4L2 buffer，VENC 从 buffer 读
//   → CPU 可以用 GetFrame 取帧（给 NPU 做检测、存 YUV 调试等）
//   → 多一点点延迟，但架构灵活

v->attr.mOnlineEnable = 0;  // 调试阶段用离线模式，稳定后再切在线
```


## 5. 总结——能证明你做过的事

| 我实际做了 | 证据 |
|---|---|
| 在 V853 开发板上适配 OV5640 | DTS 修改记录、dmesg 日志 |
| 算过 PLL 寄存器值 | 寄存器计算表（datasheet 公式 → 实际值） |
| 排查过上电失败问题 | 调试笔记（i2cdetect → dmesg → 改 GPIO 极性 → 恢复） |
| 联调过 VI → VENC → MUX 全链路 | 配置文件和启动脚本 |
| 集成 NPU 检测 + RTSP 推流 | ipc_firmware.c 代码 |
