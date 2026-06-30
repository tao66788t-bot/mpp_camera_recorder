/**
 * @file    rtsp_module.h
 * @brief   RTSP 服务器 + RTP 推流 + 码流分发线程
 *
 * 这个模块做三件事：
 *   1. RTSP 服务：TCP 端口 554，处理 OPTIONS/DESCRIBE/SETUP/PLAY/TEARDOWN
 *   2. RTP 打包：H.265 NAL 单元按 FU-A 分片，每个 RTP 包 ≤ MTU
 *   3. 码流分发线程：从 VENC/AENC 拉编码帧，分发给 MUX（存MP4）+ RTP（推流）
 *
 * 架构：
 *   ┌──────────────────────────────────────────────┐
 *   │  stream_router_thread (CPU)                   │
 *   │                                                │
 *   │  VENC_GetStream() ─→ MUX_SendFrame(video)     │
 *   │                   ─→ rtsp_push_h265()          │
 *   │                                                │
 *   │  AENC_GetStream() ─→ MUX_SendFrame(audio)     │
 *   │                   ─→ rtsp_push_aac()           │
 *   └──────────────────────────────────────────────┘
 *
 * RTSP 协议流程：
 *   客户端 → OPTIONS  → 服务器返回支持的命令
 *   客户端 → DESCRIBE → 服务器返回 SDP（描述音视频流）
 *   客户端 → SETUP    → 协商 RTP/UDP 端口
 *   客户端 → PLAY     → 开始推送 RTP 流
 *   客户端 → TEARDOWN → 结束会话
 */

#ifndef RTSP_MODULE_H
#define RTSP_MODULE_H
#include "mpp_common.h"

/* ---- 传输模式 ---- */
typedef enum {
    RTSP_TRANSPORT_TCP = 0,  /* RTP/AVP/TCP interleaved — 走RTSP连接, 过防火墙/ADB */
    RTSP_TRANSPORT_UDP = 1,  /* RTP/AVP/UDP — 局域网低延迟 */
} rtsp_transport_e;

/* ---- RTSP 配置 ---- */
typedef struct {
    int             rtsp_port;       /* 监听端口，默认 554 */
    int             video_enc_chn;   /* VENC 通道号 */
    int             aenc_chn;        /* AENC 通道号 */
    int             mux_grp;         /* MUX 组号 */
    bool            audio_enabled;   /* 是否在 RTSP 中发布音频 */
    int             audio_sample_rate; /* AAC 采样率 */
    int             audio_channels;  /* AAC 声道数 */
    unsigned int    video_payload_type;  /* RTP payload type，H265=96 */
    unsigned int    audio_payload_type;  /* RTP payload type，AAC=97 */
    unsigned int    ssrc;            /* RTP SSRC */
    unsigned int    max_packet_size; /* 最大 RTP 包大小，默认 1458 */
    rtsp_transport_e transport_mode; /* TCP interleaved(默认) 或 UDP */

    /* 输出目标 */
    const char     *output_file;     /* MP4 输出文件路径（给 MUX） */
} rtsp_config_t;

/* ---- RTSP 状态 ---- */
typedef enum {
    RTSP_STATE_INIT  = 0,   /* 未启动 */
    RTSP_STATE_READY = 1,   /* SETUP 完成，等待 PLAY */
    RTSP_STATE_PLAYING = 2, /* 正在推流 */
} rtsp_state_e;

/* ---- RTSP 句柄 ---- */
typedef struct {
    rtsp_config_t   cfg;
    rtsp_state_e    state;
    volatile bool   running;         /* 路由线程运行标志 */

    /* 客户端信息 */
    char            client_ip[64];
    int             client_rtp_video_port;
    int             client_rtp_audio_port;
    int             client_rtcp_video_port;
    int             client_rtcp_audio_port;

    /* TCP interleaved channel 号 */
    int             client_interleaved_video_rtp;    /* 通常 = 0 */
    int             client_interleaved_video_rtcp;   /* 通常 = 1 */
    int             client_interleaved_audio_rtp;    /* 通常 = 2 */
    int             client_interleaved_audio_rtcp;   /* 通常 = 3 */

    /* RTP 序号和时间戳 */
    unsigned short  rtp_seq_video;
    unsigned short  rtp_seq_audio;
    unsigned int    rtp_ts_video;     /* 视频 RTP 时间戳（90kHz） */
    unsigned int    rtp_ts_audio;     /* 音频 RTP 时间戳（采样率） */

    /* Sockets */
    int             rtsp_listen_fd;   /* RTSP TCP 监听 socket */
    int             rtsp_client_fd;   /* RTSP 客户端连接 */
    int             rtp_video_fd;     /* RTP 视频 UDP socket */
    int             rtp_audio_fd;     /* RTP 音频 UDP socket */

    /* 线程 */
    pthread_t       rtsp_thread;      /* RTSP 服务线程 */
    pthread_t       router_thread;    /* 码流分发线程 */

    /* Session ID */
    char            session_id[32];

    /* Copied H.265 VPS/SPS/PPS header from VENC. */
    unsigned char   video_header[2048];
    unsigned int    video_header_len;
} rtsp_handle_t;

/**
 * @brief 初始化 RTSP 模块（创建 RTSP 监听 socket + RTP UDP sockets + 启动线程）
 */
int rtsp_init(rtsp_handle_t **rtsp, const rtsp_config_t *cfg);

/**
 * @brief 启动码流分发线程
 */
int rtsp_start(rtsp_handle_t *rtsp);

/**
 * @brief 停止所有线程
 */
int rtsp_stop(rtsp_handle_t *rtsp);

/**
 * @brief 销毁
 */
int rtsp_deinit(rtsp_handle_t **rtsp);

/**
 * @brief 将 H.265 NAL 打包成 RTP FU-A 并通过 UDP 发送
 * @param rtsp    句柄
 * @param nal_data  NAL 数据（包含起始码或 NAL 长度前缀）
 * @param nal_size  NAL 大小
 * @param pts_us   PTS（微秒）
 * @return 发送的包数，<0 失败
 */
int rtsp_push_h265(rtsp_handle_t *rtsp,
                   const uint8_t *nal_data, unsigned int nal_size,
                   unsigned long long pts_us);

/**
 * @brief 将 AAC 音频帧打包成 RTP 并通过 UDP 发送
 */
int rtsp_push_aac(rtsp_handle_t *rtsp,
                  const uint8_t *aac_data, unsigned int aac_size,
                  unsigned long long pts_us);

/**
 * @brief 设置码流分发线程的句柄引用（在 main 里 init 之后调用）
 * @param venc_chn  VENC 通道号
 * @param aenc_chn  AENC 通道号
 * @param mux_grp   MUX 组号
 * @param rtsp      RTSP 句柄
 */
int rtsp_set_video_header(rtsp_handle_t *rtsp, const VencHeaderData *header);
void rtsp_set_router_refs(int venc_chn, int aenc_chn,
                          int mux_grp, rtsp_handle_t *rtsp);

#endif /* RTSP_MODULE_H */
