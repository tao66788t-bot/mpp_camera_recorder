/**
 * @file    rtsp_module.c
 * @brief   RTSP 服务器 + RTP 推流（TCP interleaved 优先，UDP 回退）
 *
 * TCP interleaved: RTP 包格式 $<ch><len_hi><len_lo><rtp_data>
 * 数据走 RTSP 的 TCP 连接，不需要额外端口，过防火墙/ADB forward 无压力
 */
#include "rtsp_module.h"
#include "vencoder.h"
#include "mm_comm_aenc.h"
#include "mpi_aenc.h"
#include "mm_comm_mux.h"
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define RTP_HEADER_SIZE        12
#define RTP_MAX_PLOAD          1458
#define H265_NAL_HEADER_SIZE   2
#define H265_FU_HEADER_SIZE    3
#define RTSP_FRAME_BUF_SIZE    (512 * 1024)

/* 全局：TCP client fd（push 函数用）*/
static int g_client_fd = -1;

static int aac_sample_rate_index(int sample_rate)
{
    switch (sample_rate) {
    case 96000: return 0;
    case 88200: return 1;
    case 64000: return 2;
    case 48000: return 3;
    case 44100: return 4;
    case 32000: return 5;
    case 24000: return 6;
    case 22050: return 7;
    case 16000: return 8;
    case 12000: return 9;
    case 11025: return 10;
    case 8000:  return 11;
    case 7350:  return 12;
    default:    return -1;
    }
}

static unsigned int build_aac_audio_specific_config(const rtsp_handle_t *r)
{
    int sample_rate_index;
    unsigned int channels;

    if (!r) {
        return 0;
    }

    sample_rate_index = aac_sample_rate_index(r->cfg.audio_sample_rate);
    if (sample_rate_index < 0) {
        sample_rate_index = aac_sample_rate_index(16000);
    }
    channels = (r->cfg.audio_channels > 0) ? (unsigned int)r->cfg.audio_channels : 1U;
    return (2U << 11) | ((unsigned int)sample_rate_index << 7) | (channels << 3);
}

/* local IP helper */
static void get_local_ip(char *b, int len) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {AF_INET, htons(80)};
    inet_aton("8.8.8.8", &a.sin_addr);
    connect(fd, (struct sockaddr*)&a, sizeof(a));
    struct sockaddr_in l; socklen_t s = sizeof(l);
    getsockname(fd, (struct sockaddr*)&l, &s);
    snprintf(b, len, "%s", inet_ntoa(l.sin_addr));
    close(fd);
}

static void get_rtsp_local_ip(const rtsp_handle_t *r, char *b, int len)
{
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);

    if (r && r->rtsp_client_fd > 0) {
        memset(&local_addr, 0, sizeof(local_addr));
        if (getsockname(r->rtsp_client_fd, (struct sockaddr *)&local_addr, &addr_len) == 0) {
            snprintf(b, len, "%s", inet_ntoa(local_addr.sin_addr));
            return;
        }
    }

    get_local_ip(b, len);
}

/* SDP */
static char sbuf[2048];
static const char *build_sdp(const rtsp_handle_t *r) {
    char ip[32] = "0.0.0.0";
    int written;

    get_rtsp_local_ip(r, ip, sizeof(ip));
    written = snprintf(sbuf, sizeof(sbuf),
        "v=0\r\n"
        "o=- %lld 1 IN IP4 %s\r\n"
        "s=IPC Live\r\n"
        "t=0 0\r\n"
        "a=control:*\r\n"
        "m=video 0 RTP/AVP %d\r\n"
        "a=rtpmap:%d H265/90000\r\n"
        "a=control:track0\r\n",
        (long long)time(NULL), ip,
        r->cfg.video_payload_type, r->cfg.video_payload_type);

    if (r->cfg.audio_enabled && written > 0 && written < (int)sizeof(sbuf)) {
        unsigned int asc = build_aac_audio_specific_config(r);
        snprintf(sbuf + written, sizeof(sbuf) - (size_t)written,
            "m=audio 0 RTP/AVP %d\r\n"
            "a=rtpmap:%d MPEG4-GENERIC/%d/%d\r\n"
            "a=fmtp:%d streamtype=5; profile-level-id=1; mode=AAC-hbr; config=%04X; SizeLength=13; IndexLength=3; IndexDeltaLength=3\r\n"
            "a=control:track1\r\n",
            r->cfg.audio_payload_type,
            r->cfg.audio_payload_type,
            r->cfg.audio_sample_rate,
            r->cfg.audio_channels,
            r->cfg.audio_payload_type,
            asc);
    }
    return sbuf;
}

/* RTP header builder */
static int rtp_hdr(uint8_t *b, unsigned int pt, unsigned short seq,
                    unsigned int ts, unsigned int ssrc, int m) {
    b[0]=0x80; b[1]=(uint8_t)(pt&0x7F)|(m?0x80:0);
    b[2]=(seq>>8)&0xFF; b[3]=seq&0xFF;
    b[4]=(ts>>24)&0xFF; b[5]=(ts>>16)&0xFF; b[6]=(ts>>8)&0xFF; b[7]=ts&0xFF;
    b[8]=(ssrc>>24)&0xFF; b[9]=(ssrc>>16)&0xFF; b[10]=(ssrc>>8)&0xFF; b[11]=ssrc&0xFF;
    return RTP_HEADER_SIZE;
}

/* UDP send helper */
static int udp_send(int fd, const uint8_t *d, unsigned int sz, const char *ip, int port) {
    if (fd < 0 || port <= 0) return -1;
    struct sockaddr_in a = {AF_INET, htons(port)};
    inet_aton(ip, &a.sin_addr);
    return (int)sendto(fd, d, sz, 0, (struct sockaddr*)&a, sizeof(a));
}

/* TCP interleaved send: $ + channel + len(2B BE) + data */
static int tcp_interleaved_send(int fd, uint8_t ch, const uint8_t *d, unsigned int sz) {
    if (fd < 0) return -1;
    uint8_t hdr[4];
    hdr[0] = '$';
    hdr[1] = ch;
    hdr[2] = (uint8_t)((sz >> 8) & 0xFF);
    hdr[3] = (uint8_t)(sz & 0xFF);
    struct iovec iov[2];
    iov[0].iov_base = hdr;       iov[0].iov_len = 4;
    iov[1].iov_base = (void *)d; iov[1].iov_len = sz;
    ssize_t n = writev(fd, iov, 2);
    return (n == (ssize_t)(4 + sz)) ? (int)sz : -1;
}

static int rtp_send(rtsp_handle_t *r, int is_video,
                    const uint8_t *data, unsigned int size);

static unsigned int annexb_start_code_len(const uint8_t *buf, unsigned int size)
{
    if (size >= 4 && buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1) {
        return 4;
    }
    if (size >= 3 && buf[0] == 0 && buf[1] == 0 && buf[2] == 1) {
        return 3;
    }
    return 0;
}

static int find_next_annexb_start_code(const uint8_t *buf, unsigned int size, unsigned int offset)
{
    unsigned int i;

    if (!buf || offset >= size) {
        return -1;
    }

    for (i = offset; i + 3 < size; ++i) {
        unsigned int sc_len = annexb_start_code_len(buf + i, size - i);
        if (sc_len > 0) {
            return (int)i;
        }
    }

    if (size >= 3 && offset <= size - 3) {
        for (i = (size >= 3 ? size - 3 : 0); i < size; ++i) {
            unsigned int sc_len = annexb_start_code_len(buf + i, size - i);
            if (sc_len > 0) {
                return (int)i;
            }
        }
    }

    return -1;
}

static int rtsp_send_h265_nal(rtsp_handle_t *rtsp, const uint8_t *nal,
                              unsigned int nal_size, unsigned int ts, int mark)
{
    uint8_t nal_type;

    if (!rtsp || !nal || nal_size <= H265_NAL_HEADER_SIZE) {
        return 0;
    }

    nal_type = (uint8_t)((nal[0] >> 1) & 0x3F);
    if (nal_size <= RTP_MAX_PLOAD) {
        uint8_t pkt[1500];
        int pos = rtp_hdr(pkt, rtsp->cfg.video_payload_type,
                          rtsp->rtp_seq_video++, ts, rtsp->cfg.ssrc, mark);
        memcpy(pkt + pos, nal, nal_size);
        pos += (int)nal_size;
        return rtp_send(rtsp, 1, pkt, (unsigned int)pos);
    }

    {
        const uint8_t *payload = nal + H265_NAL_HEADER_SIZE;
        unsigned int remaining = nal_size - H265_NAL_HEADER_SIZE;
        unsigned int max_chunk = RTP_MAX_PLOAD - H265_FU_HEADER_SIZE;
        int packet_count = 0;

        while (remaining > 0) {
            uint8_t pkt[1500];
            unsigned int chunk = (remaining > max_chunk) ? max_chunk : remaining;
            int start = (packet_count == 0);
            int last = (chunk == remaining);
            int pos = rtp_hdr(pkt, rtsp->cfg.video_payload_type,
                              rtsp->rtp_seq_video++, ts, rtsp->cfg.ssrc,
                              (mark && last) ? 1 : 0);

            pkt[pos++] = (uint8_t)((nal[0] & 0x81) | (49 << 1));
            pkt[pos++] = nal[1];
            pkt[pos++] = (uint8_t)((start ? 0x80 : 0x00) |
                                   (last ? 0x40 : 0x00) |
                                   nal_type);
            memcpy(pkt + pos, payload, chunk);
            pos += (int)chunk;
            rtp_send(rtsp, 1, pkt, (unsigned int)pos);
            payload += chunk;
            remaining -= chunk;
            packet_count++;
        }
        return packet_count;
    }
}

static int rtsp_push_h265_frame(rtsp_handle_t *rtsp, const uint8_t *data,
                                unsigned int size, unsigned long long pts)
{
    unsigned int ts;
    int first_sc;
    int sent = 0;

    if (!rtsp || rtsp->state != RTSP_STATE_PLAYING || !data || size == 0) {
        return 0;
    }

    ts = (unsigned int)(pts * 9 / 100);
    first_sc = find_next_annexb_start_code(data, size, 0);
    if (first_sc < 0) {
        return rtsp_send_h265_nal(rtsp, data, size, ts, 1);
    }

    {
        unsigned int pos = (unsigned int)first_sc;

        while (pos < size) {
            unsigned int sc_len = annexb_start_code_len(data + pos, size - pos);
            unsigned int nal_start;
            int next_sc;
            unsigned int nal_end;

            if (sc_len == 0) {
                pos++;
                continue;
            }

            nal_start = pos + sc_len;
            next_sc = find_next_annexb_start_code(data, size, nal_start);
            nal_end = (next_sc >= 0) ? (unsigned int)next_sc : size;

            while (nal_end > nal_start && data[nal_end - 1] == 0x00) {
                nal_end--;
            }

            if (nal_end > nal_start) {
                int mark = (next_sc < 0) ? 1 : 0;
                sent += rtsp_send_h265_nal(rtsp, data + nal_start,
                                           nal_end - nal_start, ts, mark);
            }

            if (next_sc < 0) {
                break;
            }
            pos = (unsigned int)next_sc;
        }
    }

    return sent;
}

/* 统一 RTP 发送分发：根据 transport_mode 走 TCP interleaved 或 UDP */
static int rtp_send(rtsp_handle_t *r, int is_video,
                     const uint8_t *data, unsigned int size) {
    if (r->cfg.transport_mode == RTSP_TRANSPORT_TCP) {
        int ch = is_video ? r->client_interleaved_video_rtp
                          : r->client_interleaved_audio_rtp;
        return tcp_interleaved_send(g_client_fd, (uint8_t)ch, data, size);
    } else {
        int fd   = is_video ? r->rtp_video_fd : r->rtp_audio_fd;
        int port = is_video ? r->client_rtp_video_port
                            : r->client_rtp_audio_port;
        return udp_send(fd, data, size, r->client_ip, port);
    }
}

int rtsp_push_h265(rtsp_handle_t *rtsp, const uint8_t *nal, unsigned int sz, unsigned long long pts) {
    return rtsp_push_h265_frame(rtsp, nal, sz, pts);
}

/* AAC push over UDP */
int rtsp_push_aac(rtsp_handle_t *rtsp, const uint8_t *d, unsigned int sz, unsigned long long pts) {
    if (!rtsp || !rtsp->cfg.audio_enabled || rtsp->state != RTSP_STATE_PLAYING) return 0;
    if (!d || sz==0) return 0;
    unsigned int audio_ssrc = rtsp->cfg.ssrc + 1;
    unsigned int ts = (unsigned int)((pts * (unsigned long long)rtsp->cfg.audio_sample_rate) / 1000000ULL);
    uint8_t pkt[1500];
    int pos = rtp_hdr(pkt, rtsp->cfg.audio_payload_type, rtsp->rtp_seq_audio++, ts, audio_ssrc, 1);
    pkt[pos++]=0x00; pkt[pos++]=0x10;
    uint16_t ah = (uint16_t)((sz<<3)&0xFFF8);
    pkt[pos++]=(ah>>8)&0xFF; pkt[pos++]=(ah&0xFF);
    memcpy(pkt+pos, d, sz); pos+=sz;
    return rtp_send(rtsp, 0, pkt, (unsigned int)pos);
}

int rtsp_set_video_header(rtsp_handle_t *rtsp, const VencHeaderData *header)
{
    if (!rtsp || !header || !header->pBuffer || header->nLength <= 0) {
        return -1;
    }
    if ((unsigned int)header->nLength > sizeof(rtsp->video_header)) {
        return -1;
    }

    memcpy(rtsp->video_header, header->pBuffer, header->nLength);
    rtsp->video_header_len = (unsigned int)header->nLength;
    return 0;
}

/* === RTSP server thread === */
static void rtsp_send(int fd, const char *s) { send(fd, s, strlen(s), 0); }

static void *rtsp_server_thread(void *arg) {
    rtsp_handle_t *r = (rtsp_handle_t*)arg;
    char buf[4096]; int lfd=r->rtsp_listen_fd, cseq;
    fd_set fds; struct timeval tv={1,0};

    while(r->running) {
        FD_ZERO(&fds); FD_SET(lfd, &fds);
        if (r->rtsp_client_fd > 0) FD_SET(r->rtsp_client_fd, &fds);
        int mx = (r->rtsp_client_fd > lfd) ? r->rtsp_client_fd : lfd;
        if (select(mx+1, &fds, NULL, NULL, &tv) <= 0) continue;

        if (FD_ISSET(lfd, &fds)) {
            struct sockaddr_in ca; socklen_t al=sizeof(ca);
            int fd = accept(lfd, (struct sockaddr*)&ca, &al);
            if (fd > 0) {
                if (r->rtsp_client_fd > 0) close(r->rtsp_client_fd);
                r->rtsp_client_fd = fd; g_client_fd = fd;
                r->state = RTSP_STATE_INIT;
            }
        }
        if (r->rtsp_client_fd > 0 && FD_ISSET(r->rtsp_client_fd, &fds)) {
            ssize_t n = recv(r->rtsp_client_fd, buf, sizeof(buf)-1, 0);
            if (n <= 0) {
                close(r->rtsp_client_fd); r->rtsp_client_fd = -1; g_client_fd = -1;
                r->state = RTSP_STATE_INIT;
                continue;
            }
            buf[n]=0; cseq=0;
            const char *cs = strstr(buf, "CSeq:");
            if (cs) sscanf(cs, "CSeq: %d", &cseq);
            char resp[4096];

            if (strncmp(buf, "OPTIONS", 7)==0) {
                snprintf(resp,sizeof(resp),"RTSP/1.0 200 OK\r\nCSeq: %d\r\nPublic: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n",cseq);
                rtsp_send(r->rtsp_client_fd, resp);
            } else if (strncmp(buf, "DESCRIBE", 8)==0) {
                const char *s=build_sdp(r);
                snprintf(resp,sizeof(resp),"RTSP/1.0 200 OK\r\nCSeq: %d\r\nContent-Type: application/sdp\r\nContent-Length: %zu\r\n\r\n%s",cseq,strlen(s),s);
                rtsp_send(r->rtsp_client_fd, resp);
            } else if (strncmp(buf, "SETUP", 5)==0) {
                int is_video = (strstr(buf, "track0") || strstr(buf, "video"));
                /* 优先检测 TCP interleaved */
                if (strstr(buf, "RTP/AVP/TCP") || strstr(buf, "rtp/avp/tcp")) {
                    int ch0=0, ch1=1;
                    const char *il = strstr(buf, "interleaved=");
                    if (il) sscanf(il, "interleaved=%d-%d", &ch0, &ch1);
                    if (is_video) {
                        r->client_interleaved_video_rtp  = ch0;
                        r->client_interleaved_video_rtcp = ch1;
                    } else {
                        r->client_interleaved_audio_rtp  = ch0;
                        r->client_interleaved_audio_rtcp = ch1;
                    }
                    r->cfg.transport_mode = RTSP_TRANSPORT_TCP;
                    snprintf(resp,sizeof(resp),
                        "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                        "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
                        "Session: %s\r\n\r\n",
                        cseq, ch0, ch1, r->session_id);
                } else {
                    /* UDP fallback */
                    int rtp=50000, rtcp=50001;
                    const char *cp = strstr(buf, "client_port=");
                    if (cp) sscanf(cp, "client_port=%d-%d", &rtp, &rtcp);
                    if (is_video) {
                        r->client_rtp_video_port=rtp; r->client_rtcp_video_port=rtcp;
                    } else {
                        r->client_rtp_audio_port=rtp; r->client_rtcp_audio_port=rtcp;
                    }
                    snprintf(resp,sizeof(resp),
                        "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                        "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n"
                        "Session: %s\r\n\r\n",
                        cseq, rtp, rtcp, r->session_id);
                }
                rtsp_send(r->rtsp_client_fd, resp); r->state=RTSP_STATE_READY;
            } else if (strncmp(buf, "PLAY", 4)==0) {
                struct sockaddr_in ca; socklen_t al=sizeof(ca);
                getpeername(r->rtsp_client_fd, (struct sockaddr*)&ca, &al);
                snprintf(r->client_ip,sizeof(r->client_ip),"%s",inet_ntoa(ca.sin_addr));
                snprintf(resp,sizeof(resp),"RTSP/1.0 200 OK\r\nCSeq: %d\r\nSession: %s\r\n\r\n",cseq,r->session_id);
                rtsp_send(r->rtsp_client_fd, resp); r->state=RTSP_STATE_PLAYING;
            } else if (strncmp(buf, "TEARDOWN", 8)==0) {
                snprintf(resp,sizeof(resp),"RTSP/1.0 200 OK\r\nCSeq: %d\r\nSession: %s\r\n\r\n",cseq,r->session_id);
                rtsp_send(r->rtsp_client_fd, resp); r->state=RTSP_STATE_READY;
            }
        }
    }
    if (r->rtsp_client_fd>0) close(r->rtsp_client_fd);
    g_client_fd=-1;
    return NULL;
}

/* === stream router === */
static VENC_CHN g_ve=-1; static AENC_CHN g_ae=-1; static MUX_GRP g_mx=-1;
static rtsp_handle_t *g_rt=NULL;

void rtsp_set_router_refs(int ve, int ae, int mx, rtsp_handle_t *r){
    g_ve=ve; g_ae=ae; g_mx=mx; g_rt=r;
}

static void *stream_router_thread(void *arg) {
    rtsp_handle_t *r = (rtsp_handle_t*)arg;
    VENC_STREAM_S vs; VENC_PACK_S pack[8];
    AUDIO_STREAM_S as;
    uint8_t *frame_buf = NULL;
    unsigned int fc = 0;
    unsigned int failc = 0;
    unsigned int ac = 0;

    frame_buf = (uint8_t *)malloc(RTSP_FRAME_BUF_SIZE);
    if (!frame_buf) {
        printf("[ROUTER] alloc frame buffer failed\n");
        return NULL;
    }
    printf("[ROUTER] start ve=%d ae=%d mx=%d audio=%d\n",
           g_ve, g_ae, g_mx, r->cfg.audio_enabled ? 1 : 0);

    while(r->running) {
        int ret;
        memset(&vs,0,sizeof(vs));
        memset(pack,0,sizeof(pack));
        vs.mPackCount = 8;
        vs.mpPack = pack;
        ret = AW_MPI_VENC_GetStream(g_ve, &vs, 50);
        if(ret == 0 && vs.mPackCount > 0) {
            fc++;
            if(g_mx>=0) AW_MPI_MUX_SendVideoStream(g_mx, &vs, 0, -1);
            for(unsigned int i=0;i<vs.mPackCount;i++) {
                unsigned int total_len = vs.mpPack[i].mLen0 + vs.mpPack[i].mLen1 + vs.mpPack[i].mLen2;
                unsigned int copied = 0;
                int is_h265_i = (vs.mpPack[i].mDataType.enH265EType == H265E_NALU_ISLICE);

                if (total_len == 0) {
                    continue;
                }
                if (total_len > RTSP_FRAME_BUF_SIZE) {
                    printf("[ROUTER] skip oversized frame len=%u idx=%u\n", total_len, i);
                    continue;
                }

                if (vs.mpPack[i].mLen0 > 0) {
                    memcpy(frame_buf + copied, vs.mpPack[i].mpAddr0, vs.mpPack[i].mLen0);
                    copied += vs.mpPack[i].mLen0;
                }
                if (vs.mpPack[i].mLen1 > 0) {
                    memcpy(frame_buf + copied, vs.mpPack[i].mpAddr1, vs.mpPack[i].mLen1);
                    copied += vs.mpPack[i].mLen1;
                }
                if (vs.mpPack[i].mLen2 > 0) {
                    memcpy(frame_buf + copied, vs.mpPack[i].mpAddr2, vs.mpPack[i].mLen2);
                    copied += vs.mpPack[i].mLen2;
                }

                if (is_h265_i && r->video_header_len > 0) {
                    rtsp_push_h265(r, r->video_header, r->video_header_len, vs.mpPack[i].mPTS);
                }
                rtsp_push_h265(r, frame_buf, copied, vs.mpPack[i].mPTS);
            }
            AW_MPI_VENC_ReleaseStream(g_ve, &vs);
            if((fc % 100) == 0) {
                printf("[ROUTER] ok=%u packs=%d pts=%lld len0=%d len1=%d len2=%d\n",
                       fc,
                       vs.mPackCount,
                       (long long)vs.mpPack[0].mPTS,
                       vs.mpPack[0].mLen0,
                       vs.mpPack[0].mLen1,
                       vs.mpPack[0].mLen2);
            }
        } else {
            failc++;
            if((failc % 20) == 0) {
                printf("[ROUTER] wait ret=%d ok=%u fail=%u\n", ret, fc, failc);
            }
        }

        if (g_ae >= 0) {
            int drain = 0;
            while (drain < 4 && r->running) {
                memset(&as, 0, sizeof(as));
                ret = AW_MPI_AENC_GetStream(g_ae, &as, 0);
                if (ret != SUCCESS) {
                    break;
                }
                if (as.pStream != NULL && as.mLen > 0) {
                    if (g_mx >= 0) {
                        /*
                         * Match the vendor avmuxer demo: video streamId stays on 0,
                         * while manually injected audio goes to streamId 1.
                         */
                        AW_MPI_MUX_SendAudioStream(g_mx, &as, 1, -1);
                    }
                    rtsp_push_aac(r, as.pStream, as.mLen, as.mTimeStamp);
                    ac++;
                }
                AW_MPI_AENC_ReleaseStream(g_ae, &as);
                drain++;
            }
            if (ac > 0 && (ac % 100) == 0) {
                printf("[ROUTER] audio_ok=%u last_len=%u last_pts=%llu\n",
                       ac, as.mLen, (unsigned long long)as.mTimeStamp);
            }
        }
    }
    free(frame_buf);
    printf("[ROUTER] exit, video_ok=%u audio_ok=%u fail=%u\n", fc, ac, failc);
    return NULL;
}

/* === public API === */
int rtsp_init(rtsp_handle_t **rr, const rtsp_config_t *c) {
    rtsp_handle_t *r=calloc(1,sizeof(*r)); if(!r)return -1;
    r->cfg=*c; r->state=RTSP_STATE_INIT; r->rtsp_client_fd=-1;
    r->rtp_seq_video=(unsigned short)(time(NULL)&0xFFFF);
    r->rtp_seq_audio=(unsigned short)((time(NULL)+10000)&0xFFFF);
    snprintf(r->session_id,sizeof(r->session_id),"%08lx",(long)(time(NULL)&0xFFFFFFFF));

    /* TCP interleaved channel 默认值 */
    r->client_interleaved_video_rtp  = 0;
    r->client_interleaved_video_rtcp = 1;
    r->client_interleaved_audio_rtp  = 2;
    r->client_interleaved_audio_rtcp = 3;
    /* 默认 TCP interleaved（可靠，过防火墙/ADB forward） */
    r->cfg.transport_mode = RTSP_TRANSPORT_TCP;

    r->rtsp_listen_fd=socket(AF_INET,SOCK_STREAM,0);
    r->rtp_video_fd=socket(AF_INET,SOCK_DGRAM,0);
    r->rtp_audio_fd=socket(AF_INET,SOCK_DGRAM,0);
    if(r->rtsp_listen_fd<0){free(r);return -1;}
    int opt=1; setsockopt(r->rtsp_listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in a; memset(&a,0,sizeof(a));
    a.sin_family=AF_INET; a.sin_addr.s_addr=INADDR_ANY; a.sin_port=htons(c->rtsp_port);
    if(bind(r->rtsp_listen_fd,(struct sockaddr*)&a,sizeof(a))<0){close(r->rtsp_listen_fd);free(r);return -1;}
    listen(r->rtsp_listen_fd,1);
    printf("[RTSP] TCP:%d (TCP-interleaved+UDP) ready\n",c->rtsp_port);
    *rr=r; return 0;
}

int rtsp_start(rtsp_handle_t *r) {
    if(!r||r->running)return -1;
    r->running=true;
    pthread_create(&r->rtsp_thread,NULL,rtsp_server_thread,r);
    pthread_create(&r->router_thread,NULL,stream_router_thread,r);
    return 0;
}

int rtsp_stop(rtsp_handle_t *r) {
    if(!r||!r->running)return 0;
    r->running=false;
    pthread_join(r->router_thread,NULL);
    pthread_join(r->rtsp_thread,NULL);
    r->state=RTSP_STATE_INIT; return 0;
}

int rtsp_deinit(rtsp_handle_t **rr) {
    if(!rr||!*rr)return 0;
    rtsp_handle_t *r=*rr;
    if(r->running)rtsp_stop(r);
    if(r->rtsp_client_fd>0)close(r->rtsp_client_fd);
    if(r->rtp_video_fd>=0)close(r->rtp_video_fd);
    if(r->rtp_audio_fd>=0)close(r->rtp_audio_fd);
    if(r->rtsp_listen_fd>=0)close(r->rtsp_listen_fd);
    g_ve=-1;g_ae=-1;g_mx=-1;g_rt=NULL;
    free(r);*rr=NULL; return 0;
}
