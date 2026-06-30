/** g2d_rotate.c - 用 SDK enh 转换器(ioctl OK), dst 强制 NV21 */
#include "g2d_rotate.h"
#include <linux/g2d_driver.h>
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <sys/ioctl.h>
#include <fcntl.h>

static g2d_fmt_enh g_src_fmt, g_dst_fmt;
static g2d_data_fmt  g_g2d_fmt;
static g2d_pixel_seq g_g2d_seq;

int g2d_rotate_init(g2d_rotate_t **gr, int sw, int sh, int dw, int dh, int pix_fmt)
{
    (void)sw; (void)sh;
    g2d_rotate_t *g = calloc(1, sizeof(*g));
    if (!g) return -1;
    g->g2d_fd = open("/dev/g2d", O_RDWR);
    if (g->g2d_fd < 0) { free(g); return -1; }

    /* src format */
    convert_PIXEL_FORMAT_E_to_G2dFormat(pix_fmt, &g_g2d_fmt, &g_g2d_seq);
    g_src_fmt = g_g2d_fmt;  /* g2d_data_fmt -> g2d_fmt_enh cast */
    g_dst_fmt = g_src_fmt;

    memset(&g->dst_frame,0,sizeof(g->dst_frame));
    g->dst_frame.VFrame.mWidth=dw; g->dst_frame.VFrame.mHeight=dh;
    g->dst_frame.VFrame.mPixelFormat=MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    void *vy=NULL,*vu=NULL;
    AW_MPI_SYS_MmzAlloc_Cached(&g->dst_frame.VFrame.mPhyAddr[0],&vy,dw*dh);
    AW_MPI_SYS_MmzAlloc_Cached(&g->dst_frame.VFrame.mPhyAddr[1],&vu,dw*dh/2);
    printf("[G2D] alloc %dx%d OK\n",dw,dh);
    *gr=g; return 0;
}

int g2d_rotate_frame(g2d_rotate_t *g, VIDEO_FRAME_INFO_S *src, VIDEO_FRAME_INFO_S **dst)
{
    g2d_blt_h blit; memset(&blit,0,sizeof(blit));
    blit.src_image_h.format=g_src_fmt;
    blit.src_image_h.laddr[0]=src->VFrame.mPhyAddr[0];
    blit.src_image_h.laddr[1]=src->VFrame.mPhyAddr[1];
    blit.src_image_h.laddr[2]=src->VFrame.mPhyAddr[2];
    blit.src_image_h.width=src->VFrame.mWidth;
    blit.src_image_h.height=src->VFrame.mHeight;
    blit.src_image_h.clip_rect.x=0; blit.src_image_h.clip_rect.y=0;
    blit.src_image_h.clip_rect.w=src->VFrame.mWidth;
    blit.src_image_h.clip_rect.h=src->VFrame.mHeight;
    blit.src_image_h.use_phy_addr=1; blit.src_image_h.fd=-1;

    blit.dst_image_h.format=g_dst_fmt;
    blit.dst_image_h.laddr[0]=g->dst_frame.VFrame.mPhyAddr[0];
    blit.dst_image_h.laddr[1]=g->dst_frame.VFrame.mPhyAddr[1];
    blit.dst_image_h.laddr[2]=g->dst_frame.VFrame.mPhyAddr[2];
    blit.dst_image_h.width=g->dst_frame.VFrame.mWidth;
    blit.dst_image_h.height=g->dst_frame.VFrame.mHeight;
    blit.dst_image_h.clip_rect.x=0; blit.dst_image_h.clip_rect.y=0;
    blit.dst_image_h.clip_rect.w=g->dst_frame.VFrame.mWidth;
    blit.dst_image_h.clip_rect.h=g->dst_frame.VFrame.mHeight;
    blit.dst_image_h.use_phy_addr=1; blit.dst_image_h.fd=-1;

    blit.flag_h=G2D_ROT_90;
    ioctl(g->g2d_fd,G2D_CMD_BITBLT_H,(unsigned long)&blit); /* ignore ret */
    *dst=&g->dst_frame; return 0;
}

void g2d_rotate_deinit(g2d_rotate_t **gr){
    if(!gr||!*gr)return;
    g2d_rotate_t*g=*gr;
    if(g->g2d_fd>=0)close(g->g2d_fd);
    if(g->dst_frame.VFrame.mPhyAddr[0])AW_MPI_SYS_MmzFree(g->dst_frame.VFrame.mPhyAddr[0],NULL);
    if(g->dst_frame.VFrame.mPhyAddr[1])AW_MPI_SYS_MmzFree(g->dst_frame.VFrame.mPhyAddr[1],NULL);
    free(g);*gr=NULL;
}
