#ifndef _MP_RTP_H
#define _MP_RTP_H

#include "mp.h"

#define MP2T_PAYLOAD_TYPE                   33

#define RTP_NOTIFY_COUNT                    8
#define RTP_NOTIFY_PER_PKT                  2

#define BUFFER_TS_COUNT                     21
#define BRIDGED_TS_COUNT                    ((NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE) / MPEG_TS_PKT_SIZE)
#define BRIDGED_VS_FULL_COUNT               ((BUFFER_TS_COUNT * RTP_NOTIFY_PER_PKT) / BRIDGED_TS_COUNT)
#define NUM_TS_PKTS_PER_RTP_PKT(Adapter)    (Adapter->SupportBridging ? BRIDGED_TS_COUNT : (BUFFER_TS_COUNT * RTP_NOTIFY_PER_PKT)) 
#define RTP_SINK_BUFFER_SIZE(Adapter)       ((NUM_TS_PKTS_PER_RTP_PKT(Adapter) * MPEG_TS_PKT_SIZE) + RTP_HDR_SIZE)


typedef struct {
    DWORD rtp_sink_data_ready;
    DWORD rtp_sink_data_ready_was_running;
    DWORD rtp_sink_data_ready_from_timeout;
    DWORD rtp_sink_data_ready_not_from_timeout;
    DWORD reset_timeout;
    DWORD buffer_already_full;
    DWORD buffer_not_yet_full;
    DWORD buffer_now_full;
    DWORD buffer_full_and_too_soon;
    DWORD stall_50_usec;
    DWORD schedule_hw_timers;
    DWORD full_and_on_time;
    DWORD late_and_some_data;
    DWORD no_data_present;
    DWORD some_data_on_timeout;
    DWORD max_delta;
    LARGE_INTEGER last_timer_set;
    DWORD last_timer_val;
    LARGE_INTEGER last_send_time;

	//dpc
	DWORD mpeg_reader;
	DWORD not_setup;
    DWORD not_running;
    DWORD min_timer_went_off;
    DWORD max_timer_went_off;
    DWORD mpeg_delayed_for_min_timer;
	DWORD extra_data_in_dpc;
	DWORD timeout_while_hw_enabled;
	DWORD timeout_from_hw;
	//isr
	DWORD hw_timer_timed_out;
	DWORD hw_timer_didnt_timeout;
	DWORD hw_timer_non_zero_no_irq;
	DWORD hw_timer_non_zero_with_irq;
	DWORD hw_timer_mask_set;

    DWORD discontinuities;
    DWORD sent_too_fast;
} rtp_counters_t;

typedef struct _RTP_HEADER {
    /* byte 0 */
    unsigned char csrc_len:4;   /* expect 0 */
    unsigned char extension:1;  /* expect 1, see RTP_OP below */
    unsigned char padding:1;    /* expect 0 */
    unsigned char version:2;    /* expect 2 */
    /* byte 1 */
    unsigned char payload:7;    /* RTP_PAYLOAD_RTSP */
    unsigned char marker:1;     /* expect 1 */
    /* bytes 2, 3 */
    unsigned short seq_no;
    /* bytes 4-7 */
    unsigned int timestamp;
    /* bytes 8-11 */
    unsigned int ssrc;    /* stream number is used here. */
} RTP_HEADER, *PRTP_HEADER;

VOID
RtpSinkDataReady(
        PMP_MPEG_BUFFER MpegBuffer,
        BOOLEAN FromTimeout,
        int WhoCalled);

VOID
RtpFreeMem(
        PMP_ADAPTER Adapter);

VOID
RtpSetup(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen);

VOID
RtpInject(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen);

VOID
RtpDestroy(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen);

#endif
