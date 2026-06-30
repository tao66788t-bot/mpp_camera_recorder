/**
 * @file    vo_module.c
 * @brief   VO LCD 实时预览实现（不改显示参数，用内核预设配置）
 */
#include "vo_module.h"

int vo_init(vo_handle_t **vo, const vo_config_t *cfg)
{
    MPP_CHECK_NULL(vo, "vo");
    MPP_CHECK_NULL(cfg, "cfg");

    vo_handle_t *v = (vo_handle_t *)calloc(1, sizeof(vo_handle_t));
    MPP_CHECK_NULL(v, "vo_handle_t alloc");
    v->cfg   = *cfg;
    v->layer = cfg->layer;
    v->chn   = cfg->chn_id;

    int ret;

    /* ① 使能 VO 设备 */
    ret = AW_MPI_VO_Enable(0);
    MPP_CHECK_RET(ret, "AW_MPI_VO_Enable");
    printf("[VO] AW_MPI_VO_Enable OK\n");

    /* ② 关闭 UI 层（sample_virvi2vo 的做法，防止 UI 遮住视频）*/
    {
        VO_LAYER ui_layer = 0x00020000; /* HLAY(2,0) = 硬件层 2 */
        AW_MPI_VO_AddOutsideVideoLayer(ui_layer);
        AW_MPI_VO_CloseVideoLayer(ui_layer);
        printf("[VO] Add+Close UI layer OK\n");
    }

    /* ③ 设置显示类型为 LCD */
    {
        VO_PUB_ATTR_S pub_attr;
        memset(&pub_attr, 0, sizeof(pub_attr));
        AW_MPI_VO_GetPubAttr(0, &pub_attr);
        pub_attr.enIntfType = VO_INTF_LCD;
        pub_attr.enIntfSync = VO_OUTPUT_NTSC;
        ret = AW_MPI_VO_SetPubAttr(0, &pub_attr);
        MPP_CHECK_RET(ret, "AW_MPI_VO_SetPubAttr");
        printf("[VO] SetPubAttr LCD OK\n");
    }

    /* ③ 使能图层 + 设置显示区域 */
    ret = AW_MPI_VO_EnableVideoLayer(v->layer);
    MPP_CHECK_RET(ret, "AW_MPI_VO_EnableVideoLayer");
    printf("[VO] EnableVideoLayer OK\n");

    {
        VO_VIDEO_LAYER_ATTR_S layer_attr;
        memset(&layer_attr, 0, sizeof(layer_attr));
        AW_MPI_VO_GetVideoLayerAttr(v->layer, &layer_attr);
        layer_attr.stDispRect.X      = 0;
        layer_attr.stDispRect.Y      = 0;
        layer_attr.stDispRect.Width  = cfg->disp_width;
        layer_attr.stDispRect.Height = cfg->disp_height;
        ret = AW_MPI_VO_SetVideoLayerAttr(v->layer, &layer_attr);
        MPP_CHECK_RET(ret, "AW_MPI_VO_SetVideoLayerAttr");
        printf("[VO] SetVideoLayerAttr %dx%d OK\n", cfg->disp_width, cfg->disp_height);
    }

    /* ④ 设置图层优先级（z-order，越大越靠前）*/
    AW_MPI_VO_SetVideoLayerPriority(v->layer, 15);  /* max priority, cover UI */
    printf("[VO] SetVideoLayerPriority=10 OK\n");

    /* ⑤ 创建通道 + 双缓冲 */
    ret = AW_MPI_VO_CreateChn(v->layer, v->chn);
    MPP_CHECK_RET(ret, "AW_MPI_VO_CreateChn");
    printf("[VO] CreateChn OK\n");

    ret = AW_MPI_VO_SetChnDispBufNum(v->layer, v->chn, 2);
    MPP_CHECK_RET(ret, "AW_MPI_VO_SetChnDispBufNum");
    printf("[VO] vo_init OK: layer=%d chn=%d\n", v->layer, v->chn);

    *vo = v;
    return 0;
}

int vo_start(vo_handle_t *vo)
{
    MPP_CHECK_NULL(vo, "vo");
    if (vo->is_running) return 0;
    int ret = AW_MPI_VO_StartChn(vo->layer, vo->chn);
    MPP_CHECK_RET(ret, "AW_MPI_VO_StartChn");
    vo->is_running = true;
    printf("[VO] start OK\n");
    return 0;
}

int vo_send_frame(vo_handle_t *vo, VIDEO_FRAME_INFO_S *frame, int timeout_ms)
{
    if (!vo || !vo->is_running) return -1;
    return AW_MPI_VO_SendFrame(vo->layer, vo->chn, frame, timeout_ms);
}

int vo_stop(vo_handle_t *vo)
{
    MPP_CHECK_NULL(vo, "vo");
    if (!vo->is_running) return 0;
    AW_MPI_VO_StopChn(vo->layer, vo->chn);
    vo->is_running = false;
    return 0;
}

int vo_deinit(vo_handle_t **vo)
{
    if (!vo || !*vo) return 0;
    vo_handle_t *v = *vo;
    if (v->is_running) vo_stop(v);
    AW_MPI_VO_DestroyChn(v->layer, v->chn);
    AW_MPI_VO_DisableVideoLayer(v->layer);
    free(v);
    *vo = NULL;
    return 0;
}
