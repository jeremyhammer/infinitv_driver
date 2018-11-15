#ifndef _MP_MPEG_H
#define _MP_MPEG_H

#define PAGES_PER_MPEG_DEVICE (1024)
#define DEFAULT_NUM_PAGES_PER_BANK (64)
#define DEFAULT_MPEG_BYTES_PER_PAGE (21*188)
#define DEFAULT_NUM_PAGES_PER_NOTIFY (8)

#define WRAPPED_PAGE_INDEX(index) ( (index) % MpegBuffer->NumPages )
#define PAGE_INDEX_TO_PTR(index) (MpegBuffer->Buffers[(index)])

VOID
MpegInitDma(
        IN PMP_ADAPTER Adapter);

VOID
MpegDeinitDma(
        IN PMP_ADAPTER Adapter);

VOID
MpegBufferMapBank(
        PMP_ADAPTER Adapter,
        PMP_MPEG_BUFFER MpegBuffer,
        ULONG Bank,
        BOOLEAN LockCnt);

ULONG
MpegBufferRead(
        PMP_MPEG_BUFFER MpegBuffer,
        PUCHAR Buffer,
        ULONG BufferLength);

VOID
MpegReaderThread(
        PVOID Context);


BOOLEAN MpegIsr(PMP_ADAPTER Adapter, BOOLEAN* ShouldQueueDpc );
VOID MpegDpc(PMP_ADAPTER Adapter);

VOID MpegBufferLock( PMP_MPEG_BUFFER MpegBuffer );
VOID MpegBufferUnlock( PMP_MPEG_BUFFER MpegBuffer );

VOID RtpRunningLock( PMP_RTP_STATE RtpState );
VOID RtpRunningUnlock( PMP_RTP_STATE RtpState );

VOID MpegBufferDump( PMP_MPEG_BUFFER MpegBuffer );

#define READER_AT_DISPATCH FALSE

#endif
