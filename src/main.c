/* main.c - VI0(1080p)->VENC->(MUX+RTSP), VI2(480x800 NV12)->VO */
#include "vi_module.h"
#include "vo_module.h"
#include "venc_module.h"
#include "audio_module.h"
#include "mux_module.h"
#include "npu_module.h"
#include "rtsp_module.h"
#include "diag_vi_dump.h"

static struct {
    vi_handle_t *vi;
    vi_handle_t *vi2;
    npu_handle_t *npu;
    vo_handle_t *vo;
    venc_handle_t *ve;
    audio_handle_t *au;
    mux_handle_t *mx;
    rtsp_handle_t *rt;
    cdx_sem_t sem;
} g;

static vi_config_t c_vi;
static vi_config_t c_vi2;
static vo_config_t c_vo;
static venc_config_t c_ve;
static audio_config_t c_au;
static mux_config_t c_mx;
static npu_config_t c_npu;
static rtsp_config_t c_rt;

static void signal_handler(int sig)
{
    (void)sig;
    cdx_sem_up(&g.sem);
}

static void stage_log(const char *stage)
{
    if (!stage) {
        return;
    }
    printf("[MAIN] %s\n", stage);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    bool enable_odet = false;
    char odet_model[MAX_FILE_PATH_LEN] = "/mnt/UDISK/Facedet_480_288_nv12.nb";
    int ret;
    int rc = 0;
    MPP_SYS_CONF_S sys_conf;
    int argi;

    for (argi = 1; argi < argc; ++argi) {
        if (!strcmp(argv[argi], "--diag-vi-dump")) {
            return run_diag_vi_dump();
        }
        if (!strcmp(argv[argi], "--enable-odet")) {
            enable_odet = true;
            continue;
        }
        if (!strcmp(argv[argi], "--odet-model") && (argi + 1) < argc) {
            strncpy(odet_model, argv[argi + 1], sizeof(odet_model) - 1);
            odet_model[sizeof(odet_model) - 1] = '\0';
            ++argi;
        }
    }

    cdx_sem_init(&g.sem, 0);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    memset(&c_vi, 0, sizeof(c_vi));
    c_vi.dev_id = 0;
    c_vi.isp_dev = 0;
    c_vi.chn_id = 0;
    c_vi.width = 1920;
    c_vi.height = 1088;
    c_vi.fps = 20;
    c_vi.buf_num = 5;
    c_vi.pix_fmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    c_vi.color_space = V4L2_COLORSPACE_JPEG;
    c_vi.online_en = 0;
    c_vi.online_share_buf_num = 0;
    c_vi.use_current_win = 0;

    memset(&c_vi2, 0, sizeof(c_vi2));
    c_vi2.dev_id = 4;
    c_vi2.isp_dev = 0;
    c_vi2.chn_id = 0;
    c_vi2.width = 480;
    c_vi2.height = 800;
    c_vi2.fps = 20;
    c_vi2.buf_num = 5;
    /* LCD 蓝脸/负片时，仅切换 VI2 预览链的 UV 顺序；VI0 录制主链保持不动。 */
    c_vi2.pix_fmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    c_vi2.color_space = V4L2_COLORSPACE_JPEG;
    c_vi2.use_current_win = 0;

    memset(&c_vo, 0, sizeof(c_vo));
    c_vo.layer = 4;
    c_vo.chn_id = 0;
    c_vo.disp_width = 480;
    c_vo.disp_height = 800;

    memset(&c_ve, 0, sizeof(c_ve));
    c_ve.chn_id = 0;
    c_ve.enc_type = PT_H265;
    c_ve.src_width = 1920;
    c_ve.src_height = 1088;
    c_ve.dst_width = 1920;
    c_ve.dst_height = 1088;
    c_ve.src_frame_rate = 20;
    c_ve.dst_frame_rate = 20;
    c_ve.pix_fmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    c_ve.color_space = V4L2_COLORSPACE_JPEG;
    c_ve.rc_mode = RC_MODE_CBR;
    c_ve.bit_rate = 2000000;
    c_ve.product_mode = VENC_PRODUCT_IPC_MODE;
    /*
     * GC2053 当前没有找到“必须显式写成某个 WDR sensor_type”的项目内证据，
     * 先保留 SDK 默认路径，避免把别家 sample 的 sensor 假设硬套过来。
     */
    c_ve.sensor_type = 0;
    c_ve.init_qp = 30;
    c_ve.min_i_qp = 25;
    c_ve.max_i_qp = 42;
    c_ve.min_p_qp = 25;
    c_ve.max_p_qp = 45;
    /*
     * IPC/RTSP 场景更适合把 IDR 周期和 GOP 对齐，避免“日志 GOP 一套，
     * 实际关键帧节奏另一套”的假一致。20fps 下取 40 帧，相当于 2 秒一个 IDR。
     */
    c_ve.gop_size = 40;
    c_ve.key_frame_interval = 40;
    c_ve.vbv_thresh_size = (((1920 * 1088 * 3 / 2) / 3) + 1023) & ~1023;
    c_ve.vbv_buf_size = ((c_ve.bit_rate * 4 / 8) + c_ve.vbv_thresh_size + 1023) & ~1023;
    c_ve.fast_enc = false;
    c_ve.online_enable = false;
    c_ve.online_share_buf_num = 0;

    memset(&c_au, 0, sizeof(c_au));
    c_au.ai_dev = 0;
    c_au.ai_chn = 0;
    c_au.aenc_chn = 0;
    c_au.sample_rate = 16000;
    c_au.channels = 1;
    c_au.bit_width = 16;
    c_au.sound_mode = AUDIO_SOUND_MODE_MONO;
    c_au.pt_num_per_frm = 1024;
    c_au.frm_num = 4;
    c_au.aenc_bitrate = 16000;

    memset(&c_mx, 0, sizeof(c_mx));
    c_mx.grp_id = 0;
    c_mx.enc_type = PT_H265;
    c_mx.video_width = 1920;
    c_mx.video_height = 1088;
    c_mx.video_fps = 20;
    c_mx.enc_chn = c_ve.chn_id;
    c_mx.audio_enabled = true;
    c_mx.audio_enc_type = PT_AAC;
    c_mx.audio_sample_rate = c_au.sample_rate;
    c_mx.audio_bit_width = c_au.bit_width;
    c_mx.audio_channels = c_au.channels;
    c_mx.audio_pt_num_per_frm = c_au.pt_num_per_frm;
    strcpy(c_mx.output_dir, "/mnt/UDISK");
    strcpy(c_mx.output_prefix, "cam");

    memset(&c_rt, 0, sizeof(c_rt));
    c_rt.rtsp_port = 554;
    c_rt.video_enc_chn = c_ve.chn_id;
    c_rt.aenc_chn = c_au.aenc_chn;
    c_rt.mux_grp = c_mx.grp_id;
    c_rt.audio_enabled = true;
    c_rt.audio_sample_rate = c_au.sample_rate;
    c_rt.audio_channels = c_au.channels;
    c_rt.video_payload_type = 96;
    c_rt.audio_payload_type = 97;
    c_rt.ssrc = 0x12345678;
    c_rt.max_packet_size = 1458;
    c_rt.transport_mode = RTSP_TRANSPORT_TCP;

    memset(&c_npu, 0, sizeof(c_npu));
    c_npu.enable = enable_odet;
    c_npu.detect_interval = 1;
    c_npu.max_boxes = MAX_OBJECT_DET_NUM;
    c_npu.model_input_width = 480;
    c_npu.model_input_height = 288;
    c_npu.score_thresh = 0.60f;
    c_npu.preview_width = c_vo.disp_width;
    c_npu.preview_height = c_vo.disp_height;
    c_npu.encoder_width = c_ve.dst_width;
    c_npu.encoder_height = c_ve.dst_height;
    c_npu.preview_region_base = 200;
    c_npu.encoder_region_base = 300;
    c_npu.preview_vi_dev = c_vi2.dev_id;
    c_npu.preview_vi_chn = c_vi2.chn_id;
    c_npu.encoder_vi_dev = c_vi.dev_id;
    c_npu.encoder_vi_chn = c_vi.chn_id;
    strncpy(c_npu.model_path, odet_model, sizeof(c_npu.model_path) - 1);
    c_npu.model_path[sizeof(c_npu.model_path) - 1] = '\0';
    c_npu.vi_cfg.dev_id = 8;
    c_npu.vi_cfg.isp_dev = 0;
    c_npu.vi_cfg.chn_id = 0;
    c_npu.vi_cfg.width = 480;
    c_npu.vi_cfg.height = 288;
    c_npu.vi_cfg.fps = 20;
    c_npu.vi_cfg.buf_num = 5;
    c_npu.vi_cfg.pix_fmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    c_npu.vi_cfg.color_space = V4L2_COLORSPACE_JPEG;
    c_npu.vi_cfg.use_current_win = 0;

    memset(&sys_conf, 0, sizeof(sys_conf));
    sys_conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&sys_conf);
    ret = AW_MPI_SYS_Init();
    if (ret) {
        goto ds;
    }

    ret = vi_init(&g.vi, &c_vi);
    if (ret) {
        rc = -1;
        goto ds;
    }
    stage_log("vi0 init ok");

    ret = vi_init(&g.vi2, &c_vi2);
    if (ret) {
        rc = -1;
        goto dvi;
    }
    stage_log("vi2 init ok");

    ret = vo_init(&g.vo, &c_vo);
    if (ret) {
        rc = -1;
        goto dvi2;
    }
    stage_log("vo init ok");

    ret = venc_init(&g.ve, &c_ve);
    if (ret) {
        rc = -1;
        goto dvo;
    }
    stage_log("venc init ok");

    ret = venc_get_sps_pps(g.ve);
    if (ret) {
        rc = -1;
        goto dve;
    }
    stage_log("venc header ok");

    ret = mux_init(&g.mx, &c_mx);
    if (ret) {
        rc = -1;
        goto dve;
    }
    stage_log("mux init ok");

    ret = mux_set_sps_pps(g.mx, &g.ve->sps_pps, g.ve->chn);
    if (ret) {
        rc = -1;
        goto dm;
    }
    stage_log("mux header ok");

    ret = audio_init(&g.au, &c_au);
    if (ret) {
        rc = -1;
        goto dm;
    }
    stage_log("audio init ok");

    ret = rtsp_init(&g.rt, &c_rt);
    if (ret) {
        rc = -1;
        goto dau;
    }
    stage_log("rtsp init ok");

    ret = rtsp_set_video_header(g.rt, &g.ve->sps_pps);
    if (ret) {
        rc = -1;
        goto drt;
    }
    stage_log("rtsp header ok");

    if (c_npu.enable) {
        ret = npu_init(&g.npu, &c_npu);
        if (ret) {
            rc = -1;
            goto drt;
        }
        stage_log("npu init ok");
    }

    rtsp_set_router_refs(g.ve->chn, g.au->aenc_chn, g.mx->grp, g.rt);

    {
        MPP_CHN_S vi0 = {MOD_ID_VIU, g.vi->dev, g.vi->chn};
        MPP_CHN_S ve0 = {MOD_ID_VENC, 0, g.ve->chn};
        AW_MPI_SYS_Bind(&vi0, &ve0);
    }

    {
        MPP_CHN_S vi2 = {MOD_ID_VIU, g.vi2->dev, g.vi2->chn};
        MPP_CHN_S vo = vo_get_chn(g.vo);
        AW_MPI_SYS_Bind(&vi2, &vo);
    }

    /*
     * Keep VI->VENC bind, but let the router thread fan encoded frames out to
     * both RTSP and MUX manually. This matches the SDK RTSP sample better than
     * binding VENC->MUX and avoids starving VENC_GetStream on this board.
     */
    vo_start(g.vo);
    stage_log("vo start ok");
    vi_start(g.vi2);
    stage_log("vi2 start ok");
    vi_start(g.vi);
    stage_log("vi0 start ok");
    venc_start(g.ve);
    stage_log("venc start ok");
    audio_start(g.au);
    stage_log("audio start ok");
    mux_start(g.mx);
    stage_log("mux start ok");
    rtsp_start(g.rt);
    stage_log("rtsp start ok");
    if (g.npu) {
        ret = npu_start(g.npu);
        if (ret) {
            rc = -1;
            goto stop_running;
        }
        stage_log("npu start ok");
    }

    printf("ALL:1080p MP4(A/V)+RTSP(TCP:554 A/V)+VO(480x800) LCD");
    if (g.npu) {
        printf("+ODET(dev8, model=%s)", c_npu.model_path);
    }
    printf("\n");
    cdx_sem_wait(&g.sem);

stop_running:
    if (g.npu) {
        npu_stop(g.npu);
    }
    rtsp_stop(g.rt);
    audio_stop(g.au);
    mux_stop(g.mx);
    venc_stop(g.ve);
    vi_stop(g.vi);
    vi_stop(g.vi2);
    vo_stop(g.vo);

    {
        MPP_CHN_S vi0 = {MOD_ID_VIU, g.vi->dev, g.vi->chn};
        MPP_CHN_S ve0 = {MOD_ID_VENC, 0, g.ve->chn};
        AW_MPI_SYS_UnBind(&vi0, &ve0);
    }

    {
        MPP_CHN_S vi2 = {MOD_ID_VIU, g.vi2->dev, g.vi2->chn};
        MPP_CHN_S vo = vo_get_chn(g.vo);
        AW_MPI_SYS_UnBind(&vi2, &vo);
    }

drt:
    rtsp_deinit(&g.rt);
dau:
    audio_deinit(&g.au);
dm:
    mux_deinit(&g.mx);
dve:
    npu_deinit(&g.npu);
    venc_deinit(&g.ve);
dvo:
    vo_deinit(&g.vo);
dvi2:
    vi_deinit(&g.vi2);
dvi:
    vi_deinit(&g.vi);
ds:
    AW_MPI_SYS_Exit();
    cdx_sem_deinit(&g.sem);
    return rc;
}
