/**
 * @file    mux_module.c
 * @brief   MUX 封装模块实现
 *
 * 初始化流程：
 *   1. 填充 MUX_GRP_ATTR_S（指定编码类型/分辨率/帧率/关联编码通道）
 *   2. AW_MPI_MUX_CreateGrp()     创建 MUX 组
 *   3. 打开输出文件 → AW_MPI_MUX_CreateChn() 创建 MUX 通道
 *   4. AW_MPI_MUX_StartGrp()      启动
 */

#include "mux_module.h"

/* 简单文件写模式的缓存大小 */
#define SIMPLE_CACHE_SIZE    (64 * 1024)

int mux_init(mux_handle_t **mux, const mux_config_t *cfg)
{
    MPP_CHECK_NULL(mux, "mux");
    MPP_CHECK_NULL(cfg, "cfg");

    mux_handle_t *m = (mux_handle_t *)calloc(1, sizeof(mux_handle_t));
    MPP_CHECK_NULL(m, "mux_handle_t alloc");
    m->cfg  = *cfg;
    m->grp  = cfg->grp_id;

    /* ---- 1. 配置 MUX 组属性 ---- */
    memset(&m->grp_attr, 0, sizeof(MUX_GRP_ATTR_S));
    m->grp_attr.mVideoAttrValidNum = 1;
    m->grp_attr.mVideoAttr[0].mVideoEncodeType = cfg->enc_type;
    m->grp_attr.mVideoAttr[0].mWidth           = cfg->video_width;
    m->grp_attr.mVideoAttr[0].mHeight          = cfg->video_height;
    m->grp_attr.mVideoAttr[0].mVideoFrmRate    = cfg->video_fps * 1000;
    m->grp_attr.mVideoAttr[0].mVeChn           = cfg->enc_chn;
    m->grp_attr.mAudioEncodeType               = PT_AAC;
    m->grp_attr.mBitsPerSample                 = 16;
    m->grp_attr.mSamplesPerFrame               = 1024;
    m->grp_attr.mSampleRate                    = 16000;

    /* ---- 2. 创建 MUX 组 ---- */
    int ret = AW_MPI_MUX_CreateGrp(m->grp, &m->grp_attr);
    MPP_CHECK_RET(ret, "AW_MPI_MUX_CreateGrp");

    /*
     * In non-tunnel mode, MUX expects input nodes to be preallocated and the
     * source VENC channel to be mapped to the streamId used by
     * AW_MPI_MUX_SendVideoStream(). Our RTSP router pushes video on streamId 0.
     */
    ret = AW_MPI_MUX_SetInputNodeCount(m->grp, 8);
    MPP_CHECK_RET(ret, "AW_MPI_MUX_SetInputNodeCount");

    ret = AW_MPI_MUX_SetVeChnBindStreamId(m->grp, cfg->enc_chn, 0);
    MPP_CHECK_RET(ret, "AW_MPI_MUX_SetVeChnBindStreamId");

    /* ---- 3. 生成输出文件名并创建 MUX 通道 ---- */
    char filepath[MAX_FILE_PATH_LEN];
    mux_gen_filename(m, filepath, sizeof(filepath));

    /* 打开输出文件 */
    int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        aloge("mux_init: cannot open file '%s'", filepath);
        return -1;
    }

    /* 填充 MUX 通道属性 */
    MUX_CHN_ATTR_S chn_attr;
    memset(&chn_attr, 0, sizeof(MUX_CHN_ATTR_S));
    chn_attr.mMuxerId         = 0;
    chn_attr.mMediaFileFormat = MEDIA_FILE_FORMAT_MP4;
    chn_attr.mMaxFileDuration = cfg->max_file_duration * 1000;
    chn_attr.mFallocateLen    = 0;
    chn_attr.mCallbackOutFlag = FALSE;
    chn_attr.mFsWriteMode     = FSWRITEMODE_SIMPLECACHE;
    chn_attr.mSimpleCacheSize = SIMPLE_CACHE_SIZE;
    chn_attr.mAddRepairInfo   = cfg->add_repair_info;
    chn_attr.mMaxFrmsTagInterval = cfg->max_frms_tag_interval;

    ret = AW_MPI_MUX_CreateChn(m->grp, 0, &chn_attr, fd);
    close(fd);
    MPP_CHECK_RET(ret, "AW_MPI_MUX_CreateChn");

    m->muxer_id = 0;
    strncpy(m->current_file, filepath, MAX_FILE_PATH_LEN - 1);

    alogd("mux_init: grp=%d file=%s OK", m->grp, filepath);
    *mux = m;
    return 0;
}

int mux_set_sps_pps(mux_handle_t *mux, const VencHeaderData *sps_pps, int enc_chn)
{
    MPP_CHECK_NULL(mux, "mux");
    MPP_CHECK_NULL(sps_pps, "sps_pps");

    int ret;
    if (mux->cfg.enc_type == PT_H264) {
        ret = AW_MPI_MUX_SetH264SpsPpsInfo(mux->grp, enc_chn, sps_pps);
        MPP_CHECK_RET(ret, "AW_MPI_MUX_SetH264SpsPpsInfo");
    } else if (mux->cfg.enc_type == PT_H265) {
        ret = AW_MPI_MUX_SetH265SpsPpsInfo(mux->grp, enc_chn, sps_pps);
        MPP_CHECK_RET(ret, "AW_MPI_MUX_SetH265SpsPpsInfo");
    }
    alogd("mux_set_sps_pps: grp=%d OK", mux->grp);
    return 0;
}

int mux_start(mux_handle_t *mux)
{
    MPP_CHECK_NULL(mux, "mux");
    if (mux->is_running) { alogw("mux_start: already running"); return 0; }

    int ret = AW_MPI_MUX_StartGrp(mux->grp);
    MPP_CHECK_RET(ret, "AW_MPI_MUX_StartGrp");

    mux->is_running = true;
    alogd("mux_start: grp=%d OK", mux->grp);
    return 0;
}

int mux_stop(mux_handle_t *mux)
{
    MPP_CHECK_NULL(mux, "mux");
    if (!mux->is_running) return 0;

    AW_MPI_MUX_StopGrp(mux->grp);
    mux->is_running = false;
    alogd("mux_stop: grp=%d OK", mux->grp);
    return 0;
}

int mux_deinit(mux_handle_t **mux)
{
    if (!mux || !*mux) return 0;

    mux_handle_t *m = *mux;
    if (m->is_running) mux_stop(m);
    if (m->grp >= 0)   AW_MPI_MUX_DestroyGrp(m->grp);

    alogd("mux_deinit: grp=%d OK", m->grp);
    free(m);
    *mux = NULL;
    return 0;
}

int mux_gen_filename(mux_handle_t *mux, char *out_path, int max_len)
{
    static int file_cnt = 0;
    file_cnt++;

    snprintf(out_path, max_len, "%s/%s_%d.mp4",
             mux->cfg.output_dir,
             mux->cfg.output_prefix,
             file_cnt);
    return 0;
}
