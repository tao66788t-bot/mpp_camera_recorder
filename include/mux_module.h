/**
 * @file    mux_module.h
 * @brief   MUX 封装模块 —— 将编码后的码流封装为 MP4/MOV 等容器格式
 *
 * 职责：
 *   1. 创建 MUX 组和通道
 *   2. 设置输出文件路径和格式
 *   3. 接收 VENC 编码后的码流，封装写入文件
 *   4. 支持循环录制（文件分段）
 *
 * 调用顺序：
 *   mux_init() → mux_add_output() → mux_set_sps_pps() → mux_start()
 *   → ... → mux_stop() → mux_deinit()
 */

#ifndef MUX_MODULE_H
#define MUX_MODULE_H
#include "mpp_common.h"

/* ============ MUX 配置参数 ============ */
typedef struct {
    int             grp_id;          /* MUX 组 ID */
    int             enc_type;        /* 编码类型 PT_H264/PT_H265 */
    int             video_width;     /* 视频宽度 */
    int             video_height;    /* 视频高度 */
    int             video_fps;       /* 视频帧率 */
    int             enc_chn;         /* 关联的编码通道号 */
    bool            audio_enabled;   /* 是否启用音频轨 */
    int             audio_enc_type;  /* 音频编码类型，通常为 PT_AAC */
    int             audio_sample_rate; /* 音频采样率 */
    int             audio_bit_width; /* 音频位宽，单位 bit */
    int             audio_channels;  /* 音频声道数 */
    int             audio_pt_num_per_frm; /* 每帧采样点数 */
    int             max_file_duration; /* 单个文件最大时长（秒），0=不限制 */
    int             max_file_count;  /* 最大文件数 */
    bool            add_repair_info; /* 是否添加修复信息 */
    int             max_frms_tag_interval; /* 帧标签间隔 */
    char            output_dir[MAX_FILE_PATH_LEN];  /* 输出目录 */
    char            output_prefix[MAX_FILE_PATH_LEN]; /* 文件名前缀 */
} mux_config_t;

/* ============ MUX 句柄 ============ */
typedef struct {
    mux_config_t    cfg;
    MUX_GRP         grp;
    MUX_GRP_ATTR_S  grp_attr;
    int             muxer_id;        /* 当前录制通道的 muxer ID */
    char            current_file[MAX_FILE_PATH_LEN]; /* 当前输出文件路径 */
    bool            is_running;
} mux_handle_t;

/**
 * @brief 初始化 MUX 模块（创建 MUX 组、打开输出文件）
 * @param mux [out] MUX 句柄
 * @param cfg 配置参数
 * @return 0 成功
 */
int mux_init(mux_handle_t **mux, const mux_config_t *cfg);

/**
 * @brief 设置 SPS/PPS 头信息（从 VENC 获取后传入）
 * @param mux      MUX 句柄
 * @param sps_pps  VENC 的 SPS/PPS 数据
 * @param enc_chn  编码通道号
 * @return 0 成功
 */
int mux_set_sps_pps(mux_handle_t *mux, const VencHeaderData *sps_pps, int enc_chn);

/**
 * @brief 启动封装
 */
int mux_start(mux_handle_t *mux);

/**
 * @brief 停止封装
 */
int mux_stop(mux_handle_t *mux);

/**
 * @brief 销毁 MUX 模块
 */
int mux_deinit(mux_handle_t **mux);

/**
 * @brief 生成一个新的输出文件名（用于循环录制）
 * @param mux     MUX 句柄
 * @param out_path [out] 生成的文件路径
 * @param max_len 缓冲区长度
 * @return 0 成功
 */
int mux_gen_filename(mux_handle_t *mux, char *out_path, int max_len);

#endif /* MUX_MODULE_H */
