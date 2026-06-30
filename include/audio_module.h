/**
 * @file    audio_module.h
 * @brief   音频采集+编码模块 —— AI (Audio Input) + AENC (Audio Encoder)
 *
 * 数据流：
 *   MIC → ADC → I2S → AI → [SYS_Bind] → AENC → AAC码流
 *
 * 调用顺序：
 *   audio_init() → audio_start() → ... → audio_stop() → audio_deinit()
 */

#ifndef AUDIO_MODULE_H
#define AUDIO_MODULE_H
#include "mpp_common.h"

#include "mm_comm_aio.h"
#include "mpi_ai.h"
#include "mm_comm_aenc.h"
#include "mpi_aenc.h"

/* ============ Audio 配置参数 ============ */
typedef struct {
    int             ai_dev;          /* AI 设备号，通常为 0 */
    int             ai_chn;          /* AI 通道号 */
    int             aenc_chn;        /* AENC 通道号 */
    int             sample_rate;     /* 采样率，如 16000 */
    int             channels;        /* 声道数，1=单声道 */
    int             bit_width;       /* 位宽，16 */
    int             sound_mode;      /* 声音模式 */
    int             pt_num_per_frm;  /* 每帧采样点数，1024 */
    int             frm_num;         /* buffer中的帧数 */
    int             aenc_bitrate;    /* AAC 编码码率 bps */
} audio_config_t;

/* ============ Audio 句柄 ============ */
typedef struct {
    audio_config_t  cfg;
    AUDIO_DEV       ai_dev;
    AI_CHN          ai_chn;
    AENC_CHN        aenc_chn;
    bool            is_running;
} audio_handle_t;

int audio_init(audio_handle_t **audio, const audio_config_t *cfg);
int audio_start(audio_handle_t *audio);
int audio_stop(audio_handle_t *audio);
int audio_deinit(audio_handle_t **audio);

/**
 * @brief 从 AENC 取一帧编码数据
 * @return 0 成功，-1 超时/错误
 */
int audio_get_stream(audio_handle_t *audio,
                     uint8_t **data, unsigned int *size,
                     unsigned long long *pts_us, int timeout_ms);
int audio_release_stream(audio_handle_t *audio);

#endif /* AUDIO_MODULE_H */
