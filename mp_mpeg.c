#include "precomp.h"

#if DBG
#define _FILENUMBER     'GEPM'
#endif

#if TRACK_PID_CC_MPEG
typedef struct {
    ULONG last_cc;
    ULONG discontinuities;
    ULONG count;
} pid_cc_t;

pid_cc_t track_pid_cc_notify[6][0xffff];
#endif

VOID
MpegBufferUnmapPage(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG NotifyIndex,
        USHORT BytesPerPage);

VOID
MpegInitDma(
    PMP_ADAPTER Adapter
    )
{
    int i;
    //
    // Setup the mpeg dma engine
    //

    MpegDeinitDma( Adapter );

    // 20 ms in 27mhz cycles
    Adapter->DmaTimeout = 540000;

    for( i=0; i<NUM_MPEG_STREAMS; i++ ) {
        PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];
        ULONG NextWritePageIndex;

        MP_ACQUIRE_SPIN_LOCK( &MpegBuffer->Lock, FALSE );

        MpegBuffer->TotalNotifies = 0;

        ctn91xx_write8( (UCHAR)Adapter->NumPagesPerBank, Adapter->DmaBase, MPEG_DMA_PAGES_PER_BANK_BASE + (i*4) );

        MpegBuffer->NotifyIndex = MpegBuffer->WriteIndex = MpegBuffer->ReadIndex = 0;

        MpegBufferMapBank( Adapter, MpegBuffer, 0, TRUE );

        NextWritePageIndex = WRAPPED_PAGE_INDEX( MpegBuffer->WriteIndex + Adapter->NumPagesPerBank );
        MpegBuffer->WriteIndex = NextWritePageIndex;
        MpegBufferMapBank( Adapter, MpegBuffer, 1, TRUE );

        MpegBuffer->WriteIndex = 0;

        MP_RELEASE_SPIN_LOCK( &MpegBuffer->Lock, FALSE );
        
        ctn91xx_write16( DEFAULT_MPEG_BYTES_PER_PAGE, Adapter->DmaBase, MPEG_DMA_BYTES_PER_PAGE_BASE + (i*2));
        ctn91xx_write8( DEFAULT_NUM_PAGES_PER_NOTIFY, Adapter->DmaBase, MPEG_DMA_PAGES_PER_NOTIFY_BASE + (i*8));

        ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (i*2) );
        ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (i*2) );
    }

    ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_NOTIFY_ENABLE );
    ctn91xx_write32( Adapter->DmaTimeout, Adapter->DmaBase, MPEG_DMA_TIMEOUT_VALUE );
    ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_SET_TIMEOUT );
    ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_TIMER_ENABLE );

    ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_WRITE_ENABLE );
    ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_INTERRUPT_ENABLE );

    // mpeg dma engine setup done
}

VOID
MpegDeinitDma(
        /*IN*/ PMP_ADAPTER Adapter)
{
    ULONG i;
    ULONG j;

    ctn91xx_write8( 0x1, Adapter->DmaBase, MPEG_DMA_RESET );
    ctn91xx_write8( 0x0, Adapter->DmaBase, MPEG_DMA_RESET );

    MP_STALL_EXECUTION( 30 );

    if( ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_BUSY ) ) {
        DBGPRINT( MP_ERROR, ("resetting while dma is busy, very BAD\n"));
    }

    for(i=0; i<NUM_MPEG_STREAMS; i++) {
        PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];

        MP_ACQUIRE_SPIN_LOCK( &MpegBuffer->Lock, FALSE );

        for(j=0; j<MpegBuffer->NumPages; j++) {

            if(MpegBuffer->LockCnt[j]) {
                MpegBuffer->LockCnt[j] = 1;
                MpegBufferUnmapPage(Adapter, MpegBuffer, j, 0);
                MpegBuffer->LockCnt[j] = 0;
            }
            MpegBuffer->Dropped[j] = 0;
            MpegBuffer->Remaining[j] = 0;
            MpegBuffer->Sizes[j] = 0;
        }

        MP_RELEASE_SPIN_LOCK( &MpegBuffer->Lock, FALSE );
    }
}

ULONG BufferDump = 0;
VOID MpegBufferDump( PMP_MPEG_BUFFER MpegBuffer )
{
    ULONG last_was_skipped = 0;
    ULONG j = 0;
    for (j = 0; j < MpegBuffer->NumPages; j++) {

		if ((j != 0)
			&& (j != MpegBuffer->NumPages - 1)
			&& MpegBuffer->LockCnt[j - 1] == MpegBuffer->LockCnt[j]
			&& MpegBuffer->LockCnt[j] == MpegBuffer->LockCnt[j + 1]
			&& MpegBuffer->Dropped[j - 1] == MpegBuffer->Dropped[j]
			&& MpegBuffer->Dropped[j] == MpegBuffer->Dropped[j + 1]
            && MpegBuffer->Remaining[j - 1] == MpegBuffer->Remaining[j]
			&& MpegBuffer->Remaining[j] == MpegBuffer->Remaining[j + 1]
			&& j != MpegBuffer->ReadIndex
            && j != MpegBuffer->WriteIndex
			&& j != MpegBuffer->NotifyIndex)
		{
			//Skip this entry
			if (!last_was_skipped) {
				DBGPRINT(MP_WARN, ("...\n"));
			}
			last_was_skipped = 1;
		} else {
			last_was_skipped = 0;

			DBGPRINT( MP_WARN, ("MpegBuffer: %03d\t%d\t%d\t%d\t%c%c%c\n", 
                j,
                MpegBuffer->LockCnt[j],
                MpegBuffer->Dropped[j],
                MpegBuffer->Remaining[j],
				j == MpegBuffer->ReadIndex ? 'r' : ' ',
				j == MpegBuffer->WriteIndex ? 'w' : ' ',
				j == MpegBuffer->NotifyIndex ? 'n' : ' '));
		}


	}
}


BOOLEAN MpegBufferDataReady( PMP_MPEG_BUFFER MpegBuffer )
{
    return (MpegBuffer->LockCnt[MpegBuffer->ReadIndex] == 0)
        && (MpegBuffer->Remaining[MpegBuffer->ReadIndex]);
}

BOOLEAN MpegMaskInterrupt( PVOID Context )
{
    PMP_ADAPTER Adapter = Context;
    ctn91xx_write8( 1, Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_MASK );
    ctn91xx_write8( 1, Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR_MASK );
    return TRUE;
}

BOOLEAN MpegUnMaskInterrupt( PVOID Context )
{
    PMP_ADAPTER Adapter = Context;
    ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR_MASK );
    ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_MASK );
    return TRUE;
}

__drv_raisesIRQL( DISPATCH_LEVEL ) 
VOID MpegBufferLock( PMP_MPEG_BUFFER MpegBuffer )
{
    MP_ACQUIRE_SPIN_LOCK( &MpegBuffer->Lock, READER_AT_DISPATCH );
    NdisMSynchronizeWithInterruptEx(
            MpegBuffer->Adapter->NdisInterruptHandle,
            0,
            (PVOID)MpegMaskInterrupt,
            MpegBuffer->Adapter);
}

__drv_requiresIRQL( DISPATCH_LEVEL ) 
VOID MpegBufferUnlock( PMP_MPEG_BUFFER MpegBuffer )
{
    NdisMSynchronizeWithInterruptEx(
            MpegBuffer->Adapter->NdisInterruptHandle,
            0,
            (PVOID)MpegUnMaskInterrupt,
            MpegBuffer->Adapter);
    MP_RELEASE_SPIN_LOCK( &MpegBuffer->Lock, READER_AT_DISPATCH );
}

__drv_raisesIRQL(DISPATCH_LEVEL) 
VOID RtpRunningLock( PMP_RTP_STATE RtpState )
{
    MP_ACQUIRE_SPIN_LOCK( &RtpState->RunningLock, READER_AT_DISPATCH );
}

__drv_requiresIRQL(DISPATCH_LEVEL) 
VOID RtpRunningUnlock( PMP_RTP_STATE RtpState )
{
    MP_RELEASE_SPIN_LOCK( &RtpState->RunningLock, READER_AT_DISPATCH );
}

#define ACCUM_AMOUNT 100

#if PRINT_RTP_STATS
extern rtp_counters_t counters[NUM_MPEG_STREAMS];
#endif


VOID
MpegReader(
        /*IN*/ PMP_ADAPTER Adapter )
{
    int i,j;
    PMP_MPEG_BUFFER MpegBuffer = NULL;


    for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

        BOOLEAN FromTimer = FALSE;

        MpegBuffer = &Adapter->MpegBuffers[i];
#if PRINT_RTP_STATS
        counters[i].mpeg_reader++;
#endif

        if( !MpegBuffer->RtpState.GotSetup ) {

			ctn91xx_write8( 0, Adapter->TimerBase,
					TIMER_ENABLE( MpegBuffer->TunerIndex ) );
			ctn91xx_write8( 0, Adapter->TimerBase, TIMER_IRQ_REG( MpegBuffer->TunerIndex ) );
			ctn91xx_write8( 0, Adapter->TimerBase, TIMER_MASK( MpegBuffer->TunerIndex ) );

			ctn91xx_write8( 0, Adapter->TimerBase,
					MAX_TIMER_ENABLE( MpegBuffer->TunerIndex ) );
			ctn91xx_write8( 0, Adapter->TimerBase, MAX_TIMER_IRQ_REG( MpegBuffer->TunerIndex ) );
			ctn91xx_write8( 0, Adapter->TimerBase, MAX_TIMER_MASK( MpegBuffer->TunerIndex ) );
            continue;
        }

        RtpRunningLock( &MpegBuffer->RtpState );

        if( !MpegBuffer->RtpState.Running ) {

#if PRINT_RTP_STATS
            counters[i].not_setup++;
#endif

            if( (ctn91xx_read8(Adapter->TimerBase, TIMER_IRQ_REG( i ) ) & 0x2) ) {
#if PRINT_RTP_STATS
                counters[i].min_timer_went_off++;
#endif
                FromTimer = TRUE;
            } else {
#if PRINT_RTP_STATS
                counters[i].mpeg_delayed_for_min_timer++;
#endif
                RtpRunningUnlock( &MpegBuffer->RtpState );
                continue;
            }
        } else {
            if( ctn91xx_read8(Adapter->TimerBase, MAX_TIMER_IRQ_REG( i ) ) & 0x2) {
#if PRINT_RTP_STATS
                counters[i].max_timer_went_off++;
#endif
                FromTimer = TRUE;
            }
        } 

        if((( Adapter->DropCount[i] - 1) % (ACCUM_AMOUNT)) == 0) {
            DBGPRINT( MP_WARN, ("DROP (in read): DropCount %d TunerIndex %d PacketCount %d\n", Adapter->DropCount[i], i, MpegBuffer->PacketCount));
            MpegBufferDump( MpegBuffer );
            Adapter->DropCount[i]++;
        }

        MP_ACQUIRE_SPIN_LOCK( &Adapter->RtpLock, TRUE );

        MpegBufferLock( MpegBuffer );

        if( FromTimer ) {
            RtpSinkDataReady( MpegBuffer, TRUE, 1 );
        }

        for( j=0; j<(RTP_NOTIFY_COUNT/RTP_NOTIFY_PER_PKT); j++ ) {

            if( MpegBufferDataReady( MpegBuffer ) ) {
#if PRINT_RTP_STATS
                counters[i].extra_data_in_dpc++;
#endif
                RtpSinkDataReady( MpegBuffer, FALSE, 2 );
            } else {
                break;
            }

            if( !MpegBuffer->RtpState.Running ) {
                break;
            }
        }

        MpegBufferUnlock( MpegBuffer );

        MP_RELEASE_SPIN_LOCK( &Adapter->RtpLock, TRUE );

        RtpRunningUnlock( &MpegBuffer->RtpState );
    }
}

ULONG
MpegBufferRead(
        PMP_MPEG_BUFFER MpegBuffer,
        PUCHAR Buffer,
        ULONG BufferLength)
{
    BOOLEAN Done = FALSE;
    ULONG Retval = 0;
    ULONG Count = BufferLength;
    ULONG SavedCount = BufferLength;

    while(!Done) {
        ULONG Offset = MpegBuffer->Sizes[MpegBuffer->ReadIndex] - MpegBuffer->Remaining[MpegBuffer->ReadIndex];


        if( !MpegBufferDataReady( MpegBuffer) ) {
            goto out;
        }

        if( MpegBuffer->Remaining[MpegBuffer->ReadIndex] >= Count ) {

            NdisMoveMemory( &Buffer[SavedCount - Count], &MpegBuffer->Buffers[MpegBuffer->ReadIndex][Offset], Count );

            MpegBuffer->Remaining[MpegBuffer->ReadIndex] -= (USHORT)Count;
            Retval += Count;
            Done = TRUE;

        } else if( MpegBuffer->Remaining[MpegBuffer->ReadIndex] > 0) { //remaining < count

            NdisMoveMemory( &Buffer[SavedCount - Count], &MpegBuffer->Buffers[MpegBuffer->ReadIndex][Offset], MpegBuffer->Remaining[MpegBuffer->ReadIndex] );

            Retval += MpegBuffer->Remaining[MpegBuffer->ReadIndex];
            Count -= MpegBuffer->Remaining[MpegBuffer->ReadIndex];
            MpegBuffer->Remaining[MpegBuffer->ReadIndex] = 0;
        } else {
            DBGPRINT( MP_ERROR, ("mpeg read, bad state\n"));
            Retval = 0;
            goto out;
        }

        if(!MpegBuffer->Remaining[MpegBuffer->ReadIndex]) {
            MpegBuffer->ReadIndex = WRAPPED_PAGE_INDEX( MpegBuffer->ReadIndex + 1 );
        }
    }

out:
    {
        static int first = 1;
        if( first ) {
            first = 0;
            DBGPRINT( MP_WARN, ("first mpeg read %d bytes\n", Retval));
        }
    }
    return Retval;
}

ULONG
MpegBufferAddrListOffset( PMP_MPEG_BUFFER MpegBuffer, ULONG Bank )
{
    ULONG ret = 0;
    if( MpegBuffer->TunerIndex < 6 ) {
        ret = 128 + ( MpegBuffer->TunerIndex * 128 * 4 );
        if( Bank == 1 ) {
            ret += 64*4;
        }

    } else if( MpegBuffer->TunerIndex >= 6 && MpegBuffer->TunerIndex < 12 ) {
        ret = 3200 + ((MpegBuffer->TunerIndex - 6) * 16 * 4);
        if( Bank == 1 ) {
            ret += 8*4;
        }
    }
    return ret;
}

BOOLEAN
MpegBufferCheckDrop(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG NewMapPageIndex,
        ULONG NumBankPages)
{
    ULONG EndOfNewMapIndex = ( NewMapPageIndex + NumBankPages ) % MpegBuffer->NumPages;

    if( !MpegBuffer->RtpState.GotSetup ) {
        return FALSE;
    }

	// Checks to see if there is a drop
    // -If new_map_page_index (which is the new write page) is in the range
    //  that is currently being read from, then we will need to drop data.
    // -The following code checks this and also takes into account the
    //  wrapping in the vbuffer
    //
    if( EndOfNewMapIndex < NewMapPageIndex ) {
        //it wrapped
        return MpegBuffer->ReadIndex >= NewMapPageIndex || MpegBuffer->ReadIndex < EndOfNewMapIndex;
    } else {
        return MpegBuffer->ReadIndex >= NewMapPageIndex && MpegBuffer->ReadIndex < EndOfNewMapIndex;
    }
}


VOID
MpegBufferMapBank(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG Bank,
        BOOLEAN IncrementLock )
{
    ULONG i;
    NTSTATUS Status;
    ULONG AddrListOffset = MpegBufferAddrListOffset( MpegBuffer, Bank );

    for( i=0; i<Adapter->NumPagesPerBank; i++ ) {
        ULONG WritePageIndex = WRAPPED_PAGE_INDEX( MpegBuffer->WriteIndex + i );

        ctn91xx_write32( MpegBuffer->PhysicalAddresses[WritePageIndex], 
                Adapter->DmaBase, AddrListOffset + (i*4) );

        if( IncrementLock ) {
            MpegBuffer->LockCnt[WritePageIndex]++;
        }
    }
}

VOID
MpegBufferUnmapPage(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG NotifyIndex,
        USHORT BytesPerPage)
{

    //Check some exceptional conditions first
    if( MpegBuffer->LockCnt[NotifyIndex] == 0 &&
        MpegBuffer->Dropped[NotifyIndex] ) {
        MpegBuffer->Dropped[NotifyIndex] = 0;
        return;
    } else if( MpegBuffer->LockCnt[NotifyIndex] == 0 ) {
        static int FirstTime = 1;
        if( FirstTime ) {
            FirstTime = 0;

            DBGPRINT( MP_ERROR, ("LockCnt == 0 before unlock for tuner_index = %d\n", MpegBuffer->TunerIndex) );
            DBGPRINT( MP_ERROR, ("\tNotifyIndex %d\n", NotifyIndex ) );
            MpegBufferDump( MpegBuffer );
        }
        return;
    }
    {
        static int first = 1;
        if( first ) {
            first = 0;
            DBGPRINT( MP_WARN, ("first mpeg notify\n"));
            DBGPRINT( MP_WARN, ("\tpages per notify: %d\n", ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_PAGES_PER_NOTIFY_BASE + (2*8))));
        }
    }

    MpegBuffer->LockCnt[NotifyIndex]--;

    if( MpegBuffer->LockCnt[NotifyIndex] == 0 ) {

        MpegBuffer->Remaining[NotifyIndex] = BytesPerPage;
        MpegBuffer->Sizes[NotifyIndex] = BytesPerPage;
    }
}

static VOID
MpegCountCC(
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG TunerIndex,
        PUCHAR Buffer,
        ULONG Len)
{
#if TRACK_PID_CC_MPEG
    ULONG pkts = Len/188;
    ULONG i = 0;
    for (i = 0; i < pkts; i++)
    {
        PUCHAR Pkt = Buffer + (188 * i);
        ULONG pid = ((Pkt[1] & 0x1f) << 8) | (Pkt[2] & 0xff);
        ULONG continuity_counter = (Pkt[3] & 0x0f);
        UCHAR has_adaptation = (((Pkt[3] >> 5) & 0x1));
        UCHAR adaptation_length = Pkt[4];
        UCHAR adaptation_flags = Pkt[5];
        
        ULONG expected_cc = (track_pid_cc_notify[TunerIndex][pid].last_cc + 1) & 0xf;

        if( Pkt[0] != 0x47 ) {
            continue;
        }

        if (expected_cc != continuity_counter 
			&& continuity_counter != track_pid_cc_notify[TunerIndex][pid].last_cc
            && (!has_adaptation || adaptation_length == 0 || !(adaptation_flags & 0x80))) //discontinuity_indicator
		{
            track_pid_cc_notify[TunerIndex][pid].discontinuities++;
            MpegBuffer->DiscontinuityCount++;
            
            if (track_pid_cc_notify[TunerIndex][pid].discontinuities % 1024 == 0 
                    || (track_pid_cc_notify[TunerIndex][pid].discontinuities == 2 
                        && track_pid_cc_notify[TunerIndex][pid].count > 3) ) {
                DBGPRINT( MP_WARN, ("NOTIFY[%d] pid %#x discontinuities %d count %d expected_cc %#01x cc %#01x",
                            TunerIndex,
                            pid,
                            track_pid_cc_notify[TunerIndex][pid].discontinuities, 
                            track_pid_cc_notify[TunerIndex][pid].count,
                            expected_cc, 
                            continuity_counter) );
            }
        }
        track_pid_cc_notify[TunerIndex][pid].count++;
        track_pid_cc_notify[TunerIndex][pid].last_cc = continuity_counter;
    }
#endif
}

VOID
VerifyBufferState(
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG WhoCalled )
{
    ULONG j;
    ULONG NumBankPages = MpegBuffer->Adapter->NumPagesPerBank;
    static ULONG cnt=0;

    ULONG NotifyAbs = ( MpegBuffer->NotifyIndex < MpegBuffer->WriteIndex ? MpegBuffer->NumPages + MpegBuffer->NotifyIndex : MpegBuffer->NotifyIndex );

    cnt++;

    if( !(NotifyAbs >= MpegBuffer->WriteIndex && NotifyAbs < MpegBuffer->WriteIndex + 2*NumBankPages) ) {
        DBGPRINT( MP_WARN, ("Verify1 Failed (%d, %d): NotifyIndex=%d NotifyAbs=%d WriteIndex=%d \n", 
                    WhoCalled, cnt, MpegBuffer->NotifyIndex, NotifyAbs, MpegBuffer->WriteIndex) );
    }

    if( MpegBuffer->RtpState.GotSetup && MpegBuffer->LockCnt[MpegBuffer->ReadIndex] ) {
        if( !(MpegBuffer->ReadIndex == MpegBuffer->NotifyIndex) ) {
            DBGPRINT( MP_WARN, ("Verify2 Failed (%d, %d): ReadIndex=%d NotifyIndex=%d \n", 
                        WhoCalled, cnt, MpegBuffer->ReadIndex, MpegBuffer->NotifyIndex) );
        }
    }

    for (j = 0; j < MpegBuffer->NumPages; j++) {

        ULONG JAbs = ( j < MpegBuffer->WriteIndex ? MpegBuffer->NumPages + j : j);
        if(MpegBuffer->LockCnt[j]) {
            if(!(JAbs >= NotifyAbs && JAbs < MpegBuffer->WriteIndex + 2*NumBankPages)) {
                DBGPRINT( MP_WARN, ("Verify3 Failed (%d, %d): j=%d JAbs=%d NotifyIndex=%d NotifyAbs=%d WriteIndex=%d ReadIndex=%d Dropped=%d b={%d,%d,%d,%d,%d,%d}\n",
                            WhoCalled, cnt, j, JAbs, MpegBuffer->NotifyIndex, NotifyAbs, MpegBuffer->WriteIndex, MpegBuffer->ReadIndex, MpegBuffer->Dropped[j],
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (0*2) ),
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (1*2) ),
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (2*2) ),
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (3*2) ),
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (4*2) ),
                            ctn91xx_read8( MpegBuffer->Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (5*2) )));

            }
        }
        
        if(MpegBuffer->Dropped[j]) {
            if(!(JAbs >= MpegBuffer->WriteIndex  && JAbs < MpegBuffer->WriteIndex + NumBankPages)) {
                DBGPRINT( MP_WARN, ("Verify4 Failed (%d, %d): j=%d JAbs=%d NotifyIndex=%d NotifyAbs=%d WriteIndex=%d \n", 
                            WhoCalled, cnt, j, JAbs, MpegBuffer->NotifyIndex, NotifyAbs, MpegBuffer->WriteIndex) );
            }
        }
	}
}

UCHAR
MpegBufferHandleNotify(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        UCHAR NotifyCount,
        USHORT BytesPerPage,
        BOOLEAN* ShouldQueueDpc )
{
    ULONG i;

    MpegBuffer->PacketCount += ( NotifyCount * BytesPerPage ) / 188;
    
    if( NotifyCount > Adapter->NumPagesPerBank ) {
        DBGPRINT( MP_WARN, ("NotifyCount(%d) > %d %x\n", NotifyCount, Adapter->NumPagesPerBank, ShouldQueueDpc ) );
        return 0;
    }

    if( BytesPerPage != DEFAULT_MPEG_BYTES_PER_PAGE ) {
        DBGPRINT( MP_WARN, ("BytesPerPage(%d) != %d %x\n", BytesPerPage, DEFAULT_MPEG_BYTES_PER_PAGE, ShouldQueueDpc ) );
        return 0;
    }

    DBGPRINT( MP_TRACE, ("----> MpegBufferHandleNotify (NotifyCount %d BytesPerPage %d)\n", NotifyCount, BytesPerPage));
    /**
     * This is what happens here:
     * -This function is called when the hardware has finished with
     *  "notify_cnt" worth of pages.
     * -So we start at the index of the notify_ptr and unlock/unmap
     *  "notify_cnt" worth of pages
     *
     */

#if VERIFY_MPEG_BUFFERS
    VerifyBufferState(MpegBuffer, 1);
#endif

    for( i=0; i<NotifyCount; i++ ) {

        MpegBuffer->TotalNotifies++;

        MpegBufferUnmapPage( Adapter, MpegBuffer, MpegBuffer->NotifyIndex, BytesPerPage );
        MpegCountCC( MpegBuffer, MpegBuffer->TunerIndex, PAGE_INDEX_TO_PTR( MpegBuffer->NotifyIndex ), BytesPerPage );
        
#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 2);
#endif
        if( MpegBuffer->LockCnt[MpegBuffer->NotifyIndex] == 0 ) {
            MpegBuffer->NotifyIndex = WRAPPED_PAGE_INDEX( MpegBuffer->NotifyIndex + 1 );
        }
#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 3);
#endif
    }

#if VERIFY_MPEG_BUFFERS
    VerifyBufferState(MpegBuffer, 4);
#endif

    if( ShouldQueueDpc && MpegBuffer->RtpState.GotSetup && MpegBuffer->RtpState.Running ) {
        *ShouldQueueDpc = TRUE;
    }

    DBGPRINT( MP_TRACE, ("<---- MpegBufferHandleNotify\n"));

    return NotifyCount;
}

VOID
MpegBufferHandleBank(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        UCHAR WhichBank )
{
    /**
     * So this is what happens here:
     * -On entering this function the write_ptr is at the bank
     *  that we should unmap.
     * -The bank after write_ptr (next_write_page_index)
     *  is locked, and the hardware is currently dma'ing into it.
     * -The bank after that one (new_map_page_index) is the one
     *  we need to map and lock now.
     *
     * After you are done locking the new_map_page_index bank,
     * you need to move the write_ptr back to point at next_write_page_index
     *
     * To detect when we are dropping data, we need to check whether the
     * read_ptr is contained within the range that "new_map_page_index"
     * will cover. If it is, we want to make the hardware just reuse
     * the current sg list pages, until the reader has moved on.
     *
     */
    BOOLEAN DropDetected = FALSE;
    ULONG NextWritePageIndex;
    ULONG NewMapPageIndex;
    static int drop_ctr = 0;

    DBGPRINT( MP_TRACE, ("----> MpegBufferHandleBank WhichBank=%d\n", WhichBank));

#if VERIFY_MPEG_BUFFERS
    VerifyBufferState(MpegBuffer, 5);
#endif

    NextWritePageIndex = WRAPPED_PAGE_INDEX( MpegBuffer->WriteIndex + Adapter->NumPagesPerBank );
    NewMapPageIndex = ( NextWritePageIndex + Adapter->NumPagesPerBank ) % MpegBuffer->NumPages;

    if( MpegBufferCheckDrop( Adapter, MpegBuffer, NewMapPageIndex, Adapter->NumPagesPerBank ) ) {
        DropDetected = TRUE;
    }

    if( !DropDetected ) {

#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 6);
#endif

        //temporarily change write_ptr to NewMapPageIndex
        MpegBuffer->WriteIndex = NewMapPageIndex;
        MpegBufferMapBank( Adapter, MpegBuffer, WhichBank, TRUE );
        MpegBuffer->WriteIndex = NextWritePageIndex;

#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 7);
#endif
    } else {
        //in the drop case:
        //
        //		o leave write_ptr where it is
        //		o set notify_ptr = write_ptr
        //		o copy the pages currently in the OTHER bank into the bank we
        //			are updating
        //		o flag the pages at write_ptr "dropped" so unmap doesn't
        //			freak out when notify is called later with new data
        //

        ULONG j;
        ULONG SavedWriteIndex = MpegBuffer->WriteIndex;

#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 8);
#endif
		MpegBuffer->NotifyIndex = MpegBuffer->WriteIndex;

        MpegBuffer->WriteIndex = NextWritePageIndex;

        MpegBufferMapBank( Adapter, MpegBuffer, WhichBank, FALSE );

        MpegBuffer->WriteIndex = SavedWriteIndex;

        for( j=0; j<Adapter->NumPagesPerBank; j++ ) {
			ULONG CurPageIndex = WRAPPED_PAGE_INDEX( MpegBuffer->WriteIndex + j );
            MpegBuffer->Dropped[CurPageIndex] = 1;
        }

#if VERIFY_MPEG_BUFFERS
        VerifyBufferState(MpegBuffer, 9);
#endif
        Adapter->DropCount[MpegBuffer->TunerIndex]++;
    }

    DBGPRINT( MP_TRACE, ("<---- MpegBufferHandleBank\n"));
}

VOID MpegIsrHandleBank( PMP_ADAPTER Adapter )
{
    PMP_MPEG_BUFFER MpegBuffer = NULL;
    int i;

    if( ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_REAL ) == 1 ) {

        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {
            UCHAR BankLock = ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (i*2) );

            MpegBuffer = &Adapter->MpegBuffers[i];

            if( BankLock ) {
                UCHAR WhichBank = ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (i*2) + 1 );
                MpegBufferHandleBank( Adapter, MpegBuffer, WhichBank );
                ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_LOCK_BASE + (i*2) );
            }
        }
        ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_MASK );
    }
}

VOID MpegIsrHandleNotify( PMP_ADAPTER Adapter, BOOLEAN* ShouldQueueDpc )
{
    USHORT NotifyVector = 0;
    USHORT NotifyMask = 0;
    int i;
    PMP_MPEG_BUFFER MpegBuffer = NULL;

    NotifyVector = ctn91xx_read16( Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR_REAL );
    if( NotifyVector ) {
        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

            MpegBuffer = &Adapter->MpegBuffers[i];

            NotifyMask = (1 << i);
            if( NotifyVector & NotifyMask ) {

                UCHAR NotifyCount = ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_NOTIFY_BASE + (i*8) );

                USHORT BytesPerPage = ctn91xx_read16( Adapter->DmaBase, MPEG_DMA_NOTIFY_BYTES_PER_PAGE_BASE + (i*8) );
                UCHAR PagesCleared = MpegBufferHandleNotify( Adapter, MpegBuffer, NotifyCount, BytesPerPage, ShouldQueueDpc );
                ctn91xx_write8( PagesCleared, Adapter->DmaBase, MPEG_DMA_NOTIFY_BASE + (i*8) );
                ctn91xx_write8( 0x1, Adapter->DmaBase, MPEG_DMA_NOTIFY_BASE + (i*8) + 1);
            }
        }
        ctn91xx_write32( Adapter->DmaTimeout, Adapter->DmaBase, MPEG_DMA_TIMEOUT_VALUE );
        ctn91xx_write8( 0x01, Adapter->DmaBase, MPEG_DMA_SET_TIMEOUT );
        ctn91xx_write8( 0, Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR_MASK );
    }
}

BOOLEAN MpegIsr(PMP_ADAPTER Adapter, BOOLEAN* ShouldQueueDpc)
{
    BOOLEAN Handled = FALSE;
    ULONG i;
    LARGE_INTEGER t;

    *ShouldQueueDpc = FALSE;

    for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

#if PRINT_RTP_STATS
        if( ctn91xx_read32( Adapter->TimerBase, TIMER_TIMEOUT( i ) ) ) {
            if( ctn91xx_read8( Adapter->TimerBase, TIMER_IRQ_REG( i ) ) ) {
                counters[i].hw_timer_non_zero_with_irq++;
            } else {
                counters[i].hw_timer_non_zero_no_irq++;
            }
        }

        if( ctn91xx_read8( Adapter->TimerBase, TIMER_MASK( i ) ) ) {
            counters[i].hw_timer_mask_set++;
        }
#endif

        if( (ctn91xx_read8( Adapter->TimerBase, TIMER_IRQ_REG( i ) ) & 0x1) ||
             (ctn91xx_read8( Adapter->TimerBase, MAX_TIMER_IRQ_REG( i ) ) & 0x1) ) {

#if PRINT_RTP_STATS
            counters[i].hw_timer_timed_out++;

            {
                static BOOLEAN FirstTime = TRUE;
                if( FirstTime ) {
                    FirstTime = FALSE;
                    DBGPRINT(MP_WARN, ("\t\tGot first timer interrupt\n"));
                }
            }
#endif

            ctn91xx_write8( 1, Adapter->TimerBase, TIMER_MASK( i ) );
            ctn91xx_write8( 1, Adapter->TimerBase, MAX_TIMER_MASK( i ) );

            *ShouldQueueDpc = TRUE;
            Handled = TRUE;
        } else {
#if PRINT_RTP_STATS
            counters[i].hw_timer_didnt_timeout++;
#endif
        }
    }

    if( Handled ) {
        return Handled;
    }

    if( ctn91xx_read16( Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR ) ) {
        ctn91xx_write8( 1, Adapter->DmaBase, MPEG_DMA_NOTIFY_VECTOR_MASK );
        MpegIsrHandleNotify( Adapter, ShouldQueueDpc );
        Handled = TRUE;
    }

    if( ctn91xx_read8( Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_MASKED ) == 1 ) {
        //bank interrupt
        ctn91xx_write8( 1, Adapter->DmaBase, MPEG_DMA_BANK_INTERRUPT_MASK );
        MpegIsrHandleBank( Adapter );
        Handled = TRUE;
    }

    return Handled;
}

VOID MpegDpc(PMP_ADAPTER Adapter)
{
    MpegReader( Adapter );
}

