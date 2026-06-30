/**
 * @file    venc_module.c
 * @brief   VENC 硬件编码模块实现
 *
 * 初始化流程：
 *   1. 填充 VENC_CHN_ATTR_S 和 VENC_RC_PARAM_S
 *   2. AW_MPI_VENC_CreateChn()     创建编码通道
 *   3. AW_MPI_VENC_SetRcParam()    设置码率控制参数
 *   4. AW_MPI_VENC_SetFrameRate()  设置帧率
 *   5. AW_MPI_VENC_Set2DFilter/3DFilter 降噪
 *   6. 各种可选特性（旋转/镜像/裁剪/ROI/SmartP/SVC）
 */

#include "venc_module.h"

/* ---- 内部辅助：填充 H264 特有属性 ---- */
static void fill_h264_attr(VENC_CHN_ATTR_S *attr, const venc_config_t *cfg)
{
    attr->VeAttr.AttrH264e.bByFrame   = TRUE;
    attr->VeAttr.AttrH264e.BufSize    = cfg->vbv_buf_size;
    attr->VeAttr.AttrH264e.mThreshSize = cfg->vbv_thresh_size;
    attr->VeAttr.AttrH264e.PicWidth   = cfg->dst_width;
    attr->VeAttr.AttrH264e.PicHeight  = cfg->dst_height;
    attr->VeAttr.AttrH264e.mbPIntraEnable = TRUE;

    /* Profile 映射 */
    switch (cfg->enc_profile) {
        case 0:  attr->VeAttr.AttrH264e.Profile = VENC_H264ProfileBaseline; break;
        case 1:  attr->VeAttr.AttrH264e.Profile = VENC_H264ProfileMain; break;
        case 2:
        default: attr->VeAttr.AttrH264e.Profile = VENC_H264ProfileHigh; break;
    }
    attr->VeAttr.AttrH264e.mLevel = 0; /* 自动 */

    if (cfg->fast_enc) {
        attr->VeAttr.AttrH264e.FastEncFlag = TRUE;
    }
}

/* ---- 内部辅助：填充 H265 特有属性 ---- */
static void fill_h265_attr(VENC_CHN_ATTR_S *attr, const venc_config_t *cfg)
{
    attr->VeAttr.AttrH265e.mbByFrame   = TRUE;
    attr->VeAttr.AttrH265e.mBufSize    = cfg->vbv_buf_size;
    attr->VeAttr.AttrH265e.mThreshSize = cfg->vbv_thresh_size;
    attr->VeAttr.AttrH265e.mPicWidth   = cfg->dst_width;
    attr->VeAttr.AttrH265e.mPicHeight  = cfg->dst_height;
    attr->VeAttr.AttrH265e.mbPIntraEnable = TRUE;

    switch (cfg->enc_profile) {
        case 0:  attr->VeAttr.AttrH265e.mProfile = VENC_H265ProfileMain;   break;
        case 1:  attr->VeAttr.AttrH265e.mProfile = VENC_H265ProfileMain10; break;
        case 2:
        default: attr->VeAttr.AttrH265e.mProfile = VENC_H265ProfileMainStill; break;
    }
    attr->VeAttr.AttrH265e.mLevel = 0;

    if (cfg->fast_enc) {
        attr->VeAttr.AttrH265e.mFastEncFlag = TRUE;
    }
}

/* ---- 内部辅助：填充 MJPEG 特有属性 ---- */
static void fill_mjpeg_attr(VENC_CHN_ATTR_S *attr, const venc_config_t *cfg)
{
    attr->VeAttr.AttrMjpeg.mbByFrame   = TRUE;
    attr->VeAttr.AttrMjpeg.mBufSize    = cfg->vbv_buf_size;
    attr->VeAttr.AttrMjpeg.mThreshSize = cfg->vbv_thresh_size;
    attr->VeAttr.AttrMjpeg.mPicWidth   = cfg->dst_width;
    attr->VeAttr.AttrMjpeg.mPicHeight  = cfg->dst_height;
}

/* ---- 内部辅助：填充编码器公共属性 ---- */
static void fill_ve_common(VENC_CHN_ATTR_S *attr, const venc_config_t *cfg)
{
    if (cfg->online_enable) {
        attr->VeAttr.mOnlineEnable      = 1;
        attr->VeAttr.mOnlineShareBufNum = cfg->online_share_buf_num;
    }
    attr->VeAttr.Type         = cfg->enc_type;
    attr->VeAttr.MaxKeyInterval = cfg->key_frame_interval;
    attr->VeAttr.SrcPicWidth  = cfg->src_width;
    attr->VeAttr.SrcPicHeight = cfg->src_height;
    attr->VeAttr.Field        = VIDEO_FIELD_FRAME;
    attr->VeAttr.PixelFormat  = cfg->pix_fmt;
    attr->VeAttr.mColorSpace  = cfg->color_space;
    attr->VeAttr.mDropFrameNum = cfg->drop_frame_num;
    attr->VeAttr.mVeRefFrameLbcMode = cfg->ref_frame_lbc_mode;

    /* 旋转 */
    switch (cfg->rotate) {
        case 90:  attr->VeAttr.Rotate = ROTATE_90;  break;
        case 180: attr->VeAttr.Rotate = ROTATE_180; break;
        case 270: attr->VeAttr.Rotate = ROTATE_270; break;
        default:  attr->VeAttr.Rotate = ROTATE_NONE; break;
    }

    attr->EncppAttr.mbEncppEnable = cfg->encpp_enable;
}

/* ---- 内部辅助：填充码率控制参数 ---- */
static void fill_rc_param(VENC_CHN_ATTR_S *attr, VENC_RC_PARAM_S *rc,
                          const venc_config_t *cfg)
{
    rc->product_mode = cfg->product_mode;
    rc->sensor_type  = cfg->sensor_type;

    if (cfg->enc_type == PT_H264) {
        switch (cfg->rc_mode) {
            case RC_MODE_VBR:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                attr->RcAttr.mAttrH264Vbr.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH264Vbr.mStatTime = 1;
                attr->RcAttr.mAttrH264Vbr.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH264Vbr.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH264Vbr.mMaxBitRate = cfg->bit_rate;
                rc->ParamH264Vbr.mMinQp  = cfg->min_i_qp;
                rc->ParamH264Vbr.mMaxQp  = cfg->max_i_qp;
                rc->ParamH264Vbr.mMaxPqp = cfg->max_p_qp;
                rc->ParamH264Vbr.mMinPqp = cfg->min_p_qp;
                rc->ParamH264Vbr.mQpInit = cfg->init_qp;
                rc->ParamH264Vbr.mMovingTh = cfg->moving_th;
                rc->ParamH264Vbr.mQuality  = cfg->quality;
                rc->ParamH264Vbr.mIFrmBitsCoef = cfg->i_bits_coef;
                rc->ParamH264Vbr.mPFrmBitsCoef = cfg->p_bits_coef;
                break;
            case RC_MODE_FIXQP:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                attr->RcAttr.mAttrH264FixQp.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH264FixQp.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH264FixQp.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH264FixQp.mIQp = cfg->min_i_qp;
                attr->RcAttr.mAttrH264FixQp.mPQp = cfg->min_p_qp;
                break;
            case RC_MODE_ABR:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                attr->RcAttr.mAttrH264Abr.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH264Abr.mStatTime = 1;
                attr->RcAttr.mAttrH264Abr.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH264Abr.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH264Abr.mMaxBitRate = cfg->bit_rate;
                attr->RcAttr.mAttrH264Abr.mMinQp = cfg->min_i_qp;
                attr->RcAttr.mAttrH264Abr.mMaxQp = cfg->max_i_qp;
                attr->RcAttr.mAttrH264Abr.mQuality = cfg->quality;
                break;
            case RC_MODE_CBR:
            default:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                attr->RcAttr.mAttrH264Cbr.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH264Cbr.mStatTime = 1;
                attr->RcAttr.mAttrH264Cbr.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH264Cbr.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH264Cbr.mBitRate = cfg->bit_rate;
                rc->ParamH264Cbr.mMaxQp  = cfg->max_i_qp;
                rc->ParamH264Cbr.mMinQp  = cfg->min_i_qp;
                rc->ParamH264Cbr.mMaxPqp = cfg->max_p_qp;
                rc->ParamH264Cbr.mMinPqp = cfg->min_p_qp;
                rc->ParamH264Cbr.mQpInit = cfg->init_qp;
                break;
        }
    } else if (cfg->enc_type == PT_H265) {
        switch (cfg->rc_mode) {
            case RC_MODE_VBR:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                attr->RcAttr.mAttrH265Vbr.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH265Vbr.mStatTime = 1;
                attr->RcAttr.mAttrH265Vbr.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH265Vbr.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH265Vbr.mMaxBitRate = cfg->bit_rate;
                rc->ParamH265Vbr.mMinQp  = cfg->min_i_qp;
                rc->ParamH265Vbr.mMaxQp  = cfg->max_i_qp;
                rc->ParamH265Vbr.mMaxPqp = cfg->max_p_qp;
                rc->ParamH265Vbr.mMinPqp = cfg->min_p_qp;
                rc->ParamH265Vbr.mQpInit = cfg->init_qp;
                rc->ParamH265Vbr.mMovingTh = cfg->moving_th;
                rc->ParamH265Vbr.mQuality  = cfg->quality;
                break;
            case RC_MODE_FIXQP:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                attr->RcAttr.mAttrH265FixQp.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH265FixQp.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH265FixQp.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH265FixQp.mIQp = cfg->min_i_qp;
                attr->RcAttr.mAttrH265FixQp.mPQp = cfg->min_p_qp;
                break;
            case RC_MODE_CBR:
            default:
                attr->RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                attr->RcAttr.mAttrH265Cbr.mGop = cfg->gop_size;
                attr->RcAttr.mAttrH265Cbr.mStatTime = 1;
                attr->RcAttr.mAttrH265Cbr.mSrcFrmRate = cfg->src_frame_rate;
                attr->RcAttr.mAttrH265Cbr.fr32DstFrmRate = cfg->dst_frame_rate;
                attr->RcAttr.mAttrH265Cbr.mBitRate = cfg->bit_rate;
                rc->ParamH265Cbr.mMaxQp  = cfg->max_i_qp;
                rc->ParamH265Cbr.mMinQp  = cfg->min_i_qp;
                rc->ParamH265Cbr.mMaxPqp = cfg->max_p_qp;
                rc->ParamH265Cbr.mMinPqp = cfg->min_p_qp;
                rc->ParamH265Cbr.mQpInit = cfg->init_qp;
                break;
        }
    } else if (cfg->enc_type == PT_MJPEG) {
        switch (cfg->rc_mode) {
            case RC_MODE_FIXQP:
                attr->RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
                attr->RcAttr.mAttrMjpegeFixQp.mQfactor = cfg->quality;
                break;
            case RC_MODE_CBR:
            default:
                attr->RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
                attr->RcAttr.mAttrMjpegeCbr.mBitRate = cfg->bit_rate;
                break;
        }
    }

    /* GOP */
    switch (cfg->gop_mode) {
        case 1:  attr->GopAttr.enGopMode = VENC_GOPMODE_DUALP;  break;
        case 2:  attr->GopAttr.enGopMode = VENC_GOPMODE_SMARTP; break;
        default: attr->GopAttr.enGopMode = VENC_GOPMODE_NORMALP; break;
    }
    attr->GopAttr.mGopSize = cfg->gop_size;

    alogd("venc: rc_mode=%d gop_mode=%d gop_size=%d bitrate=%d",
          attr->RcAttr.mRcMode, attr->GopAttr.enGopMode,
          attr->GopAttr.mGopSize, cfg->bit_rate);
}

/* ============ 公开接口 ============ */

int venc_init(venc_handle_t **enc, const venc_config_t *cfg)
{
    MPP_CHECK_NULL(enc, "enc");
    MPP_CHECK_NULL(cfg, "cfg");

    venc_handle_t *e = (venc_handle_t *)calloc(1, sizeof(venc_handle_t));
    MPP_CHECK_NULL(e, "venc_handle_t alloc");
    e->cfg = *cfg;
    e->chn = cfg->chn_id;

    /* 1. 填充编码器属性 */
    memset(&e->attr, 0, sizeof(VENC_CHN_ATTR_S));
    fill_ve_common(&e->attr, cfg);

    switch (cfg->enc_type) {
        case PT_H264:  fill_h264_attr(&e->attr, cfg);  break;
        case PT_H265:  fill_h265_attr(&e->attr, cfg);  break;
        case PT_MJPEG: fill_mjpeg_attr(&e->attr, cfg); break;
        default:       aloge("unknown enc_type=%d", cfg->enc_type); return -1;
    }

    memset(&e->rc_param, 0, sizeof(VENC_RC_PARAM_S));
    fill_rc_param(&e->attr, &e->rc_param, cfg);

    /* 2. 创建编码通道 */
    int ret = AW_MPI_VENC_CreateChn(e->chn, &e->attr);
    MPP_CHECK_RET(ret, "AW_MPI_VENC_CreateChn");

    /* 3. 设置码率控制参数 */
    ret = AW_MPI_VENC_SetRcParam(e->chn, &e->rc_param);
    MPP_CHECK_RET(ret, "AW_MPI_VENC_SetRcParam");

    /* ---- DIAG: 读回 SDK 实际生效的所有参数（把0替换成的默认值） ---- */
    {
        VENC_CHN_ATTR_S actual_attr;
        VENC_RC_PARAM_S actual_rc;
        memset(&actual_attr, 0, sizeof(actual_attr));
        memset(&actual_rc, 0, sizeof(actual_rc));
        AW_MPI_VENC_GetChnAttr(e->chn, &actual_attr);
        AW_MPI_VENC_GetRcParam(e->chn, &actual_rc);

        printf("[VENC-DIAG] ===== SDK actual parameters after CreateChn+SetRcParam =====\n");
        printf("[VENC-DIAG] enc_type=%d, profile=%d\n",
               actual_attr.VeAttr.Type, cfg->enc_profile);
        printf("[VENC-DIAG] resolution: src=%dx%d dst=%dx%d\n",
               actual_attr.VeAttr.SrcPicWidth, actual_attr.VeAttr.SrcPicHeight,
               cfg->dst_width, cfg->dst_height);
        printf("[VENC-DIAG] rc_mode=%d, bitrate=%d kbps, gop=%d, framerate=%d/%d, product_mode=%u, sensor_type=%u\n",
               actual_attr.RcAttr.mRcMode,
               cfg->bit_rate / 1000,
               actual_attr.GopAttr.mGopSize,
               cfg->src_frame_rate, cfg->dst_frame_rate,
               actual_rc.product_mode, actual_rc.sensor_type);

        if (cfg->enc_type == PT_H265) {
            printf("[VENC-DIAG] H.265 VBV BufSize=%d, ThreshSize=%d, FastEnc=%d\n",
                   actual_attr.VeAttr.AttrH265e.mBufSize,
                   actual_attr.VeAttr.AttrH265e.mThreshSize,
                   actual_attr.VeAttr.AttrH265e.mFastEncFlag);
            printf("[VENC-DIAG] H.265 CBR: bitrate=%d, MaxQp=%d, MinQp=%d, MaxPqp=%d, MinPqp=%d, QpInit=%d\n",
                   actual_attr.RcAttr.mAttrH265Cbr.mBitRate,
                   actual_rc.ParamH265Cbr.mMaxQp,
                   actual_rc.ParamH265Cbr.mMinQp,
                   actual_rc.ParamH265Cbr.mMaxPqp,
                   actual_rc.ParamH265Cbr.mMinPqp,
                   actual_rc.ParamH265Cbr.mQpInit);
        } else if (cfg->enc_type == PT_H264) {
            printf("[VENC-DIAG] H.264 VBV BufSize=%d, ThreshSize=%d, FastEnc=%d\n",
                   actual_attr.VeAttr.AttrH264e.BufSize,
                   actual_attr.VeAttr.AttrH264e.mThreshSize,
                   actual_attr.VeAttr.AttrH264e.FastEncFlag);
            printf("[VENC-DIAG] H.264 CBR: bitrate=%d, MaxQp=%d, MinQp=%d, MaxPqp=%d, MinPqp=%d, QpInit=%d\n",
                   actual_attr.RcAttr.mAttrH264Cbr.mBitRate,
                   actual_rc.ParamH264Cbr.mMaxQp,
                   actual_rc.ParamH264Cbr.mMinQp,
                   actual_rc.ParamH264Cbr.mMaxPqp,
                   actual_rc.ParamH264Cbr.mMinPqp,
                   actual_rc.ParamH264Cbr.mQpInit);
        }
        printf("[VENC-DIAG] ===============================================================\n");
    }

    /* 4. 设置帧率 */
    VENC_FRAME_RATE_S frame_rate;
    frame_rate.SrcFrmRate = cfg->src_frame_rate;
    frame_rate.DstFrmRate = cfg->dst_frame_rate;
    ret = AW_MPI_VENC_SetFrameRate(e->chn, &frame_rate);
    MPP_CHECK_RET(ret, "AW_MPI_VENC_SetFrameRate");

    /* 5. 2D 降噪 */
    if (cfg->d2nr_enable) {
        s2DfilterParam d2nr;
        memset(&d2nr, 0, sizeof(d2nr));
        d2nr.enable_2d_filter   = 1;
        d2nr.filter_strength_y  = cfg->d2nr_strength_y;
        d2nr.filter_strength_uv = cfg->d2nr_strength_c;
        AW_MPI_VENC_Set2DFilter(e->chn, &d2nr);
        alogd("venc: 2DNR enabled (y=%d, uv=%d)", cfg->d2nr_strength_y, cfg->d2nr_strength_c);
    }

    /* 6. 3D 降噪 */
    if (cfg->d3nr_enable) {
        s3DfilterParam d3nr;
        memset(&d3nr, 0, sizeof(d3nr));
        d3nr.enable_3d_filter = 1;
        AW_MPI_VENC_Set3DFilter(e->chn, &d3nr);
        alogd("venc: 3DNR enabled");
    }

    /* 7. 转灰度 */
    if (cfg->color2grey) {
        VENC_COLOR2GREY_S grey;
        memset(&grey, 0, sizeof(grey));
        grey.bColor2Grey = TRUE;
        AW_MPI_VENC_SetColor2Grey(e->chn, &grey);
        alogd("venc: color2grey enabled");
    }

    /* 8. 水平镜像 */
    if (cfg->horizon_flip) {
        AW_MPI_VENC_SetHorizonFlip(e->chn, 1);
        alogd("venc: horizon_flip enabled");
    }

    /* 9. 裁剪 */
    if (cfg->crop_enable) {
        VENC_CROP_CFG_S crop;
        memset(&crop, 0, sizeof(crop));
        crop.bEnable       = TRUE;
        crop.Rect.X        = cfg->crop_x;
        crop.Rect.Y        = cfg->crop_y;
        crop.Rect.Width    = cfg->crop_w;
        crop.Rect.Height   = cfg->crop_h;
        AW_MPI_VENC_SetCrop(e->chn, &crop);
        alogd("venc: crop [%d,%d,%d,%d]", cfg->crop_x, cfg->crop_y, cfg->crop_w, cfg->crop_h);
    }

    /* 10. SmartP */
    if (cfg->enable_smart) {
        VencSmartFun smart;
        memset(&smart, 0, sizeof(smart));
        smart.smart_fun_en = 1;
        smart.img_bin_en   = 1;
        smart.img_bin_th   = 0;
        smart.shift_bits   = 2;
        AW_MPI_VENC_SetSmartP(e->chn, &smart);
        alogd("venc: SmartP enabled");
    }

    /* 11. SVC */
    if (cfg->svc_layer > 0) {
        VencH264SVCSkip svc;
        memset(&svc, 0, sizeof(svc));
        svc.nTemporalSVC = cfg->svc_layer;
        AW_MPI_VENC_SetH264SVCSkip(e->chn, &svc);
        alogd("venc: SVC layer=%d", cfg->svc_layer);
    }

    alogd("venc_init: chn=%d type=%d %dx%d→%dx%d bitrate=%d OK",
          e->chn, cfg->enc_type, cfg->src_width, cfg->src_height,
          cfg->dst_width, cfg->dst_height, cfg->bit_rate);

    *enc = e;
    return 0;
}

int venc_get_sps_pps(venc_handle_t *enc)
{
    MPP_CHECK_NULL(enc, "enc");
    memset(&enc->sps_pps, 0, sizeof(VencHeaderData));

    int ret;
    if (enc->cfg.enc_type == PT_H264) {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(enc->chn, &enc->sps_pps);
        MPP_CHECK_RET(ret, "AW_MPI_VENC_GetH264SpsPpsInfo");
    } else if (enc->cfg.enc_type == PT_H265) {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(enc->chn, &enc->sps_pps);
        MPP_CHECK_RET(ret, "AW_MPI_VENC_GetH265SpsPpsInfo");
    }
    alogd("venc_get_sps_pps: chn=%d OK", enc->chn);
    return 0;
}

int venc_start(venc_handle_t *enc)
{
    MPP_CHECK_NULL(enc, "enc");
    if (enc->is_running) { alogw("venc_start: already running"); return 0; }

    int ret = AW_MPI_VENC_StartRecvPic(enc->chn);
    MPP_CHECK_RET(ret, "AW_MPI_VENC_StartRecvPic");

    enc->is_running = true;
    alogd("venc_start: chn=%d OK", enc->chn);
    return 0;
}

int venc_stop(venc_handle_t *enc)
{
    MPP_CHECK_NULL(enc, "enc");
    if (!enc->is_running) return 0;

    AW_MPI_VENC_StopRecvPic(enc->chn);
    enc->is_running = false;
    alogd("venc_stop: chn=%d OK", enc->chn);
    return 0;
}

int venc_deinit(venc_handle_t **enc)
{
    if (!enc || !*enc) return 0;

    venc_handle_t *e = *enc;
    if (e->is_running) venc_stop(e);
    if (e->chn >= 0)   AW_MPI_VENC_DestroyChn(e->chn);

    alogd("venc_deinit: chn=%d OK", e->chn);
    free(e);
    *enc = NULL;
    return 0;
}
