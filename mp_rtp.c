#include "precomp.h"

#if DBG
#define _FILENUMBER     'PSTR'
#endif

#define SCRATCH_BUFFER_LENGTH 3948

#define MIN_WAIT_USEC(Adapter) (Adapter->SupportBridging ? (1000 / BRIDGED_VS_FULL_COUNT) : 1000)
#define MAX_WAIT_MSEC 20
#define LIMIT_MSEC 50
#define MP2T_TIMESTAMP_CLOCK 90000

#define MSEC_PER_SEC 1000
#define USECS_PER_MSEC 1000
#define SYSTEM_TIME_UNITS_PER_USEC 10

#if DBG
UCHAR ScratchBuffer[SCRATCH_BUFFER_LENGTH];

typedef struct _MPEG_FILE_CONTEXT {
    HANDLE hFile;
} MPEG_FILE_CONTEXT, *PMPEG_FILE_CONTEXT;
#endif

static int first_time = 1;

#if DBG
MPEG_FILE_CONTEXT MpegFiles[NUM_MPEG_STREAMS];
#endif

#if PRINT_RTP_STATS
rtp_counters_t counters[NUM_MPEG_STREAMS];
#endif

#if TRACK_SYNC
ULONG sync_errors[NUM_MPEG_STREAMS];
#endif

#if DBG
VOID
WriteMpegDataToFile(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer)
{
    IO_STATUS_BLOCK	iostatus;
    if( first_time ) {
        int i=0;
        first_time = 0;

        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

            DECLARE_UNICODE_STRING_SIZE(pathName, 255);
            OBJECT_ATTRIBUTES oa;
            NTSTATUS status = STATUS_SUCCESS;

            RtlUnicodeStringPrintf( &pathName, L"\\??\\C:\\mpeg%d.ts", i );

            InitializeObjectAttributes(&oa, &pathName,
                OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                NULL, NULL);

            status = ZwCreateFile(&MpegFiles[i].hFile, SYNCHRONIZE | GENERIC_WRITE, &oa, &iostatus, NULL, FILE_ATTRIBUTE_NORMAL,
                                0, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT|FILE_NON_DIRECTORY_FILE, NULL, 0);

            if( !NT_SUCCESS(status) ) {
                MpegFiles[i].hFile = NULL;
                DBGPRINT( MP_ERROR, ("Failed to create file %d\n", i ));
            }
        }
    }

    if( MpegFiles[MpegBuffer->TunerIndex].hFile ) {

        ULONG AmountRead = MpegBufferRead( MpegBuffer, ScratchBuffer, SCRATCH_BUFFER_LENGTH );

        if( AmountRead > 0 ) {
            NTSTATUS status = ZwWriteFile(MpegFiles[MpegBuffer->TunerIndex].hFile,
                    NULL,//   Event,
                    NULL,// PIO_APC_ROUTINE  ApcRoutine
                    NULL,// PVOID  ApcContext
                    &iostatus,
                    ScratchBuffer,
                    AmountRead,
                    0, // ByteOffset
                    NULL // Key
                    );

            if( !NT_SUCCESS(status) ) {
                DBGPRINT( MP_ERROR, ("Failed to write %d to file %d\n", AmountRead, MpegBuffer->TunerIndex ));
            }

        } else {
            DBGPRINT( MP_WARN, ("MpegBufferRead gave 0 when expecting data\n"));
        }
    }

}
#endif

ULONG
RtpSinkDataBytesLeft(
    PMP_ADAPTER Adapter,
    PMP_RTP_STATE RtpSink)
{
    return RTP_SINK_BUFFER_SIZE(Adapter) - RtpSink->BufferUsed;
}

ULONG CLAMP_ULONG( ULONG min, ULONG value, ULONG max )
{
	if (value > max) {
		return max;
	}
	if (value < min) {
		return min;
	}
	return value;
}

USHORT IpHdrCkSum( PUCHAR ip, LONG len )
{
    PUSHORT buf = (PUSHORT)ip;
    LONG sum = 0;

    while( len > 1 ) {
        sum += *buf;
        buf++;
        if( sum & 0x80000000 )   /* if high order bit set, fold */
            sum = (sum & 0xFFFF) + (sum >> 16);
        len -= 2;
    }

    if(len)       /* take care of left over byte */
        sum += (unsigned short) *(unsigned char *)buf;

    while( sum >> 16 )
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (USHORT)~sum;
}
#define RTP_PACKET_SIZE_PLUS(data_size) (RTP_HDR_SIZE + data_size)
#define UDP_PACKET_SIZE_PLUS(data_size) (RTP_PACKET_SIZE_PLUS(data_size) + 8 /* udp */)
#define IP_PACKET_SIZE_PLUS(data_size) (UDP_PACKET_SIZE_PLUS(data_size) + 20 /* ip */)
#define ETH_PACKET_SIZE_PLUS(data_size) (IP_PACKET_SIZE_PLUS(data_size) + 14 /* eth */)

#define ETH_PACKET_SIZE(MpegBuffer) (MpegBuffer->RtpState.BufferUsed + 14 /* eth */ + 20 /* ip */ + 8 /* udp */)
#define IP_PACKET_SIZE(MpegBuffer) (MpegBuffer->RtpState.BufferUsed + 20 /* ip */ + 8 /* udp */)
#define UDP_PACKET_SIZE(MpegBuffer) (MpegBuffer->RtpState.BufferUsed + 8 /* udp */)
#define RTP_PACKET_SIZE(MpegBuffer) (MpegBuffer->RtpState.BufferUsed)

VOID
RtpBufferPacker(
        PMP_ADAPTER Adapter,
        PUCHAR DestBuffer,
        ULONG Length,
        PVOID Context)
{
    PMP_MPEG_BUFFER MpegBuffer = Context;
    NdisMoveMemory( DestBuffer, MpegBuffer->RtpState.RemoteHwAddr, 6 );
    NdisMoveMemory( DestBuffer + 6, MpegBuffer->RtpState.LocalHwAddr, 6 );
    //ether type = IP
    DestBuffer[12] = 0x08;
    DestBuffer[13] = 0x00;
    DestBuffer += 14;

    //ipv4 header
    DestBuffer[0] = 0x45;
    DestBuffer[1] = 0x00;

    /* total length */
    if (MpegBuffer->RtpState.InjectMode) {
        DBGPRINT( MP_WARN, ("Inject Done: Inject Mode\n"));
        *((PUSHORT)&DestBuffer[2]) = RtlUshortByteSwap( IP_PACKET_SIZE_PLUS( MPEG_TS_PKT_SIZE ) );
    } else {
        *((PUSHORT)&DestBuffer[2]) = RtlUshortByteSwap( IP_PACKET_SIZE(MpegBuffer) );
    }

    /* identification */
    *((PUSHORT)&DestBuffer[4]) = RtlUshortByteSwap( MpegBuffer->RtpState.SeqNo );

    /* flags, fragment offset */
    DestBuffer[6] = 0;
    DestBuffer[7] = 0;
    /* ttl */
    DestBuffer[8] = 10;
    /* protocol = UDP */
    DestBuffer[9] = 0x11;

    /* checksum */
    *((PUSHORT)&DestBuffer[10]) = 0x0000;

    *((PULONG)&DestBuffer[12]) = MpegBuffer->RtpState.LocalIpAddr;
    *((PULONG)&DestBuffer[16]) = MpegBuffer->RtpState.RemoteIpAddr;

    /* go back and calc chksum */
    {
        USHORT IpChkSum = IpHdrCkSum( DestBuffer, 20 );
        *((PUSHORT)&DestBuffer[10]) = IpChkSum;
    }

    DestBuffer += 20;

    //udp header
    *((PUSHORT)&DestBuffer[0]) = MpegBuffer->RtpState.LocalPort;
    *((PUSHORT)&DestBuffer[2]) = MpegBuffer->RtpState.RemotePort;
    if (MpegBuffer->RtpState.InjectMode) {
        *((PUSHORT)&DestBuffer[4]) = RtlUshortByteSwap( (USHORT)UDP_PACKET_SIZE_PLUS(MPEG_TS_PKT_SIZE) );
    } else {
        *((PUSHORT)&DestBuffer[4]) = RtlUshortByteSwap( (USHORT)UDP_PACKET_SIZE(MpegBuffer) );
    }
    //checksum
    *((PUSHORT)&DestBuffer[6]) = 0x0000;
    DestBuffer += 8;

    if (MpegBuffer->RtpState.InjectMode) {
        NdisMoveMemory( DestBuffer, MpegBuffer->RtpState.InjectBuffer, RTP_PACKET_SIZE_PLUS(MPEG_TS_PKT_SIZE) );
    } else {
        NdisMoveMemory( DestBuffer, MpegBuffer->RtpState.Buffer, RTP_PACKET_SIZE(MpegBuffer) );
        MpegBuffer->RtpState.BufferUsed = RTP_HDR_SIZE;
    }
}

VOID
RtpSendPacket(
    PMP_RTP_STATE RtpSink )
{
    PMP_MPEG_BUFFER MpegBuffer = CONTAINING_RECORD( RtpSink, MP_MPEG_BUFFER, RtpState );
    PMP_ADAPTER Adapter = MpegBuffer->Adapter;
    PRTP_HEADER r = NULL;
    LARGE_INTEGER Now = GetTime(Adapter);
    KIRQL OldIrql = 0;

    if (MpegBuffer->RtpState.InjectMode) {
        r = (PRTP_HEADER)RtpSink->InjectBuffer;
    } else {
        r = (PRTP_HEADER)RtpSink->Buffer;
    }

#if PRINT_RTP_STATS
    if((Now.QuadPart - counters[MpegBuffer->TunerIndex].last_send_time.QuadPart) < (LONGLONG)MIN_WAIT_USEC(Adapter)) {
        counters[MpegBuffer->TunerIndex].sent_too_fast++;
    }

    counters[MpegBuffer->TunerIndex].last_send_time = Now;
#endif

    //other fields don't change
    r->seq_no = RtlUshortByteSwap( RtpSink->SeqNo++ );
    r->timestamp = RtlUlongByteSwap( (ULONG)( Now.QuadPart / MP2T_TIMESTAMP_CLOCK ) );
    r->ssrc = RtlUlongByteSwap( RtpSink->Ssrc );

    MP_ACQUIRE_SPIN_LOCK( &Adapter->RcvLock, READER_AT_DISPATCH );
    OldIrql = Adapter->RcvLock.OldIrql;

    if (MpegBuffer->RtpState.InjectMode) {
        MpHandleRecvBuffer( Adapter, RtpBufferPacker, ETH_PACKET_SIZE_PLUS(MPEG_TS_PKT_SIZE), MpegBuffer );
    } else {
        MpHandleRecvBuffer( Adapter, RtpBufferPacker, ETH_PACKET_SIZE(MpegBuffer), MpegBuffer );
    }
    
    Adapter->RcvLock.OldIrql = OldIrql;
    MP_RELEASE_SPIN_LOCK( &Adapter->RcvLock, READER_AT_DISPATCH );

    if( RtpSink->PrevTime.QuadPart ) {
        RtpSink->RtpTimeDelta = (ULONG)(Now.QuadPart - RtpSink->PrevTime.QuadPart);
    }

    RtpSink->PrevTime = Now;
}

static VOID
RtpCountSync(
        PMP_MPEG_BUFFER MpegBuffer,
        PUCHAR Buffer,
        ULONG Len)
{
#if TRACK_SYNC
    ULONG pkts = Len/188;
    ULONG i = 0;
    for (i = 0; i < pkts; i++)
    {
        PUCHAR Pkt = Buffer + (188 * i);
        if (Pkt[0] != 0x47) {
            sync_errors[MpegBuffer->TunerIndex]++;
            if (sync_errors[MpegBuffer->TunerIndex] % 10000 == 0) {
                DBGPRINT( MP_WARN, ("[%d] sync_errors %d",
                            MpegBuffer->TunerIndex,
                            sync_errors[MpegBuffer->TunerIndex]) );
                
            }
        }
    }
#endif //TRACK_SYNC
}

ULONG
RtpSinkMpegRead(
        PMP_RTP_STATE RtpSink,
        PUCHAR Buffer,
        ULONG Len)
{
    ULONG ReadSize = 0;
    PMP_MPEG_BUFFER MpegBuffer = CONTAINING_RECORD( RtpSink, MP_MPEG_BUFFER, RtpState );
    ReadSize = MpegBufferRead( MpegBuffer, Buffer, Len );
    RtpCountSync( MpegBuffer, Buffer, ReadSize );
    return MIN( ReadSize, Len );
}

VOID
RtpSinkReadData(
    PMP_ADAPTER Adapter,
    PMP_RTP_STATE RtpSink)
{
    ULONG BytesLeft = RtpSinkDataBytesLeft( Adapter, RtpSink );
    ULONG NumTsPkts = BytesLeft / MPEG_TS_PKT_SIZE;
    PUCHAR UnusedBufferStart = RtpSink->Buffer + RtpSink->BufferUsed;

    if( NumTsPkts ) {
        ULONG ReadSize = RtpSinkMpegRead( RtpSink, UnusedBufferStart, MPEG_TS_PKT_SIZE*NumTsPkts );
        RtpSink->BufferUsed += ReadSize;
    }
}

VOID
RtpSinkDataReady(
        PMP_MPEG_BUFFER MpegBuffer,
        BOOLEAN FromTimeout,
        int WhoCalled)
{
    PMP_ADAPTER Adapter = MpegBuffer->Adapter;
    BOOLEAN ResetTimeout = FALSE;
    PMP_RTP_STATE RtpSink = &MpegBuffer->RtpState;
#if PRINT_RTP_STATS
    BOOLEAN WasRunning = RtpSink->Running;

    counters[MpegBuffer->TunerIndex].rtp_sink_data_ready++;
    if (WasRunning) {
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_was_running++;
    }

    if (counters[MpegBuffer->TunerIndex].rtp_sink_data_ready % 20000 == 0) {
        DBGPRINT( MP_WARN, ("[%d] rtp_sink_data_ready %d\n "
        "\t\trtp_sink_data_ready_was_running %d\n "
        "\t\trtp_sink_data_ready_from_timeout %d\n "
        "\t\trtp_sink_data_ready_not_from_timeout %d\n "
        "\t\treset_timeout %d\n "
        "\t\tbuffer_already_full %d\n "
        "\t\tbuffer_not_yet_full %d\n "
        "\t\tbuffer_now_full %d\n "
        "\t\tbuffer_full_and_too_soon %d\n "
        "\t\tmax_delta %d\n ",

        MpegBuffer->TunerIndex,
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready,
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_was_running,
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_from_timeout,
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_not_from_timeout,
        counters[MpegBuffer->TunerIndex].reset_timeout,
        counters[MpegBuffer->TunerIndex].buffer_already_full,
        counters[MpegBuffer->TunerIndex].buffer_not_yet_full,
        counters[MpegBuffer->TunerIndex].buffer_now_full,
        counters[MpegBuffer->TunerIndex].buffer_full_and_too_soon,
        counters[MpegBuffer->TunerIndex].max_delta
        ) );

        counters[MpegBuffer->TunerIndex].max_delta = 0;

        DBGPRINT( MP_WARN, ("\t\tstall_50_usec %d\n "
        "\t\tschedule_hw_timers %d\n "
        "\t\tfull_and_on_time %d\n "
        "\t\tlate_and_some_data %d\n "
        "\t\tno_data_present %d\n "
        "\t\tsome_data_on_timeout %d \n "
		"\t\tmpeg_reader %d \n "
        "\t\tnot_running %d \n "
        "\t\tmin_timer_went_off %d \n "
        "\t\tmpeg_delayed_for_min_timer %d \n "
		"\t\textra_data_in_dpc %d \n "
		"\t\tmax_timer_went_off %d \n ",

        counters[MpegBuffer->TunerIndex].stall_50_usec,
        counters[MpegBuffer->TunerIndex].schedule_hw_timers,
        counters[MpegBuffer->TunerIndex].full_and_on_time,
        counters[MpegBuffer->TunerIndex].late_and_some_data,
        counters[MpegBuffer->TunerIndex].no_data_present,
        counters[MpegBuffer->TunerIndex].some_data_on_timeout,
		counters[MpegBuffer->TunerIndex].mpeg_reader,
        counters[MpegBuffer->TunerIndex].not_running,
        counters[MpegBuffer->TunerIndex].min_timer_went_off,
        counters[MpegBuffer->TunerIndex].mpeg_delayed_for_min_timer,
		counters[MpegBuffer->TunerIndex].extra_data_in_dpc,
		counters[MpegBuffer->TunerIndex].max_timer_went_off
        ) );

		DBGPRINT( MP_WARN, (
		"\t\ttimeout_while_hw_enabled %d\n "
		"\t\ttimeout_from_hw %d\n "
		"\t\thw_timer_timed_out %d\n "
        "\t\thw_timer_didnt_timeout %d\n "
        "\t\thw_timer_non_zero_no_irq %d\n "
        "\t\thw_timer_non_zero_with_irq %d\n "
        "\t\thw_timer_mask_set %d\n ",

		counters[MpegBuffer->TunerIndex].timeout_while_hw_enabled,
		counters[MpegBuffer->TunerIndex].timeout_from_hw,
		counters[MpegBuffer->TunerIndex].hw_timer_timed_out,
		counters[MpegBuffer->TunerIndex].hw_timer_didnt_timeout,
    	counters[MpegBuffer->TunerIndex].hw_timer_non_zero_no_irq,
	    counters[MpegBuffer->TunerIndex].hw_timer_non_zero_with_irq,
    	counters[MpegBuffer->TunerIndex].hw_timer_mask_set
        ) );

		DBGPRINT( MP_WARN, ("\t\tHi Drop Count %d\n "
        "\t\tDiscontinuities %d\n "
        "\t\tSent too fast %d\n "
        "\t\trecv resource error %d\n "
        "\t\tcur rfd %d\n ",
                    
        Adapter->DropCount[MpegBuffer->TunerIndex],
        MpegBuffer->DiscontinuityCount,
        counters[MpegBuffer->TunerIndex].sent_too_fast,
        Adapter->RcvResourceErrors,
        Adapter->CurrNumRfd
        ) );


    }
#endif

    RtpSink->Running = TRUE;

    if( !FromTimeout ) {
        //read as much data as we can without blocking
        //then check to see if we should send what we have, or wait for more
        BOOLEAN Full;
        BOOLEAN TooSoon;
        BOOLEAN TooLate;
        BOOLEAN SomeData;
        BOOLEAN PastLimit;

        LARGE_INTEGER Now = GetTime(Adapter);
        ULONG DeltaUsec = 0;

        ULONG BytesLeft = RtpSinkDataBytesLeft( Adapter, RtpSink );

#if PRINT_RTP_STATS
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_not_from_timeout++;
#endif

        if( BytesLeft < MPEG_TS_PKT_SIZE ) {
            //buffer is full already

#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].buffer_already_full++;
#endif
        } else {
            //read the data

#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].buffer_not_yet_full++;
#endif

            RtpSinkReadData( Adapter, RtpSink );
        }

        if( RtpSink->PrevTime.QuadPart == 0 ) {
            DeltaUsec = ( MAX_WAIT_MSEC * USECS_PER_MSEC );
            if(DeltaUsec == 0) {
                DBGPRINT( MP_WARN, ("\t\t1 %lld %lld WhoCalled == %d\n", Now.QuadPart, RtpSink->PrevTime.QuadPart, WhoCalled) );
            }
        } else {

            DeltaUsec = (ULONG)(Now.QuadPart - RtpSink->PrevTime.QuadPart);
#if PRINT_RTP_STATS
            if (DeltaUsec > counters[MpegBuffer->TunerIndex].max_delta) {
                counters[MpegBuffer->TunerIndex].max_delta = DeltaUsec;
            }
            if( DeltaUsec > (USECS_PER_MSEC*MSEC_PER_SEC) ) {
                DBGPRINT( MP_WARN, ("\t\tWhere was I? %lu", DeltaUsec) );
                MpegBufferDump( MpegBuffer );                
            }
#endif

            DeltaUsec = CLAMP_ULONG( 0, DeltaUsec, LIMIT_MSEC * USECS_PER_MSEC );
            if(DeltaUsec == 0) {
                DBGPRINT( MP_WARN, ("\t\t2 %lld %lld WhoCalled == %d\n", Now.QuadPart, RtpSink->PrevTime.QuadPart, WhoCalled) );
            }
        }

        BytesLeft = RtpSinkDataBytesLeft( Adapter, RtpSink );

        Full = BytesLeft < MPEG_TS_PKT_SIZE;
        TooSoon = DeltaUsec < (ULONG)(MIN_WAIT_USEC(Adapter));
        TooLate = DeltaUsec >= (MAX_WAIT_MSEC * USECS_PER_MSEC);
        SomeData = RtpSink->BufferUsed > RTP_HDR_SIZE;
        PastLimit = DeltaUsec > LIMIT_MSEC * USECS_PER_MSEC;

        if( Full ) {
#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].buffer_now_full++;
#endif

            if( TooSoon ) {

                LONG TimeToSleep = ( MIN_WAIT_USEC(Adapter) ) - DeltaUsec;

#if PRINT_RTP_STATS
                counters[MpegBuffer->TunerIndex].buffer_full_and_too_soon++;
#endif

                if( TimeToSleep <= 50 ) {
#if PRINT_RTP_STATS
                    counters[MpegBuffer->TunerIndex].stall_50_usec++;
#endif
#if USE_STALL_EXECUTION
                    KeStallExecutionProcessor( TimeToSleep );
#endif
                    RtpSendPacket( RtpSink );
                    ResetTimeout = TRUE;
                } else {
#if PRINT_RTP_STATS
                    counters[MpegBuffer->TunerIndex].schedule_hw_timers++;
#endif

                    {
                        static BOOLEAN first = TRUE;
                        if( first ) {
                            first = FALSE;
                            DBGPRINT( MP_WARN, ("\t\tSetting timer for %d\n", TimeToSleep ) );
                        }
                    }

#if 0
                    {
                        ULONG curvalue = ctn91xx_read32( Adapter->TimerBase, TIMER_TIMEOUT( MpegBuffer->TunerIndex ) );
                        if(curvalue != 0 ) {
                            DBGPRINT( MP_WARN, ("\t\tTimer(%d-%d-%d) non-zero %d (WasRunning %d, CurRunning %d)\n", MpegBuffer->TunerIndex,
                                        ctn91xx_read8(Adapter->TimerBase, TIMER_ENABLE( MpegBuffer->TunerIndex ) ),
                                        WhoCalled,
                                        curvalue, WasRunning, RtpSink->Running ) );
                        } else if(!WasRunning) {
                            DBGPRINT( MP_WARN, ("\t\tGot here when we weren't running? How?\n") );
                        }
                    }
#endif

                    RtpSink->Running = FALSE;

                    ctn91xx_write32(
                            Adapter->TicksPerUS * (TimeToSleep),
                            Adapter->TimerBase,
                            TIMER_TIMEOUT( MpegBuffer->TunerIndex ) );

                    ctn91xx_write8( 1, Adapter->TimerBase,
                            TIMER_ENABLE( MpegBuffer->TunerIndex ) );
                }

            } else {
                //We have full buffer and it is time
#if PRINT_RTP_STATS
                counters[MpegBuffer->TunerIndex].full_and_on_time++;
#endif

                RtpSendPacket( RtpSink );
                ResetTimeout = TRUE;
            }
        } else if( TooLate && SomeData ) {
#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].late_and_some_data++;
#endif

            RtpSendPacket( RtpSink );
            ResetTimeout = TRUE;
        } else {
            //we don't have enough data yet
#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].no_data_present++;
#endif
        }
    } else {
        int from_hw = ctn91xx_read8( Adapter->TimerBase, TIMER_IRQ_REG( MpegBuffer->TunerIndex ) );
        //from timeout
#if PRINT_RTP_STATS
        counters[MpegBuffer->TunerIndex].rtp_sink_data_ready_from_timeout++;
#endif
		if(ctn91xx_read8( Adapter->TimerBase, TIMER_ENABLE( MpegBuffer->TunerIndex ) ) ) {
#if PRINT_RTP_STATS
			counters[MpegBuffer->TunerIndex].timeout_while_hw_enabled++;
#endif
			if(ctn91xx_read8( Adapter->TimerBase, TIMER_IRQ_REG( MpegBuffer->TunerIndex ) ) ) {
#if PRINT_RTP_STATS
				counters[MpegBuffer->TunerIndex].timeout_from_hw++;
#endif
			}
		}

        //clear timer interrupt if there is one
        ctn91xx_write8( 0, Adapter->TimerBase,
                TIMER_ENABLE( MpegBuffer->TunerIndex ) );
        ctn91xx_write8( 0, Adapter->TimerBase, TIMER_IRQ_REG( MpegBuffer->TunerIndex ) );
        ctn91xx_write8( 0, Adapter->TimerBase, TIMER_MASK( MpegBuffer->TunerIndex ) );

        //clear timer interrupt if there is one
        ctn91xx_write8( 0, Adapter->TimerBase,
                MAX_TIMER_ENABLE( MpegBuffer->TunerIndex ) );
        ctn91xx_write8( 0, Adapter->TimerBase, MAX_TIMER_IRQ_REG( MpegBuffer->TunerIndex ) );
        ctn91xx_write8( 0, Adapter->TimerBase, MAX_TIMER_MASK( MpegBuffer->TunerIndex ) );

        if( RtpSink->BufferUsed > RTP_HDR_SIZE ) {
            //we have some data, send it
#if PRINT_RTP_STATS
            counters[MpegBuffer->TunerIndex].some_data_on_timeout++;
#endif

            RtpSendPacket( RtpSink );
            ResetTimeout = TRUE;
        } else if(from_hw) {
            //DBGPRINT( MP_WARN, ("Why did we get a HW interrupt with no data?\n"));
        }
    }

    if( ResetTimeout ) {

        ctn91xx_write32(
                Adapter->TicksPerUS * (USECS_PER_MSEC * MAX_WAIT_MSEC),
                Adapter->TimerBase,
                MAX_TIMER_TIMEOUT( MpegBuffer->TunerIndex ) );

        ctn91xx_write8( 1, Adapter->TimerBase,
                MAX_TIMER_ENABLE( MpegBuffer->TunerIndex ) );
#if PRINT_RTP_STATS
        counters[MpegBuffer->TunerIndex].reset_timeout++;
#endif
    }
}

VOID
RtpFreeMem(
        PMP_ADAPTER Adapter)
{
    int i;
    for( i=0; i<NUM_MPEG_STREAMS; i++ ) {
        PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];

        MpegBuffer->RtpState.Running = FALSE;
        MpegBuffer->RtpState.GotSetup = FALSE;
        MP_FREEMEM( MpegBuffer->RtpState.Buffer );

#if DBG
        if( MpegFiles[i].hFile ) {
            ZwClose( MpegFiles[i].hFile );
            MpegFiles[i].hFile = NULL;
        }
#endif
    }
}

VOID
RtpSetup(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen)
{
    ULONG TunerIndex;
    PMP_MPEG_BUFFER MpegBuffer;

    if( MsgLen < SIZEOF_CONTROL_MSG_RTP_SETUP ) {
        DBGPRINT( MP_WARN, ("MsgLen %d too small\n", MsgLen));
        return;
    }

    TunerIndex = RtlUlongByteSwap( *((PULONG)&Msg[0]) );

    if( TunerIndex >= NUM_MPEG_STREAMS ) {
        DBGPRINT( MP_WARN, ("TunerIndex %d was too large\n", TunerIndex));
        return;
    }

    MpegBuffer = &Adapter->MpegBuffers[TunerIndex];
    Msg += 4;

    NdisMoveMemory( MpegBuffer->RtpState.RemoteHwAddr, Msg, 6 );
    Msg += 6;

    DBGPRINT( MP_TRACE, ("\tremote hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
                MpegBuffer->RtpState.RemoteHwAddr[0] & 0xff,
                MpegBuffer->RtpState.RemoteHwAddr[1] & 0xff,
                MpegBuffer->RtpState.RemoteHwAddr[2] & 0xff,
                MpegBuffer->RtpState.RemoteHwAddr[3] & 0xff,
                MpegBuffer->RtpState.RemoteHwAddr[4] & 0xff,
                MpegBuffer->RtpState.RemoteHwAddr[5] & 0xff));

    NdisMoveMemory( MpegBuffer->RtpState.LocalHwAddr, Msg, 6 );
    Msg += 6;

    DBGPRINT( MP_TRACE, ("\tlocal hwaddr %02x:%02x:%02x:%02x:%02x:%02x\n",
                MpegBuffer->RtpState.LocalHwAddr[0] & 0xff,
                MpegBuffer->RtpState.LocalHwAddr[1] & 0xff,
                MpegBuffer->RtpState.LocalHwAddr[2] & 0xff,
                MpegBuffer->RtpState.LocalHwAddr[3] & 0xff,
                MpegBuffer->RtpState.LocalHwAddr[4] & 0xff,
                MpegBuffer->RtpState.LocalHwAddr[5] & 0xff));

    MpegBuffer->RtpState.RemoteIpAddr = *((PULONG)&Msg[0]);
    Msg += 4;

    DBGPRINT( MP_TRACE, ("\tremote ipaddr %08x\n", MpegBuffer->RtpState.RemoteIpAddr));

    MpegBuffer->RtpState.LocalIpAddr = *((PULONG)&Msg[0]);
    Msg += 4;

    DBGPRINT( MP_TRACE, ("\tlocal ipaddr %08x\n", MpegBuffer->RtpState.LocalIpAddr));

    MpegBuffer->RtpState.RemotePort = *((PUSHORT)&Msg[0]);
    Msg += 2;

    DBGPRINT( MP_TRACE, ("\tremote port %04x\n", MpegBuffer->RtpState.RemotePort));

    MpegBuffer->RtpState.LocalPort = *((PUSHORT)&Msg[0]);
    Msg += 2;

    DBGPRINT( MP_TRACE, ("\tlocal port %04x\n", MpegBuffer->RtpState.LocalPort));

    MP_ACQUIRE_SPIN_LOCK( &Adapter->RtpLock, TRUE );
    MpegBufferLock( MpegBuffer );

    if( !MpegBuffer->RtpState.GotSetup ) {

        PRTP_HEADER r = NULL;


        MpegBuffer->RtpState.BufferUsed = RTP_HDR_SIZE;

        r = (PRTP_HEADER)MpegBuffer->RtpState.Buffer;
        r->version = 2;
        r->padding = 0;
        r->extension = 0;
        r->csrc_len = 0;
        r->marker = 0;
        r->payload = MP2T_PAYLOAD_TYPE;

        ASSERT( sizeof( RTP_HEADER ) == RTP_HDR_SIZE );

        MpegBuffer->ReadIndex = MpegBuffer->NotifyIndex;
        MpegBuffer->RtpState.GotSetup = TRUE;
        MpegBuffer->RtpState.Running = TRUE;
    }

    MpegBufferUnlock( MpegBuffer );
    MP_RELEASE_SPIN_LOCK( &Adapter->RtpLock, TRUE );
}

VOID
RtpInject(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen)
{
    ULONG TunerIndex;
    USHORT InjectPid;
    PMP_MPEG_BUFFER MpegBuffer;

    if( MsgLen < SIZEOF_CONTROL_MSG_PBDA_TAG_TABLE ) {
        DBGPRINT( MP_WARN, ("MsgLen %d was too small\n", MsgLen));
        return;
    }

    TunerIndex = RtlUlongByteSwap( *((PULONG)&Msg[0]) );

    if( TunerIndex >= NUM_MPEG_STREAMS ) {
        DBGPRINT( MP_WARN, ("TunerIndex %d was too large\n", TunerIndex));
        return;
    }

    MpegBuffer = &Adapter->MpegBuffers[TunerIndex];

    InjectPid = RtlUshortByteSwap( *((PUSHORT)&Msg[5]) );
    InjectPid &= 0x1FFF;

    DBGPRINT( MP_WARN, ("Inject: TunerIndex %d Pid %#04x\n", TunerIndex, InjectPid));

    MP_ACQUIRE_SPIN_LOCK( &MpegBuffer->Adapter->RtpLock, TRUE );
    MpegBufferLock( MpegBuffer );

    MpegBuffer->RtpState.InjectMode = 1;

    if (MpegBuffer->RtpState.GotSetup) {
        PRTP_HEADER r = NULL;

        r = (PRTP_HEADER)MpegBuffer->RtpState.InjectBuffer;
        r->version = 2;
        r->padding = 0;
        r->extension = 0;
        r->csrc_len = 0;
        r->marker = 0;
        r->payload = MP2T_PAYLOAD_TYPE;

        NdisMoveMemory(MpegBuffer->RtpState.InjectBuffer + RTP_HDR_SIZE, Msg + 4, MPEG_TS_PKT_SIZE);
    } else {
        DBGPRINT( MP_WARN, ("Dropping insert on %d as it is not running", TunerIndex));
    }

    RtpSendPacket( &MpegBuffer->RtpState );

    MpegBuffer->RtpState.InjectMode = 0;

    MpegBufferUnlock( MpegBuffer );
    MP_RELEASE_SPIN_LOCK( &MpegBuffer->Adapter->RtpLock, TRUE );

    DBGPRINT( MP_WARN, ("Inject Done: TunerIndex %d Pid %#04x\n", TunerIndex, InjectPid));
}

VOID
RtpDestroy(
        PMP_ADAPTER Adapter,
        PUCHAR Msg,
        ULONG MsgLen)
{
    ULONG TunerIndex;
    PMP_MPEG_BUFFER MpegBuffer;

    if( MsgLen < SIZEOF_CONTROL_MSG_RTP_DESTROY ) {
        DBGPRINT( MP_WARN, ("MsgLen %d was too small\n", MsgLen));
        return;
    }

    TunerIndex = RtlUlongByteSwap( *((PULONG)&Msg[0]) );

    if( TunerIndex >= NUM_MPEG_STREAMS ) {
        DBGPRINT( MP_WARN, ("TunerIndex %d was too large\n", TunerIndex));
        return;
    }

    MpegBuffer = &Adapter->MpegBuffers[TunerIndex];

    MP_ACQUIRE_SPIN_LOCK( &Adapter->RtpLock, TRUE );

    MpegBuffer->RtpState.Running = FALSE;
    MpegBuffer->RtpState.GotSetup = FALSE;
    
    NdisZeroMemory( MpegBuffer->RtpState.RemoteHwAddr,
        sizeof(MpegBuffer->RtpState.RemoteHwAddr) );
    NdisZeroMemory( MpegBuffer->RtpState.LocalHwAddr,
        sizeof(MpegBuffer->RtpState.RemoteHwAddr) );
    MpegBuffer->RtpState.RemoteIpAddr = 0;
    MpegBuffer->RtpState.LocalIpAddr = 0;
    MpegBuffer->RtpState.RemotePort = 0;
    MpegBuffer->RtpState.LocalPort = 0;

    MP_RELEASE_SPIN_LOCK( &Adapter->RtpLock, TRUE );
}

