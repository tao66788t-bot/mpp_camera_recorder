#include "npu_module.h"
#include "npu_det_res.h"
#include "media/mm_comm_region.h"
#include "mpi_region.h"
#include "utils/VIDEO_FRAME_INFO_S.h"
#include "awnn.h"

#define NPU_MAX_BOXES    MAX_OBJECT_DET_NUM
#define NPU_LOG_INTERVAL 30

typedef struct {
    int x;
    int y;
    int width;
    int height;
} npu_rect_t;

struct npu_handle {
    npu_config_t cfg;
    vi_handle_t *vi;
    pthread_t thread;
    bool thread_started;
    bool running;
    bool model_ready;
    int last_box_count;
    unsigned int frame_counter;
    unsigned int input_size;
    unsigned char *yuv_buffer;
    detect_res_t result;
    Awnn_Context_t *awnn_ctx;
    bool preview_region_live[NPU_MAX_BOXES];
    bool encoder_region_live[NPU_MAX_BOXES];
};

static int npu_even_floor(int value)
{
    if (value < 0) {
        return 0;
    }
    return value & ~1;
}

static int npu_even_ceil(int value)
{
    if (value < 0) {
        return 0;
    }
    return (value + 1) & ~1;
}

static int npu_clamp(int value, int low, int high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void npu_normalize_result(detect_res_t *result)
{
    int i;

    if (!result) {
        return;
    }

    if (result->num < 0) {
        result->num = 0;
    } else if (result->num > MAX_OBJECT_DET_NUM) {
        result->num = MAX_OBJECT_DET_NUM;
    }

    for (i = 0; i < result->num; ++i) {
        result->objs[i].width = result->objs[i].right_down_x - result->objs[i].left_up_x;
        result->objs[i].height = result->objs[i].right_down_y - result->objs[i].left_up_y;
    }
}

static void npu_log_result(const struct npu_handle *npu)
{
    int i;

    if (!npu) {
        return;
    }

    if ((npu->frame_counter % NPU_LOG_INTERVAL) != 0 && npu->result.num == npu->last_box_count) {
        return;
    }

    printf("[NPU] frame=%u detect_num=%d\n", npu->frame_counter, npu->result.num);
    for (i = 0; i < npu->result.num && i < MAX_OBJECT_DET_NUM; ++i) {
        const object_res_t *obj = &npu->result.objs[i];
        printf("[NPU] obj[%d] label=%d prob=%.3f tl=(%d,%d) rb=(%d,%d) wh=%dx%d\n",
               i,
               obj->label,
               obj->prob,
               obj->left_up_x,
               obj->left_up_y,
               obj->right_down_x,
               obj->right_down_y,
               obj->width,
               obj->height);
    }
    fflush(stdout);
}

static int npu_copy_frame_to_buffer(struct npu_handle *npu, VIDEO_FRAME_INFO_S *frame)
{
    VideoFrameBufferSizeInfo size_info;
    unsigned char *dst;

    if (!npu || !frame || !npu->yuv_buffer) {
        return -1;
    }

    memset(&size_info, 0, sizeof(size_info));
    getVideoFrameBufferSizeInfo(frame, &size_info);

    dst = npu->yuv_buffer;
    if (frame->VFrame.mpVirAddr[0] && size_info.mYSize > 0) {
        memcpy(dst, frame->VFrame.mpVirAddr[0], size_info.mYSize);
        dst += size_info.mYSize;
    } else {
        return -1;
    }

    if (frame->VFrame.mpVirAddr[1] && size_info.mUSize > 0) {
        memcpy(dst, frame->VFrame.mpVirAddr[1], size_info.mUSize);
    } else {
        return -1;
    }

    return 0;
}

static bool npu_scale_box(
    const struct npu_handle *npu,
    const object_res_t *src,
    int dst_w,
    int dst_h,
    npu_rect_t *dst)
{
    int left;
    int top;
    int right;
    int bottom;
    int src_w;
    int src_h;

    if (!npu || !src || !dst || dst_w < 2 || dst_h < 2) {
        return false;
    }

    src_w = npu->cfg.model_input_width;
    src_h = npu->cfg.model_input_height;
    if (src_w <= 0 || src_h <= 0) {
        return false;
    }

    left = (src->left_up_x * dst_w) / src_w;
    top = (src->left_up_y * dst_h) / src_h;
    right = (src->right_down_x * dst_w) / src_w;
    bottom = (src->right_down_y * dst_h) / src_h;

    left = npu_even_floor(npu_clamp(left, 0, dst_w - 2));
    top = npu_even_floor(npu_clamp(top, 0, dst_h - 2));
    right = npu_even_ceil(npu_clamp(right, left + 2, dst_w));
    bottom = npu_even_ceil(npu_clamp(bottom, top + 2, dst_h));

    if (right <= left || bottom <= top) {
        return false;
    }

    dst->x = left;
    dst->y = top;
    dst->width = right - left;
    dst->height = bottom - top;
    return true;
}

static void npu_destroy_region(int region_id, bool *live_flag)
{
    if (!live_flag || !*live_flag) {
        return;
    }

    if (AW_MPI_RGN_Destroy(region_id) != SUCCESS) {
        alogw("npu: destroy region %d failed", region_id);
    }
    *live_flag = false;
}

static void npu_clear_regions(struct npu_handle *npu)
{
    int i;

    if (!npu) {
        return;
    }

    for (i = 0; i < NPU_MAX_BOXES; ++i) {
        npu_destroy_region(npu->cfg.preview_region_base + i, &npu->preview_region_live[i]);
        npu_destroy_region(npu->cfg.encoder_region_base + i, &npu->encoder_region_live[i]);
    }
    npu->last_box_count = 0;
}

static void npu_attach_rect_to_channel(
    int region_id,
    const MPP_CHN_S *channel,
    const npu_rect_t *rect,
    bool *live_flag)
{
    RGN_ATTR_S attr;
    RGN_CHN_ATTR_S chn_attr;
    int ret;

    if (!channel || !rect || !live_flag) {
        return;
    }

    memset(&attr, 0, sizeof(attr));
    memset(&chn_attr, 0, sizeof(chn_attr));

    attr.enType = ORL_RGN;
    ret = AW_MPI_RGN_Create(region_id, &attr);
    if (ret != SUCCESS) {
        alogw("npu: create region %d failed ret=0x%x", region_id, ret);
        return;
    }

    chn_attr.bShow = TRUE;
    chn_attr.enType = ORL_RGN;
    chn_attr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
    chn_attr.unChnAttr.stOrlChn.mColor = 0x00FF00;
    chn_attr.unChnAttr.stOrlChn.mThick = 2;
    chn_attr.unChnAttr.stOrlChn.mLayer = region_id;
    chn_attr.unChnAttr.stOrlChn.stRect.X = rect->x;
    chn_attr.unChnAttr.stOrlChn.stRect.Y = rect->y;
    chn_attr.unChnAttr.stOrlChn.stRect.Width = rect->width;
    chn_attr.unChnAttr.stOrlChn.stRect.Height = rect->height;

    ret = AW_MPI_RGN_AttachToChn(region_id, (MPP_CHN_S *)channel, &chn_attr);
    if (ret != SUCCESS) {
        alogw("npu: attach region %d failed ret=0x%x", region_id, ret);
        AW_MPI_RGN_Destroy(region_id);
        return;
    }

    *live_flag = true;
}

static void npu_draw_result(struct npu_handle *npu)
{
    MPP_CHN_S preview_chn;
    MPP_CHN_S encoder_chn;
    int i;
    int count;

    if (!npu) {
        return;
    }

    count = npu->result.num;
    if (count > npu->cfg.max_boxes) {
        count = npu->cfg.max_boxes;
    }

    npu_clear_regions(npu);
    if (count <= 0) {
        return;
    }

    preview_chn.mModId = MOD_ID_VIU;
    preview_chn.mDevId = npu->cfg.preview_vi_dev;
    preview_chn.mChnId = npu->cfg.preview_vi_chn;

    encoder_chn.mModId = MOD_ID_VIU;
    encoder_chn.mDevId = npu->cfg.encoder_vi_dev;
    encoder_chn.mChnId = npu->cfg.encoder_vi_chn;

    for (i = 0; i < count; ++i) {
        npu_rect_t preview_rect;
        npu_rect_t encoder_rect;

        if (npu_scale_box(npu,
                          &npu->result.objs[i],
                          npu->cfg.preview_width,
                          npu->cfg.preview_height,
                          &preview_rect)) {
            npu_attach_rect_to_channel(
                npu->cfg.preview_region_base + i,
                &preview_chn,
                &preview_rect,
                &npu->preview_region_live[i]);
        }

        if (npu_scale_box(npu,
                          &npu->result.objs[i],
                          npu->cfg.encoder_width,
                          npu->cfg.encoder_height,
                          &encoder_rect)) {
            npu_attach_rect_to_channel(
                npu->cfg.encoder_region_base + i,
                &encoder_chn,
                &encoder_rect,
                &npu->encoder_region_live[i]);
        }
    }

    npu->last_box_count = count;
}

static int npu_run_awnn_detect(struct npu_handle *npu)
{
    unsigned char *input_buffers[2];
    Awnn_Post_t post;
    Awnn_Result_t result;
    int i;

    if (!npu || !npu->awnn_ctx || !npu->yuv_buffer) {
        return -1;
    }

    memset(&post, 0, sizeof(post));
    memset(&result, 0, sizeof(result));

    input_buffers[0] = npu->yuv_buffer;
    input_buffers[1] = npu->yuv_buffer + (npu->cfg.model_input_width * npu->cfg.model_input_height);

    awnn_set_input_buffers(npu->awnn_ctx, input_buffers);
    awnn_run(npu->awnn_ctx);

    post.type = AWNN_DET_POST_FACE_1;
    post.width = npu->cfg.model_input_width;
    post.height = npu->cfg.model_input_height;
    post.thresh = npu->cfg.score_thresh;
    awnn_det_post(npu->awnn_ctx, &post, &result);

    memset(&npu->result, 0, sizeof(npu->result));
    npu->result.num = result.valid_cnt;
    if (npu->result.num > MAX_OBJECT_DET_NUM) {
        npu->result.num = MAX_OBJECT_DET_NUM;
    }

    for (i = 0; i < npu->result.num; ++i) {
        object_res_t *obj = &npu->result.objs[i];
        BBoxRect_t *box = &result.boxes[i];
        int k;

        obj->label = box->label;
        obj->prob = box->score;
        obj->left_up_x = box->xmin;
        obj->left_up_y = box->ymin;
        obj->right_down_x = box->xmax;
        obj->right_down_y = box->ymax;
        for (k = 0; k < 5; ++k) {
            obj->landmark_x[k] = box->landmark_x[k];
            obj->landmark_y[k] = box->landmark_y[k];
        }
    }

    npu_normalize_result(&npu->result);
    return 0;
}

static void *npu_thread_main(void *arg)
{
    struct npu_handle *npu = (struct npu_handle *)arg;

    while (npu && npu->running) {
        VIDEO_FRAME_INFO_S frame;
        int ret;
        memset(&frame, 0, sizeof(frame));
        ret = AW_MPI_VI_GetFrame(npu->vi->dev, npu->vi->chn, &frame, 500);
        if (ret != SUCCESS) {
            continue;
        }

        if ((npu->frame_counter % (unsigned int)npu->cfg.detect_interval) == 0) {
            if (npu_copy_frame_to_buffer(npu, &frame) == 0) {
                if (npu_run_awnn_detect(npu) == 0) {
                    npu_log_result(npu);
                    npu_draw_result(npu);
                }
            }
        }

        AW_MPI_VI_ReleaseFrame(npu->vi->dev, npu->vi->chn, &frame);
        ++npu->frame_counter;
    }

    return NULL;
}

int npu_init(npu_handle_t **npu, const npu_config_t *cfg)
{
    struct npu_handle *ctx;
    int ret;

    MPP_CHECK_NULL(npu, "npu");
    MPP_CHECK_NULL(cfg, "cfg");

    ctx = (struct npu_handle *)calloc(1, sizeof(*ctx));
    MPP_CHECK_NULL(ctx, "npu alloc");
    ctx->cfg = *cfg;
    if (ctx->cfg.detect_interval <= 0) {
        ctx->cfg.detect_interval = 1;
    }
    if (ctx->cfg.max_boxes <= 0 || ctx->cfg.max_boxes > NPU_MAX_BOXES) {
        ctx->cfg.max_boxes = NPU_MAX_BOXES;
    }
    if (ctx->cfg.model_input_width <= 0) {
        ctx->cfg.model_input_width = 480;
    }
    if (ctx->cfg.model_input_height <= 0) {
        ctx->cfg.model_input_height = 288;
    }
    if (ctx->cfg.score_thresh <= 0.0f) {
        ctx->cfg.score_thresh = 0.60f;
    }

    ctx->input_size = (unsigned int)(ctx->cfg.model_input_width * ctx->cfg.model_input_height * 3 / 2);
    ctx->yuv_buffer = (unsigned char *)calloc(1, ctx->input_size);
    if (!ctx->yuv_buffer) {
        free(ctx);
        return -1;
    }

    ret = vi_init(&ctx->vi, &ctx->cfg.vi_cfg);
    if (ret != 0) {
        free(ctx->yuv_buffer);
        free(ctx);
        return ret;
    }

    awnn_init(3670016);
    ctx->awnn_ctx = awnn_create(ctx->cfg.model_path);
    if (!ctx->awnn_ctx) {
        vi_deinit(&ctx->vi);
        free(ctx->yuv_buffer);
        free(ctx);
        return -1;
    }
    ctx->model_ready = true;

    *npu = ctx;
    alogd("npu_init: model=%s dev=%d chn=%d",
          ctx->cfg.model_path, ctx->vi->dev, ctx->vi->chn);
    return 0;
}

int npu_start(npu_handle_t *npu)
{
    struct npu_handle *ctx = (struct npu_handle *)npu;
    int ret;

    MPP_CHECK_NULL(ctx, "npu");
    if (ctx->thread_started) {
        return 0;
    }

    ret = vi_start(ctx->vi);
    if (ret != 0) {
        return ret;
    }

    ctx->running = true;
    ret = pthread_create(&ctx->thread, NULL, npu_thread_main, ctx);
    if (ret != 0) {
        ctx->running = false;
        vi_stop(ctx->vi);
        return -1;
    }

    ctx->thread_started = true;
    return 0;
}

int npu_stop(npu_handle_t *npu)
{
    struct npu_handle *ctx = (struct npu_handle *)npu;

    if (!ctx) {
        return 0;
    }

    if (ctx->thread_started) {
        ctx->running = false;
        pthread_join(ctx->thread, NULL);
        ctx->thread_started = false;
    }

    npu_clear_regions(ctx);
    return vi_stop(ctx->vi);
}

int npu_deinit(npu_handle_t **npu)
{
    struct npu_handle *ctx;

    if (!npu || !*npu) {
        return 0;
    }

    ctx = (struct npu_handle *)(*npu);
    npu_stop(ctx);

    if (ctx->model_ready) {
        if (ctx->awnn_ctx) {
            awnn_destroy(ctx->awnn_ctx);
            ctx->awnn_ctx = NULL;
        }
        awnn_uninit();
    }

    vi_deinit(&ctx->vi);
    free(ctx->yuv_buffer);
    free(ctx);
    *npu = NULL;
    return 0;
}
