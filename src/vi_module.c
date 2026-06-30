/**
 * @file    vi_module.c
 * @brief   VI 摄像头采集模块实现
 *
 * 初始化流程：
 *   1. AW_MPI_VI_CreateVipp()      创建 VI 设备
 *   2. AW_MPI_VI_SetVippAttr()     设置设备属性（分辨率/格式/帧率）
 *   3. AW_MPI_ISP_Run()            启动 ISP
 *   4. AW_MPI_VI_EnableVipp()      使能设备
 *   5. AW_MPI_VI_CreateVirChn()    创建虚拟通道
 *   6. AW_MPI_VI_EnableVirChn()    使能通道（在 vi_start 中）
 */

#include "vi_module.h"
#include <sys/stat.h>

#define ISP_CTX_EXPECTED_SIZE_BYTES 34300
#define ISP_EXT_BIN_DIR "/mnt/UDISK/gc2053"

static bool vi_has_valid_isp_ctx(const vi_config_t *cfg)
{
    struct stat st;
    char path[128];

    if (!cfg) {
        return false;
    }

    snprintf(path, sizeof(path),
             "/mnt/extsd/isp%d_1920_1088_%d_0_ctx_saved.bin",
             cfg->isp_dev, cfg->fps);

    if (stat(path, &st) != 0) {
        alogw("vi: isp ctx missing: %s", path);
        return false;
    }

    if (st.st_size != ISP_CTX_EXPECTED_SIZE_BYTES) {
        alogw("vi: isp ctx invalid size=%ld path=%s",
              (long)st.st_size, path);
        return false;
    }

    alogd("vi: isp ctx valid: %s", path);
    return true;
}

static bool vi_has_external_isp_bin_set(const char *dir)
{
    static const char *const files[] = {
        "isp_test_settings.bin",
        "isp_3a_settings.bin",
        "isp_iso_settings.bin",
        "isp_tuning_settings.bin",
    };
    struct stat st;
    char path[160];
    size_t i;

    if (!dir || !dir[0]) {
        return false;
    }

    for (i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        if (stat(path, &st) != 0 || st.st_size <= 0) {
            alogw("vi: external isp bin missing: %s", path);
            return false;
        }
    }

    alogd("vi: external isp bin set ready: %s", dir);
    return true;
}

static int vi_try_load_external_isp_bin(vi_handle_t *v)
{
    int ret;

    if (!v) {
        return -1;
    }

    if (!vi_has_external_isp_bin_set(ISP_EXT_BIN_DIR)) {
        return -1;
    }

    /* 0x00 = color mode, matches the normal GC2053 day profile. */
    ret = AW_MPI_ISP_ReadIspCfgBin(v->isp_dev, 0, ISP_EXT_BIN_DIR);
    if (ret == 0) {
        alogw("vi: loaded external gc2053 isp bin from %s", ISP_EXT_BIN_DIR);
    } else {
        aloge("vi: failed to load external gc2053 isp bin from %s ret=%d",
              ISP_EXT_BIN_DIR, ret);
    }

    return ret;
}

int vi_init(vi_handle_t **vi, const vi_config_t *cfg)
{
    MPP_CHECK_NULL(vi, "vi");
    MPP_CHECK_NULL(cfg, "cfg");

    vi_handle_t *v = (vi_handle_t *)calloc(1, sizeof(vi_handle_t));
    MPP_CHECK_NULL(v, "vi_handle_t alloc");
    v->cfg = *cfg;

    v->dev     = cfg->dev_id;
    v->isp_dev = cfg->isp_dev;
    v->chn     = cfg->chn_id;

    /* ---- 1. 创建 VI 设备 ---- */
    int ret = AW_MPI_VI_CreateVipp(v->dev);
    MPP_CHECK_RET(ret, "AW_MPI_VI_CreateVipp");

    /* ---- 2. 配置 VI 属性 ---- */
    memset(&v->attr, 0, sizeof(VI_ATTR_S));

    if (cfg->online_en) {
        v->attr.mOnlineEnable      = 1;
        v->attr.mOnlineShareBufNum = cfg->online_share_buf_num;
    }
    v->attr.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v->attr.memtype            = V4L2_MEMORY_MMAP;
    v->attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(cfg->pix_fmt);
    v->attr.format.field       = V4L2_FIELD_NONE;
    v->attr.format.colorspace  = cfg->color_space;
    v->attr.format.width       = cfg->width;
    v->attr.format.height      = cfg->height;
    v->attr.nbufs              = cfg->buf_num;
    v->attr.nplanes            = (cfg->nplanes > 0) ? cfg->nplanes : 2;
    v->attr.wdr_mode           = cfg->wdr_en;
    v->attr.fps                = cfg->fps;
    v->attr.drop_frame_num     = cfg->drop_frame_num;
    v->attr.mbEncppEnable      = cfg->encpp_enable;
    v->attr.use_current_win    = cfg->use_current_win;

    ret = AW_MPI_VI_SetVippAttr(v->dev, &v->attr);
    MPP_CHECK_RET(ret, "AW_MPI_VI_SetVippAttr");

    /* ---- 3. 启动 ISP ---- */
#if ISP_RUN
    ret = AW_MPI_ISP_Run(v->isp_dev);
    MPP_CHECK_RET(ret, "AW_MPI_ISP_Run");
#endif

    /* ---- 4. 使能 VI 设备 ---- */
    ret = AW_MPI_VI_EnableVipp(v->dev);
    MPP_CHECK_RET(ret, "AW_MPI_VI_EnableVipp");

#if ISP_RUN
    {
        bool has_valid_ctx = vi_has_valid_isp_ctx(cfg);
        bool loaded_external_bin = false;

        if (!has_valid_ctx) {
            loaded_external_bin = (vi_try_load_external_isp_bin(v) == 0);
        }

        if (!has_valid_ctx && !loaded_external_bin) {
            int ae_mode = -1;
            int ae_exposure = -1;
            int ae_gain = -1;

            /* Last-resort rescue when both ctx and external bin are unavailable. */
            AW_MPI_ISP_AE_SetMode(v->isp_dev, 1);
            AW_MPI_ISP_AE_SetExposure(v->isp_dev, 2048);
            AW_MPI_ISP_AE_SetGain(v->isp_dev, 256);
            AW_MPI_ISP_AE_GetMode(v->isp_dev, &ae_mode);
            AW_MPI_ISP_AE_GetExposure(v->isp_dev, &ae_exposure);
            AW_MPI_ISP_AE_GetGain(v->isp_dev, &ae_gain);
            alogw("vi: rescue isp ae mode=%d exposure=%d gain=%d",
                  ae_mode, ae_exposure, ae_gain);
        }
    }
#endif

    /* ---- 5. 饱和度调整（可选） ---- */
    if (cfg->saturation != 0) {
        int sat_val = 0;
        AW_MPI_ISP_GetSaturation(v->isp_dev, &sat_val);
        sat_val += cfg->saturation;
        AW_MPI_ISP_SetSaturation(v->isp_dev, sat_val);
        alogd("vi: saturation adjusted to %d", sat_val);
    }

    /* ---- 6. 创建虚拟通道 ---- */
    ViVirChnAttrS vir_chn_attr;
    memset(&vir_chn_attr, 0, sizeof(ViVirChnAttrS));
    vir_chn_attr.mbRecvInIdleState = TRUE;

    ret = AW_MPI_VI_CreateVirChn(v->dev, v->chn, &vir_chn_attr);
    MPP_CHECK_RET(ret, "AW_MPI_VI_CreateVirChn");

    alogd("vi_init: dev=%d chn=%d %dx%d@%dfps fmt=0x%x OK",
          v->dev, v->chn, cfg->width, cfg->height, cfg->fps, cfg->pix_fmt);

    *vi = v;
    return 0;
}

int vi_start(vi_handle_t *vi)
{
    MPP_CHECK_NULL(vi, "vi");

    if (vi->is_running) {
        alogw("vi_start: already running");
        return 0;
    }

    int ret = AW_MPI_VI_EnableVirChn(vi->dev, vi->chn);
    MPP_CHECK_RET(ret, "AW_MPI_VI_EnableVirChn");

    vi->is_running = true;
    alogd("vi_start: dev=%d chn=%d OK", vi->dev, vi->chn);
    return 0;
}

int vi_stop(vi_handle_t *vi)
{
    MPP_CHECK_NULL(vi, "vi");
    if (!vi->is_running) return 0;

    AW_MPI_VI_DisableVirChn(vi->dev, vi->chn);
    vi->is_running = false;
    alogd("vi_stop: dev=%d chn=%d OK", vi->dev, vi->chn);
    return 0;
}

int vi_deinit(vi_handle_t **vi)
{
    if (!vi || !*vi) return 0;

    vi_handle_t *v = *vi;

    /* 先停止 */
    if (v->is_running) {
        vi_stop(v);
    }

    /* 销毁通道 */
    if (v->chn >= 0) {
        AW_MPI_VI_DestroyVirChn(v->dev, v->chn);
    }

    /* 关闭设备 */
    if (v->dev >= 0) {
        AW_MPI_VI_DisableVipp(v->dev);
#if ISP_RUN
        /* 保存 ISP 校准 context 到 /mnt/extsd/isp0_*.bin */
        AW_MPI_ISP_SetSaveCTX(v->isp_dev);
        AW_MPI_ISP_Stop(v->isp_dev);
#endif
        AW_MPI_VI_DestroyVipp(v->dev);
    }

    alogd("vi_deinit: dev=%d OK", v->dev);
    free(v);
    *vi = NULL;
    return 0;
}
