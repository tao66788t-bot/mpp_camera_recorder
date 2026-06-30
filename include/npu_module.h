#ifndef NPU_MODULE_H
#define NPU_MODULE_H

#include "mpp_common.h"
#include "npu_det_res.h"
#include "vi_module.h"

typedef struct {
    bool enable;
    int detect_interval;
    int max_boxes;
    int model_input_width;
    int model_input_height;
    float score_thresh;
    int preview_width;
    int preview_height;
    int encoder_width;
    int encoder_height;
    int preview_region_base;
    int encoder_region_base;
    VI_DEV preview_vi_dev;
    VI_CHN preview_vi_chn;
    VI_DEV encoder_vi_dev;
    VI_CHN encoder_vi_chn;
    char model_path[MAX_FILE_PATH_LEN];
    vi_config_t vi_cfg;
} npu_config_t;

typedef struct npu_handle npu_handle_t;

int npu_init(npu_handle_t **npu, const npu_config_t *cfg);
int npu_start(npu_handle_t *npu);
int npu_stop(npu_handle_t *npu);
int npu_deinit(npu_handle_t **npu);

#endif
