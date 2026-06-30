/**
 * @file    mpp_common.h
 * @brief   全志 MPP 公共头文件 —— 所有模块统一引用此文件
 *
 * 包含：
 *   1. 所有 MPP 子系统的头文件（VI/VENC/MUX/SYS）
 *   2. 统一的错误处理和日志宏
 *   3. 公共数据结构
 */

#ifndef MPP_COMMON_H
#define MPP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

/* ============ 全志 MPP 子系统头文件 ============ */
#include "mm_comm_sys.h"
#include "mpi_sys.h"

#include "mm_comm_vi.h"
#include "mpi_vi.h"
#include "mpi_isp.h"

#include "vencoder.h"
#include "mpi_venc.h"

#include "mm_comm_mux.h"
#include "mpi_mux.h"

#include "mm_comm_video.h"
#include "mm_comm_vo.h"
#include "mpi_vo.h"

#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include <cdx_list.h>
#include <tmessage.h>
#include <tsemaphore.h>
#include <utils/plat_log.h>

/* ============ 日志级别定义（SDK 要求应用层定义） ============ */
#define _GLOG_DEBUG  0
#define _GLOG_INFO   1
#define _GLOG_WARN   2
#define _GLOG_ERROR  3

/* GLOG_PRINT + log_printf 空壳 —— 代替 liblog.a + glog + libstdc++ 的依赖链 */
#include <stdio.h>
static inline void GLOG_PRINT(int level, const char *fmt, ...) {
    (void)level; (void)fmt;
}
static inline void log_printf(const char *fmt, ...) {
    (void)fmt;
}

/* ============ 公共常量 ============ */
#define MAX_FILE_PATH_LEN       256
#define MAX_CAPTURE_NUM         4

/* ============ 统一错误处理宏 ============ */
#define MPP_CHECK_NULL(ptr, msg)                    \
    do {                                            \
        if (!(ptr)) {                               \
            aloge("[ERROR] %s is NULL", msg);       \
            return -1;                              \
        }                                           \
    } while (0)

#define MPP_CHECK_RET(ret, msg)                     \
    do {                                            \
        if ((ret) != SUCCESS) {                     \
            aloge("[ERROR] %s failed, ret=0x%x", msg, ret); \
            return ret;                             \
        }                                           \
    } while (0)

/* ============ 录制状态枚举 ============ */
typedef enum {
    REC_STATE_IDLE        = 0,  /* 未初始化 */
    REC_STATE_PREPARED    = 1,  /* 模块已创建，已绑定，待启动 */
    REC_STATE_RECORDING   = 2,  /* 正在录制 */
    REC_STATE_STOPPED     = 3,  /* 已停止 */
    REC_STATE_ERROR       = 4,  /* 错误状态 */
} recorder_state_e;

/* ============ 编码格式枚举 ============ */
typedef enum {
    ENC_TYPE_H264  = 0,
    ENC_TYPE_H265  = 1,
    ENC_TYPE_MJPEG = 2,
} encoder_type_e;

/* ============ 码率控制模式 ============ */
typedef enum {
    RC_MODE_CBR   = 0,  /* 固定码率 */
    RC_MODE_VBR   = 1,  /* 可变码率 */
    RC_MODE_FIXQP = 2,  /* 固定QP */
    RC_MODE_ABR   = 3,  /* 平均码率 */
} rc_mode_e;

/* ============ 像素格式字符串→内部格式映射 ============ */
typedef struct {
    const char  *name;
    int          fmt;
} pixfmt_map_t;

static const pixfmt_map_t g_pixfmt_map[] = {
    { "nv21",        MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 },
    { "nv12",        MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 },
    { "yv12",        MM_PIXEL_FORMAT_YVU_PLANAR_420     },
    { "yu12",        MM_PIXEL_FORMAT_YUV_PLANAR_420     },
    { "aw_lbc_2_5x", MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X   },
    { "aw_lbc_2_0x", MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X   },
    { "aw_lbc_1_5x", MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X   },
    { "aw_lbc_1_0x", MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X   },
    { NULL,          0                                   },
};

static inline int mpp_parse_pixfmt(const char *name)
{
    for (const pixfmt_map_t *p = g_pixfmt_map; p->name; p++) {
        if (strcmp(p->name, name) == 0) return p->fmt;
    }
    alogw("unknown pixfmt '%s', fallback to nv21", name);
    return MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
}

static inline const char *mpp_pixfmt_name(int fmt)
{
    for (const pixfmt_map_t *p = g_pixfmt_map; p->name; p++) {
        if (p->fmt == fmt) return p->name;
    }
    return "unknown";
}

/* ============ 编码格式字符串→内部格式映射 ============ */
static inline int mpp_parse_encoder_type(const char *name)
{
    if (!strcmp(name, "H.264") || !strcmp(name, "h264"))  return PT_H264;
    if (!strcmp(name, "H.265") || !strcmp(name, "h265"))  return PT_H265;
    if (!strcmp(name, "MJPEG") || !strcmp(name, "mjpeg")) return PT_MJPEG;
    alogw("unknown encoder '%s', fallback to H.264", name);
    return PT_H264;
}

/* ============ 时间工具 ============ */
static inline unsigned long long mpp_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

#endif /* MPP_COMMON_H */
