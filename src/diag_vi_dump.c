#include "diag_vi_dump.h"
#include "vi_module.h"

typedef struct {
    const char *tag;
    const char *path;
    int dev_id;
    int isp_dev;
    int chn_id;
    int width;
    int height;
    int fps;
    int pix_fmt;
    int color_space;
    int use_current_win;
} diag_vi_cfg_t;

static int dump_frame_to_file(const char *path, VIDEO_FRAME_INFO_S *frame)
{
    FILE *fp;
    size_t y_size;
    size_t uv_size;

    if (!path || !frame || !frame->VFrame.mpVirAddr[0] || !frame->VFrame.mpVirAddr[1]) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (!fp) {
        aloge("diag: open %s failed: %s", path, strerror(errno));
        return -1;
    }

    y_size = (size_t)frame->VFrame.mWidth * frame->VFrame.mHeight;
    uv_size = y_size / 2;
    fwrite(frame->VFrame.mpVirAddr[0], 1, y_size, fp);
    fwrite(frame->VFrame.mpVirAddr[1], 1, uv_size, fp);
    fclose(fp);

    alogd("diag: dumped %s %ux%u fmt=0x%x -> %s",
          path,
          frame->VFrame.mWidth,
          frame->VFrame.mHeight,
          frame->VFrame.mPixelFormat,
          path);
    return 0;
}

static int capture_one(const diag_vi_cfg_t *cfg)
{
    vi_handle_t *vi = NULL;
    vi_config_t vcfg;
    VIDEO_FRAME_INFO_S frame;
    int ret;
    int retry;

    memset(&vcfg, 0, sizeof(vcfg));
    vcfg.dev_id = cfg->dev_id;
    vcfg.isp_dev = cfg->isp_dev;
    vcfg.chn_id = cfg->chn_id;
    vcfg.width = cfg->width;
    vcfg.height = cfg->height;
    vcfg.fps = cfg->fps;
    vcfg.buf_num = 5;
    vcfg.pix_fmt = cfg->pix_fmt;
    vcfg.color_space = cfg->color_space;
    vcfg.nplanes = 2;
    vcfg.use_current_win = cfg->use_current_win;

    ret = vi_init(&vi, &vcfg);
    if (ret != 0) {
        aloge("diag: vi_init %s failed ret=%d", cfg->tag, ret);
        return ret;
    }

    ret = vi_start(vi);
    if (ret != 0) {
        aloge("diag: vi_start %s failed ret=%d", cfg->tag, ret);
        vi_deinit(&vi);
        return ret;
    }

    memset(&frame, 0, sizeof(frame));
    ret = -1;
    for (retry = 0; retry < 30; ++retry) {
        if (AW_MPI_VI_GetFrame(vi->dev, vi->chn, &frame, 500) == 0) {
            ret = dump_frame_to_file(cfg->path, &frame);
            AW_MPI_VI_ReleaseFrame(vi->dev, vi->chn, &frame);
            break;
        }
    }

    if (ret != 0) {
        aloge("diag: capture %s failed after retries", cfg->tag);
    }

    vi_stop(vi);
    vi_deinit(&vi);
    return ret;
}

int run_diag_vi_dump(void)
{
    static const diag_vi_cfg_t kCfgs[] = {
        {
            .tag = "vi0",
            .path = "/mnt/UDISK/diag_vi0.yuv",
            .dev_id = 0,
            .isp_dev = 0,
            .chn_id = 0,
            .width = 1920,
            .height = 1088,
            .fps = 20,
            .pix_fmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420,
            .color_space = V4L2_COLORSPACE_JPEG,
            .use_current_win = 0,
        },
        {
            .tag = "vi2",
            .path = "/mnt/UDISK/diag_vi2.yuv",
            .dev_id = 4,
            .isp_dev = 0,
            .chn_id = 0,
            .width = 480,
            .height = 800,
            .fps = 20,
            .pix_fmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
            .color_space = V4L2_COLORSPACE_JPEG,
            .use_current_win = 0,
        },
    };
    MPP_SYS_CONF_S sc;
    int ret;
    size_t i;

    memset(&sc, 0, sizeof(sc));
    sc.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&sc);
    ret = AW_MPI_SYS_Init();
    if (ret != 0) {
        aloge("diag: AW_MPI_SYS_Init failed ret=%d", ret);
        return ret;
    }

    ret = 0;
    for (i = 0; i < sizeof(kCfgs) / sizeof(kCfgs[0]); ++i) {
        if (capture_one(&kCfgs[i]) != 0) {
            ret = -1;
            break;
        }
    }

    AW_MPI_SYS_Exit();
    return ret;
}
