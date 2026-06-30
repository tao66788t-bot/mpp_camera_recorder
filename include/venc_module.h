/**
 * @file    venc_module.h
 * @brief   VENC 硬件视频编码模块
 *
 * 职责：
 *   1. 根据配置创建硬件编码通道（H264/H265/MJPEG）
 *   2. 设置码率控制参数（CBR/VBR/FIXQP）
 *   3. 设置 GOP / 帧率 / 3DNR / 裁剪 / 旋转等
 *   4. 通过 SYS_Bind 从 VI 接收帧，编码后发给 MUX
 *
 * 调用顺序：
 *   venc_init() → venc_get_sps_pps() → venc_start() → ... → venc_stop() → venc_deinit()
 */

#ifndef VENC_MODULE_H
#define VENC_MODULE_H
#include "mpp_common.h"

/* ============ VENC 配置参数 ============ */
typedef struct {
    /* 基本属性 */
    int             chn_id;          /* 编码通道号 */
    int             enc_type;        /* PT_H264 / PT_H265 / PT_MJPEG */
    int             src_width;       /* 输入宽度 */
    int             src_height;      /* 输入高度 */
    int             dst_width;       /* 输出宽度 */
    int             dst_height;      /* 输出高度 */
    int             src_frame_rate;  /* 输入帧率 */
    int             dst_frame_rate;  /* 输出帧率 */
    int             pix_fmt;         /* 输入像素格式 */
    int             color_space;     /* 颜色空间 */

    /* 码率控制 */
    int             rc_mode;         /* RC_MODE_CBR/VBR/FIXQP/ABR */
    int             bit_rate;        /* 目标码率，如 2000000 */
    int             product_mode;    /* VENC_PRODUCT_NORMAL_MODE / VENC_PRODUCT_IPC_MODE */
    int             sensor_type;     /* 0=SDK default; only set when sensor mode is confirmed */
    int             init_qp;         /* 初始 QP */
    int             min_i_qp;        /* 最小 I 帧 QP */
    int             max_i_qp;        /* 最大 I 帧 QP */
    int             min_p_qp;        /* 最小 P 帧 QP */
    int             max_p_qp;        /* 最大 P 帧 QP */
    int             quality;         /* 画质等级 */
    int             moving_th;       /* 运动检测阈值 */
    float           i_bits_coef;     /* I 帧码率系数 */
    float           p_bits_coef;     /* P 帧码率系数 */

    /* GOP */
    int             gop_mode;        /* VENC_GOPMODE_NORMALP/DUALP/SMARTP */
    int             gop_size;        /* GOP 大小 */
    int             key_frame_interval;

    /* 高级特性 */
    int             enc_profile;     /* 编码 profile */
    int             rotate;          /* 旋转角度：0/90/180/270 */
    bool            fast_enc;        /* 快速编码模式 */
    bool            color2grey;      /* 转灰度 */
    bool            horizon_flip;    /* 水平镜像 */
    bool            enable_smart;    /* SmartP 编码 */
    int             svc_layer;       /* SVC 层数 */

    /* 降噪 */
    bool            d2nr_enable;     /* 2D 降噪 */
    int             d2nr_strength_y;
    int             d2nr_strength_c;
    bool            d3nr_enable;     /* 3D 降噪 */

    /* 裁剪 */
    bool            crop_enable;
    int             crop_x, crop_y, crop_w, crop_h;

    /* ROI */
    int             roi_num;
    int             roi_qp;

    /* VUI 时间信息 */
    bool            vui_timing_enable;

    /* 在线模式 */
    bool            online_enable;
    int             online_share_buf_num;

    /* Vbv 缓冲 */
    int             vbv_buf_size;
    int             vbv_thresh_size;

    /* 丢帧 */
    int             drop_frame_num;

    /* Encpp */
    bool            encpp_enable;
    bool            isp_ve_linkage;

    /* SuperFrame */
    int             super_frm_mode;

    /* 高级参考帧 */
    int             advanced_ref_base;
    int             advanced_ref_enhance;
    int             advanced_ref_refbase_en;

    /* LBC 模式 */
    int             ref_frame_lbc_mode;

} venc_config_t;

/* ============ VENC 句柄 ============ */
typedef struct {
    venc_config_t   cfg;
    VENC_CHN        chn;
    VENC_CHN_ATTR_S attr;
    VENC_RC_PARAM_S rc_param;
    VencHeaderData  sps_pps;        /* 编码头信息（给MUX用） */
    bool            is_running;
} venc_handle_t;

/**
 * @brief 初始化 VENC 模块（创建编码通道、设置所有参数）
 * @param enc [out] 编码器句柄
 * @param cfg 配置参数
 * @return 0 成功
 */
int venc_init(venc_handle_t **enc, const venc_config_t *cfg);

/**
 * @brief 获取 SPS/PPS 头信息（H264/H265 需要在绑定 MUX 前调用）
 * @param enc 编码器句柄（sps_pps 字段被填充）
 * @return 0 成功
 */
int venc_get_sps_pps(venc_handle_t *enc);

/**
 * @brief 启动编码（开始接收帧）
 */
int venc_start(venc_handle_t *enc);

/**
 * @brief 停止编码
 */
int venc_stop(venc_handle_t *enc);

/**
 * @brief 销毁编码模块
 */
int venc_deinit(venc_handle_t **enc);

#endif /* VENC_MODULE_H */
