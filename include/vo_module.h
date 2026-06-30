/**
 * @file    vo_module.h
 * @brief   VO（Video Output）LCD 实时预览模块
 *
 * 通过 SYS_Bind 直接从 VI 拿帧显示到屏幕，零拷贝，不经过 CPU。
 * 双缓冲（DispBufNum=2）防止画面撕裂。
 */

#ifndef VO_MODULE_H
#define VO_MODULE_H
#include "mpp_common.h"

/* ============ VO 配置 ============ */
typedef struct {
    int             layer;           /* 图层号，通常为 0 */
    int             chn_id;          /* 通道号 */
    int             disp_width;      /* 显示宽度 */
    int             disp_height;     /* 显示高度 */
} vo_config_t;

/* ============ VO 句柄 ============ */
typedef struct {
    vo_config_t     cfg;
    VO_LAYER        layer;
    VO_CHN          chn;
    bool            is_running;
} vo_handle_t;

int  vo_init(vo_handle_t **vo, const vo_config_t *cfg);
int  vo_start(vo_handle_t *vo);
int  vo_stop(vo_handle_t *vo);
int  vo_deinit(vo_handle_t **vo);

/** @brief 获取通道标识（给 SYS_Bind 用） */
static inline MPP_CHN_S vo_get_chn(const vo_handle_t *vo) {
    MPP_CHN_S c = { MOD_ID_VOU, vo->layer, vo->chn };
    return c;
}

/** @brief 手动送帧到 VO（G2D 旋转后使用） */
int vo_send_frame(vo_handle_t *vo, VIDEO_FRAME_INFO_S *frame, int timeout_ms);

#endif /* VO_MODULE_H */
