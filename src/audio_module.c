/**
 * @file    audio_module.c
 * @brief   音频采集+编码实现（适配 MPP SDK 实际 API）
 *
 * 初始化流程：
 *   1. AW_MPI_AI_SetPubAttr()     设置 AI 公共属性（采样率/位宽/声道）
 *   2. AW_MPI_AI_Enable()         使能 AI 设备
 *   3. AW_MPI_AI_CreateChn()      创建 AI 通道（只需 dev+chn 两个参数）
 *   4. AW_MPI_AENC_CreateChn()    创建 AENC 编码通道
 *   5. AW_MPI_SYS_Bind()          绑定 AI → AENC
 */

#include "audio_module.h"
#include "media_common_aio.h"

static AUDIO_STREAM_S g_aenc_stream_buf;
static bool           g_aenc_stream_valid = false;

static AUDIO_SOUND_MODE_E audio_sound_mode_from_channels(int channels)
{
    return (channels > 1) ? AUDIO_SOUND_MODE_STEREO : AUDIO_SOUND_MODE_MONO;
}

int audio_init(audio_handle_t **audio, const audio_config_t *cfg)
{
    MPP_CHECK_NULL(audio, "audio");
    MPP_CHECK_NULL(cfg, "cfg");

    audio_handle_t *a = (audio_handle_t *)calloc(1, sizeof(audio_handle_t));
    MPP_CHECK_NULL(a, "audio_handle_t alloc");
    a->cfg      = *cfg;
    a->ai_dev   = cfg->ai_dev;
    a->ai_chn   = cfg->ai_chn;
    a->aenc_chn = cfg->aenc_chn;

    int ret;

    /* ---- ① AI：设置公共属性 ---- */
    AIO_ATTR_S aio_attr;
    memset(&aio_attr, 0, sizeof(aio_attr));
    aio_attr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(cfg->sample_rate);
    aio_attr.enBitwidth   = map_BitWidth_to_AUDIO_BIT_WIDTH_E(cfg->bit_width);
    aio_attr.enSoundmode  = audio_sound_mode_from_channels(cfg->channels);
    aio_attr.u32FrmNum    = cfg->frm_num;
    aio_attr.mPtNumPerFrm = cfg->pt_num_per_frm;
    aio_attr.mChnCnt      = cfg->channels;

    ret = AW_MPI_AI_SetPubAttr(a->ai_dev, &aio_attr);
    MPP_CHECK_RET(ret, "AW_MPI_AI_SetPubAttr");
    alogd("audio: AI pub attr set: %dHz/%dbit/%dch", cfg->sample_rate, cfg->bit_width, cfg->channels);

    /* ---- ② AI：使能设备 ---- */
    ret = AW_MPI_AI_Enable(a->ai_dev);
    MPP_CHECK_RET(ret, "AW_MPI_AI_Enable");

    /* ---- ③ AI：创建通道 ---- */
    ret = AW_MPI_AI_CreateChn(a->ai_dev, a->ai_chn);
    MPP_CHECK_RET(ret, "AW_MPI_AI_CreateChn");
    alogd("audio: AI chn[%d:%d] created", a->ai_dev, a->ai_chn);

    /* ---- ④ AENC：创建编码通道 ---- */
    AENC_CHN_ATTR_S aenc_attr;
    memset(&aenc_attr, 0, sizeof(aenc_attr));
    aenc_attr.AeAttr.Type          = PT_AAC;
    aenc_attr.AeAttr.sampleRate    = cfg->sample_rate;
    aenc_attr.AeAttr.channels      = cfg->channels;
    aenc_attr.AeAttr.bitRate       = cfg->aenc_bitrate;
    aenc_attr.AeAttr.bitsPerSample = cfg->bit_width;
    /*
     * Match the SDK mux sample: keep AAC elementary stream raw.
     * MUX will write container metadata itself, and RTP/AAC also wants raw AU.
     */
    aenc_attr.AeAttr.attachAACHeader = 0;

    ret = AW_MPI_AENC_CreateChn(a->aenc_chn, &aenc_attr);
    MPP_CHECK_RET(ret, "AW_MPI_AENC_CreateChn");
    alogd("audio: AENC chn[%d] created, AAC bitrate=%d", a->aenc_chn, cfg->aenc_bitrate);

    /* ---- ⑤ SYS_Bind: AI → AENC ---- */
    {
        MPP_CHN_S ai_src   = { MOD_ID_AI,   a->ai_dev,   a->ai_chn   };
        MPP_CHN_S aenc_dst = { MOD_ID_AENC, 0,            a->aenc_chn };
        ret = AW_MPI_SYS_Bind(&ai_src, &aenc_dst);
        MPP_CHECK_RET(ret, "AW_MPI_SYS_Bind(AI→AENC)");
        alogd("audio: bound AI[%d:%d] → AENC[%d]", a->ai_dev, a->ai_chn, a->aenc_chn);
    }

    memset(&g_aenc_stream_buf, 0, sizeof(g_aenc_stream_buf));
    *audio = a;
    return 0;
}

int audio_start(audio_handle_t *audio)
{
    MPP_CHECK_NULL(audio, "audio");
    if (audio->is_running) { alogw("audio_start: already running"); return 0; }

    int ret = AW_MPI_AI_EnableChn(audio->ai_dev, audio->ai_chn);
    MPP_CHECK_RET(ret, "AW_MPI_AI_EnableChn");

    ret = AW_MPI_AENC_StartRecvPcm(audio->aenc_chn);
    MPP_CHECK_RET(ret, "AW_MPI_AENC_StartRecvPcm");

    audio->is_running = true;
    alogd("audio_start: AI[%d:%d] + AENC[%d] started",
          audio->ai_dev, audio->ai_chn, audio->aenc_chn);
    return 0;
}

int audio_stop(audio_handle_t *audio)
{
    MPP_CHECK_NULL(audio, "audio");
    if (!audio->is_running) return 0;

    AW_MPI_AENC_StopRecvPcm(audio->aenc_chn);
    AW_MPI_AI_DisableChn(audio->ai_dev, audio->ai_chn);

    audio->is_running = false;
    alogd("audio_stop: stopped");
    return 0;
}

int audio_deinit(audio_handle_t **audio)
{
    if (!audio || !*audio) return 0;

    audio_handle_t *a = *audio;
    if (a->is_running) audio_stop(a);

    {
        MPP_CHN_S ai_src   = { MOD_ID_AI,   a->ai_dev,   a->ai_chn   };
        MPP_CHN_S aenc_dst = { MOD_ID_AENC, 0,            a->aenc_chn };
        AW_MPI_SYS_UnBind(&ai_src, &aenc_dst);
    }

    AW_MPI_AENC_DestroyChn(a->aenc_chn);
    AW_MPI_AI_DestroyChn(a->ai_dev, a->ai_chn);
    AW_MPI_AI_Disable(a->ai_dev);

    alogd("audio_deinit: OK");
    free(a);
    *audio = NULL;
    return 0;
}

int audio_get_stream(audio_handle_t *audio,
                     uint8_t **data, unsigned int *size,
                     unsigned long long *pts_us, int timeout_ms)
{
    MPP_CHECK_NULL(audio, "audio");

    if (g_aenc_stream_valid) {
        audio_release_stream(audio);
    }

    memset(&g_aenc_stream_buf, 0, sizeof(g_aenc_stream_buf));

    int ret = AW_MPI_AENC_GetStream(audio->aenc_chn, &g_aenc_stream_buf, timeout_ms);
    if (ret != 0) {
        return -1;
    }

    if (g_aenc_stream_buf.pStream != NULL && g_aenc_stream_buf.mLen > 0) {
        *data   = g_aenc_stream_buf.pStream;
        *size   = g_aenc_stream_buf.mLen;
        *pts_us = g_aenc_stream_buf.mTimeStamp;
    } else {
        *data   = NULL;
        *size   = 0;
        *pts_us = 0;
    }

    g_aenc_stream_valid = true;
    return 0;
}

int audio_release_stream(audio_handle_t *audio)
{
    MPP_CHECK_NULL(audio, "audio");
    if (!g_aenc_stream_valid) return 0;

    AW_MPI_AENC_ReleaseStream(audio->aenc_chn, &g_aenc_stream_buf);
    g_aenc_stream_valid = false;
    return 0;
}
