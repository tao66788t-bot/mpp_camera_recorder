/**
 * @file    g2d_rotate.h
 * @brief   G2D 硬件旋转模块：1920×1088 → 90°旋转 → 1088×1920
 */
#ifndef G2D_ROTATE_H
#define G2D_ROTATE_H
#include "mpp_common.h"

typedef struct {
    int              g2d_fd;          /* /dev/g2d fd */
    VIDEO_FRAME_INFO_S dst_frame;      /* 旋转后的输出帧(DMA buffer) */
    volatile bool    running;
    pthread_t        thread;
} g2d_rotate_t;

/**
 * @brief 初始化 G2D 旋转
 * @param src_w, src_h  源宽高 (1920, 1088)
 * @param dst_w, dst_h  目标宽高 (1088, 1920) -- 旋转90°
 * @param pix_fmt       像素格式
 * @return 0 成功
 */
int g2d_rotate_init(g2d_rotate_t **gr, int src_w, int src_h, int dst_w, int dst_h, int pix_fmt);

/**
 * @brief 旋转一帧，返回旋转后的 VIDEO_FRAME_INFO_S
 */
int g2d_rotate_frame(g2d_rotate_t *gr, VIDEO_FRAME_INFO_S *src, VIDEO_FRAME_INFO_S **dst);

void g2d_rotate_deinit(g2d_rotate_t **gr);

#endif
