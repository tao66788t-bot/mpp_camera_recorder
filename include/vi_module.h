/**
 * @file    vi_module.h
 * @brief   VI（Video Input）摄像头采集模块
 *
 * 职责：
 *   1. 配置并初始化 VI 设备（Vipp）
 *   2. 设置摄像头参数（分辨率/格式/帧率/颜色空间）
 *   3. 启动 ISP（自动曝光/白平衡/降噪）
 *   4. 创建/启用虚拟通道
 *
 * 调用顺序：
 *   vi_init() → vi_start() → ... → vi_stop() → vi_deinit()
 *
 * 数据流（硬件自动完成，CPU不需要取帧）：
 *   OV5640 Sensor → MIPI → VI → ISP → DDR
 *                                 ↓
 *                           [通过SYS_Bind] → VENC
 */

#ifndef VI_MODULE_H
#define VI_MODULE_H
#include "mpp_common.h"

/* ============ VI 配置参数 ============ */
typedef struct {
    int             dev_id;         /* VI 设备号，通常为 0 */
    int             isp_dev;        /* ISP 设备号，通常为 0 */
    int             chn_id;         /* 虚拟通道号，通常为 0 */
    int             width;          /* 采集宽度，如 1920 */
    int             height;         /* 采集高度，如 1080 */
    int             fps;            /* 帧率，如 25 */
    int             pix_fmt;        /* 像素格式（MM_PIXEL_FORMAT_xxx） */
    int             color_space;    /* 颜色空间（V4L2_COLORSPACE_xxx） */
    int             buf_num;        /* V4L2 buffer 数量，通常 3~6 */
    int             wdr_en;         /* 是否启用 WDR 宽动态 */
    int             online_en;      /* 是否启用在线模式（VI直连VENC） */
    int             online_share_buf_num;
    int             drop_frame_num; /* 丢帧数（离线模式） */
    int             saturation;     /* 饱和度调整值，0=不变 */
    bool            encpp_enable;   /* 是否启用编码预处理 */
    int             use_current_win; /* 复用 sensor 窗口（多VIPP共享sensor时=1）*/
    int             nplanes;        /* plane 数量：2=semi-planar, 3=planar */
} vi_config_t;

/* ============ VI 句柄 ============ */
typedef struct {
    vi_config_t     cfg;
    VI_DEV          dev;
    ISP_DEV         isp_dev;
    VI_CHN          chn;
    VI_ATTR_S       attr;
    bool            is_running;
} vi_handle_t;

/**
 * @brief 初始化 VI 模块（创建设备、配置属性、启动 ISP、创建通道）
 * @param vi   [out] VI 句柄
 * @param cfg  配置参数
 * @return 0 成功
 */
int vi_init(vi_handle_t **vi, const vi_config_t *cfg);

/**
 * @brief 启动 VI（使能虚拟通道）
 * @param vi  VI 句柄
 * @return 0 成功
 */
int vi_start(vi_handle_t *vi);

/**
 * @brief 停止 VI（关闭虚拟通道）
 */
int vi_stop(vi_handle_t *vi);

/**
 * @brief 销毁 VI 模块（释放所有资源）
 */
int vi_deinit(vi_handle_t **vi);

#endif /* VI_MODULE_H */
