#ifndef NPU_DET_RES_H
#define NPU_DET_RES_H

#define MAX_OBJECT_DET_NUM 20

typedef struct {
    int label;
    float prob;
    int left_up_x;
    int left_up_y;
    int right_down_x;
    int right_down_y;
    int width;
    int height;
    int landmark_x[5];
    int landmark_y[5];
} object_res_t;

typedef struct {
    int num;
    object_res_t objs[MAX_OBJECT_DET_NUM];
} detect_res_t;

#endif
