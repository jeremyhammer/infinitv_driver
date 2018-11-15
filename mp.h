/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp.h

Abstract:
    Miniport generic portion header file
    Change send and receive APIs to use Net Buffer and Net Buffer List

Revision History:

Notes:

--*/

#ifndef _MP_H
#define _MP_H

#define NUM_MPEG_STREAMS 6

#define USE_CTN91XX_HW_FOR_TIME         1
#define USE_STALL_EXECUTION             1
#define VERIFY_MPEG_BUFFERS             0
#define PRINT_RTP_STATS                 DBG
#define POWER_MGMT_STATE_SAVE           0
#define TRACK_SYNC                      DBG
#define TRACK_PID_CC_MPEG               DBG
#define EXPAND_RFD_POOL                 1
#define STAMP_LAST_BRIDGING_SUPPORT     1
#define STAMP_NUM_TS_PKTS_PER_RTP_PKT   0


#define SIZEOF_CONTROL_MSG_FIRST_HEADER 5
#define SIZEOF_CONTROL_MSG_HEADER       1
#define SIZEOF_CONTROL_MSG_RTP_SETUP    28
#define SIZEOF_CONTROL_MSG_RTP_DESTROY  4
#define SIZEOF_CONTROL_MSG_PBDA_TAG_TABLE (4+188)


//also defined in libctn91xx/libctn91xx_rpc.h and ceton_mocur_usb/Device.c
#define CONTROL_MSG_RTP_SETUP               0x01
#define CONTROL_MSG_RTP_DESTROY             0x02
#define CONTROL_MSG_MASK_TABLE_SETUP        0x03
#define CONTROL_MSG_DISCONNECT_CABLE        0x04
#define CONTROL_MSG_CONNECT_CABLE           0x05
#define CONTROL_MSG_RESTORE_STATE           0x06
#define CONTROL_MSG_SAVE_SUBSCRIPTION       0x07
#define CONTROL_MSG_SLOT_NUMBER             0x08
#define CONTROL_MSG_PBDA_TAG_TABLE          0x09
#define CONTROL_MSG_POLL_MASK_TABLE_SETUP   0x0A
#define CONTROL_MSG_NETWORK_UP              0x0B

#define MP_NDIS_MAJOR_VERSION       6
#define MP_NDIS_MINOR_VERSION       0

extern NDIS_OID NICSupportedOids[58];

#define MP_NTDEVICE_STRING  L"\\Device\\CetonMOCUR"
#define MP_LINKNAME_STRING  L"\\DosDevices\\CetonMOCUR"

#define ALIGN_16                   16

#ifndef MIN
#define MIN(a, b)   ((a) > (b) ? b: a)
#endif

#define ABSOLUTE(wait) (wait)

#define RELATIVE(wait) (-(wait))

#define NANOSECONDS(nanos) \
    (((signed __int64)(nanos)) / 100L)

#define MICROSECONDS(micros) \
    (((signed __int64)(micros)) * NANOSECONDS(1000L))

#define MILLISECONDS(milli) \
    (((signed __int64)(milli)) * MICROSECONDS(1000L))

#define SECONDS(seconds) \
    (((signed __int64)(seconds)) * MILLISECONDS(1000L))

//
// The driver should put the data(after Ethernet header) at 8-bytes boundary
//
#define ETH_DATA_ALIGN                      8   // the data(after Ethernet header) should be 8-byte aligned
// 
// Shift HW_RFD 0xA bytes to make Tcp data 8-byte aligned
// Since the ethernet header is 14 bytes long. If a packet is at 0xA bytes 
// offset, its data(ethernet user data) will be at 8 byte boundary
// 
#define HWRFD_SHIFT_OFFSET                0xA   // Shift HW_RFD 0xA bytes to make Tcp data 8-byte aligned

//
// The driver has to allocate more data then HW_RFD needs to allow shifting data
// 
#define MORE_DATA_FOR_ALIGN         (ETH_DATA_ALIGN + HWRFD_SHIFT_OFFSET)

//
// Define 4 macros to get some fields in NetBufferList for miniport's own use
//
#define MP_GET_NET_BUFFER_LIST_LINK(_NetBufferList)       (&(NET_BUFFER_LIST_NEXT_NBL(_NetBufferList)))
#define MP_GET_NET_BUFFER_LIST_NEXT_SEND(_NetBufferList)  (((PMP_NET_BUFFER_LIST_PRIVATE)(_NetBufferList)->MiniportReserved[0])->Next)
#define MP_GET_NET_BUFFER_LIST_REF_COUNT(_NetBufferList)  ((ULONG)(((PMP_NET_BUFFER_LIST_PRIVATE)(_NetBufferList)->MiniportReserved[0])->RefCount))
#define MP_GET_NET_BUFFER_LIST_TYPE(_NetBufferList)       ((ULONG)(((PMP_NET_BUFFER_LIST_PRIVATE)(_NetBufferList)->MiniportReserved[0])->Type))


#define MP_GET_NET_BUFFER_PREV(_NetBuffer)      ((_NetBuffer)->MiniportReserved[0])
#define MP_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(_pEntry)  \
    (CONTAINING_RECORD((_pEntry), NET_BUFFER_LIST, Next))

//
// We may need to initialize other field later
//
#define MP_GET_NET_BUFFER_LIST_RFD(_NetBufferList)      ((_NetBufferList)->MiniportReserved[0]) 
#define INIT_RECV_NET_BUFFER_LIST(_NetBufferList, _Mdl)                     \
    {                                                                       \
        ASSERT((NET_BUFFER_LIST_FIRST_NB(_NetBufferList)) != NULL);                   \
        NET_BUFFER_FIRST_MDL(NET_BUFFER_LIST_FIRST_NB(_NetBufferList)) = (_Mdl);                \
    }                                                                       

#define MP_GET_NET_BUFFER_LIST_PRIVATE(_NetBufferList) ((_NetBufferList)->MiniportReserved[0])

//
// Take advantage of dispatch level
//
#define MP_ACQUIRE_SPIN_LOCK(_Lock, DispatchLevel)                          \
    {                                                                       \
        if (DispatchLevel)                                                  \
        {                                                                   \
            NdisDprAcquireSpinLock(_Lock);                                  \
        }                                                                   \
        else                                                                \
        {                                                                   \
            NdisAcquireSpinLock(_Lock);                                     \
        }                                                                   \
    }
            
#define MP_RELEASE_SPIN_LOCK(_Lock, DispatchLevel)                          \
    {                                                                       \
        if (DispatchLevel)                                                  \
        {                                                                   \
            NdisDprReleaseSpinLock(_Lock);                                  \
        }                                                                   \
        else                                                                \
        {                                                                   \
            NdisReleaseSpinLock(_Lock);                                     \
        }                                                                   \
    }
    
//
// Take advantage of MiniportReserved in NDIS_REQUEST
//
#define NDIS_REQUEST_NEXT_REQUEST(_NdisRequest)     ((PNDIS_OID_REQUEST) &((_NdisRequest)->MiniportReserved[0]))
#define MP_INIT_NDIS_STATUS_INDICATION(_pStatusIndication, _M, _St, _Buf, _BufSize)        \
    {                                                                                      \
        NdisZeroMemory(_pStatusIndication, sizeof(NDIS_STATUS_INDICATION));                \
        (_pStatusIndication)->Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;            \
        (_pStatusIndication)->Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;         \
        (_pStatusIndication)->Header.Size = sizeof(NDIS_STATUS_INDICATION);                \
        (_pStatusIndication)->SourceHandle = _M;                                           \
        (_pStatusIndication)->StatusCode = _St;                                            \
        (_pStatusIndication)->StatusBuffer = _Buf;                                         \
        (_pStatusIndication)->StatusBufferSize = _BufSize;                                 \
    }


//--------------------------------------
// Queue structure and macros
//--------------------------------------
typedef struct _QUEUE_ENTRY
{
    struct _QUEUE_ENTRY *Next;
} QUEUE_ENTRY, *PQUEUE_ENTRY;

typedef struct _QUEUE_HEADER
{
    PQUEUE_ENTRY Head;
    PQUEUE_ENTRY Tail;
} QUEUE_HEADER, *PQUEUE_HEADER;

#define ETH_IS_LOCALLY_ADMINISTERED(Address) \
    (BOOLEAN)(((PUCHAR)(Address))[0] & ((UCHAR)0x02))

#define InitializeQueueHeader(QueueHeader)                 \
    {                                                      \
        (QueueHeader)->Head = (QueueHeader)->Tail = NULL;  \
    }

#define IsQueueEmpty(QueueHeader) ((QueueHeader)->Head == NULL)
#define GetHeadQueue(QueueHeader) ((QueueHeader)->Head)  //Get the head of the queue

#define RemoveHeadQueue(QueueHeader)                  \
    (QueueHeader)->Head;                              \
    {                                                 \
        PQUEUE_ENTRY pNext;                           \
        ASSERT((QueueHeader)->Head);                  \
        pNext = (QueueHeader)->Head->Next;            \
        (QueueHeader)->Head = pNext;                  \
        if (pNext == NULL)                            \
            (QueueHeader)->Tail = NULL;               \
    }

#define InsertHeadQueue(QueueHeader, QueueEntry)                \
    {                                                           \
        ((PQUEUE_ENTRY)QueueEntry)->Next = (QueueHeader)->Head; \
        (QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);       \
        if ((QueueHeader)->Tail == NULL)                        \
            (QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);   \
    }

#define InsertTailQueue(QueueHeader, QueueEntry)                     \
    {                                                                \
        ((PQUEUE_ENTRY)QueueEntry)->Next = NULL;                     \
        if ((QueueHeader)->Tail)                                     \
            (QueueHeader)->Tail->Next = (PQUEUE_ENTRY)(QueueEntry);  \
        else                                                         \
            (QueueHeader)->Head = (PQUEUE_ENTRY)(QueueEntry);        \
        (QueueHeader)->Tail = (PQUEUE_ENTRY)(QueueEntry);            \
    }

//--------------------------------------
// Common fragment list structure
// Identical to the scatter gather frag list structure
// This is created to simplify the NIC-specific portion code
//--------------------------------------
#define MP_FRAG_ELEMENT SCATTER_GATHER_ELEMENT 
#define PMP_FRAG_ELEMENT PSCATTER_GATHER_ELEMENT 

typedef struct _MP_FRAG_LIST {
    ULONG NumberOfElements;
    ULONG_PTR Reserved;
    MP_FRAG_ELEMENT Elements[NIC_MAX_PHYS_BUF_COUNT];
} MP_FRAG_LIST, *PMP_FRAG_LIST;
                     

//--------------------------------------
// Some utility macros        
//--------------------------------------
#ifndef min
#define min(_a, _b)     (((_a) < (_b)) ? (_a) : (_b))
#endif

#ifndef max
#define max(_a, _b)     (((_a) > (_b)) ? (_a) : (_b))
#endif

#define MP_ALIGNMEM(_p, _align) (((_align) == 0) ? (_p) : (PUCHAR)(((ULONG_PTR)(_p) + ((_align)-1)) & (~((ULONG_PTR)(_align)-1))))
#define MP_ALIGNMEM_PHYS(_p, _align) (((_align) == 0) ?  (_p) : (((ULONG)(_p) + ((_align)-1)) & (~((ULONG)(_align)-1))))
#define MP_ALIGNMEM_PA(_p, _align) (((_align) == 0) ?  (_p).QuadPart : (((_p).QuadPart + ((_align)-1)) & (~((ULONGLONG)(_align)-1))))

#define GetListHeadEntry(ListHead)  ((ListHead)->Flink)
#define GetListTailEntry(ListHead)  ((ListHead)->Blink)
#define GetListFLink(ListEntry)     ((ListEntry)->Flink)

#define IsSListEmpty(ListHead)  (((PSINGLE_LIST_ENTRY)ListHead)->Next == NULL)

#define MP_EXIT goto exit

//--------------------------------------
// Memory manipulation macros        
//--------------------------------------

/*++
VOID
MP_MEMSET(
    IN  PVOID       Pointer,
    IN  ULONG       Length,
    IN  UCHAR       Value
    )
--*/
#define MP_MEMSET(Pointer, Length, Value)   NdisFillMemory(Pointer, Length, Value)

/*++
VOID
MP_MEMCOPY(
    IN  POPAQUE     Destn,
    IN  POPAQUE     Source,
    IN  ULONG       Length
    )
--*/
#define MP_MEMCOPY(Destn, Source, Length) NdisMoveMemory((Destn), (Source), (Length))


/*++
ULONG
MP_MEMCOPY(
    IN  PVOID       Destn,
    IN  PVOID       Source,
    IN  ULONG       Length
    )
--*/
#define MPMemCmp(Destn, Source, Length)   \
    RtlCompareMemory((PUCHAR)(Destn), (PUCHAR)(Source), (ULONG)(Length))

#if 0

/*++
PVOID
MP_ALLOCMEMTAG(
    IN  ULONG   Size
    )
--*/

#define MP_ALLOCMEMTAG(_NdisHandle, size) \
    MPAuditAllocMemTag(size, _FILENUMBER, __LINE__, _NdisHandle);

/*++
VOID
MP_FREEMEM(
    IN  PVOID   Pointer
    )
--*/
#define MP_FREEMEM(ptr) if(ptr) MPAuditFreeMem(ptr); ptr = NULL

#else // DBG

#define MP_ALLOCMEMTAG(_NdisHandle, size) \
    NdisAllocateMemoryWithTagPriority(_NdisHandle, size, NIC_TAG, LowPoolPriority)

#define MP_FREEMEM(ptr) if(ptr) NdisFreeMemory(ptr, 0, 0); ptr = NULL

#endif 

#define MP_FREE_NDIS_STRING(str)                        \
    MP_FREEMEM((str)->Buffer);                          \
    (str)->Length = 0;                                  \
    (str)->MaximumLength = 0;                           \
    (str)->Buffer = NULL;

//
// HW IO Operations
//
#define ctn91xx_read8( _base, _offset ) *((PUCHAR)(_base + _offset))
#define ctn91xx_read16( _base, _offset ) *((PUSHORT)(_base + _offset))
#define ctn91xx_read32( _base, _offset ) *((PULONG)(_base + _offset))
#define ctn91xx_read64( _base, _offset ) *((PULONGLONG)(_base + _offset))

#define ctn91xx_write32( _val, _base, _offset ) {\
    ULONG dummy;\
    *((PULONG)(_base + _offset)) = _val;\
    dummy = ctn91xx_read32( _base, _offset );\
}

#define ctn91xx_write16( _val, _base, _offset ) {\
    USHORT dummy;\
    *((PUSHORT)(_base + _offset)) = _val;\
    dummy = ctn91xx_read16( _base, _offset );\
}

#define ctn91xx_write8( _val, _base, _offset ) {\
    UCHAR dummy;\
    *((PUCHAR)(_base + _offset)) = _val;\
    dummy = ctn91xx_read8( _base, _offset );\
}

//--------------------------------------
// Macros for flag and ref count operations       
//--------------------------------------
#define MP_SET_FLAG(_M, _F)         ((_M)->Flags |= (_F))   
#define MP_CLEAR_FLAG(_M, _F)       ((_M)->Flags &= ~(_F))
#define MP_CLEAR_FLAGS(_M)          ((_M)->Flags = 0)
#define MP_TEST_FLAG(_M, _F)        (((_M)->Flags & (_F)) != 0)
#define MP_TEST_FLAGS(_M, _F)       (((_M)->Flags & (_F)) == (_F))

#define MP_INC_REF(_A)              NdisInterlockedIncrement(&(_A)->RefCount)
#define MP_DEC_REF(_A)              NdisInterlockedDecrement(&(_A)->RefCount)
#define MP_GET_REF(_A)              ((_A)->RefCount)

#define MP_INC_RCV_REF(_A)          ((_A)->RcvRefCount++)
#define MP_DEC_RCV_REF(_A)          ((_A)->RcvRefCount--)
#define MP_GET_RCV_REF(_A)          ((_A)->RcvRefCount)
   
typedef struct _MP_ADAPTER MP_ADAPTER, *PMP_ADAPTER;


//stored in miniport reserved[0]
#define SEND_BUFFER_ORIGIN_ETHERNET 0x1
#define SEND_BUFFER_ORIGIN_RPC      0x2

typedef struct _SEND_BUFFER_CONTEXT SEND_BUFFER_CONTEXT, *PSEND_BUFFER_CONTEXT;

struct _SEND_BUFFER_CONTEXT
{
    PMDL Mdl;
    PUCHAR Buffer;
    ULONG Length;
    ULONG Offset;
};

typedef struct _MP_NET_BUFFER_LIST_PRIVATE MP_NET_BUFFER_LIST_PRIVATE, *PMP_NET_BUFFER_LIST_PRIVATE;

struct _MP_NET_BUFFER_LIST_PRIVATE
{
    ULONG Type;
    PNET_BUFFER Next;
    ULONG RefCount;
};

//--------------------------------------
// RFD (Receive Frame Descriptor)
//--------------------------------------
typedef struct _MP_RFD
{
    LIST_ENTRY              List;
    PNET_BUFFER_LIST        NetBufferList;
    PMDL                    Mdl;                // Mdl pointing to Buffer
    PVOID                   RfdBuffer;

    ULONG                   Flags;
    UINT                    PacketSize;         // total size of receive frame
} MP_RFD, *PMP_RFD;

//--------------------------------------
// Structure for pended OIS query request
//--------------------------------------
typedef struct _MP_QUERY_REQUEST
{
    IN NDIS_OID Oid;
    IN PVOID InformationBuffer;
    IN ULONG InformationBufferLength;
    OUT PULONG BytesWritten;
    OUT PULONG BytesNeeded;
} MP_QUERY_REQUEST, *PMP_QUERY_REQUEST;

//--------------------------------------
// Structure for pended OIS set request
//--------------------------------------
typedef struct _MP_SET_REQUEST
{
    IN NDIS_OID Oid;
    IN PVOID InformationBuffer;
    IN ULONG InformationBufferLength;
    OUT PULONG BytesRead;
    OUT PULONG BytesNeeded;
} MP_SET_REQUEST, *PMP_SET_REQUEST;

//--------------------------------------
// Macros specific to miniport adapter structure 
//--------------------------------------

#define MP_OFFSET(field)   ((UINT)FIELD_OFFSET(MP_ADAPTER,field))
#define MP_SIZE(field)     sizeof(((PMP_ADAPTER)0)->field)

typedef enum _NIC_STATE
{
    NicPaused,
    NicPausing,
    NicRunning
} NIC_STATE, *PNIC_STATE;


typedef struct _MP_ADAPTER *PMP_ADAPTER;

#define RTP_HDR_SIZE 12
#define MPEG_TS_PKT_SIZE 188

typedef struct _MP_RTP_STATE
{
    UCHAR RemoteHwAddr[6];
    UCHAR LocalHwAddr[6];
    ULONG RemoteIpAddr;
    ULONG LocalIpAddr;
    USHORT LocalPort;
    USHORT RemotePort;

    BOOLEAN GotSetup;
    BOOLEAN Running;
    NDIS_SPIN_LOCK  RunningLock;

    PUCHAR Buffer;
    ULONG BufferUsed;

    /*
     * Session state
     */
    ULONG StartRtpTime;
    ULONG StartSeq;
    ULONG SeqNo;
    LARGE_INTEGER PrevTime;
    ULONG RtpTimeDelta;
    ULONG Ssrc;

    UCHAR InjectBuffer[RTP_HDR_SIZE+MPEG_TS_PKT_SIZE];
    ULONG InjectMode;

} MP_RTP_STATE, *PMP_RTP_STATE;

typedef struct _MP_MPEG_BUFFER
{
    PMP_ADAPTER     Adapter;
    ULONG           NumPages;
    PUCHAR*         Buffers;
    ULONG           ReadIndex;
    ULONG           WriteIndex;
    ULONG           NotifyIndex;
    PUCHAR          NotifyPtr;

    NDIS_SPIN_LOCK  Lock;
    PMDL            Mdl;

    /**
     * Arrays of size NumPages 
     */
    PUCHAR          LockCnt;
    PUCHAR          Dropped;
    PUSHORT         Remaining;
    PUSHORT         Sizes;
    PULONG          PhysicalAddresses;

    ULONG           TotalNotifies;

    ULONG           TunerIndex;
    ULONG           DiscontinuityCount;
    ULONG           PacketCount;

    MP_RTP_STATE    RtpState;

    //Used as parameters to AllocateAdapterChannel callback
    BOOLEAN         ParamIncrementLock;
    ULONG           ParamBank;

} MP_MPEG_BUFFER, *PMP_MPEG_BUFFER;

typedef enum {
    FIRMWARE_UPDATE_NOT_RUNNING,
    FIRMWARE_UPDATE_INIT,
    FIRMWARE_UPDATE_ERASING,
    FIRMWARE_UPDATE_WRITING,
    FIRMWARE_UPDATE_VERIFYING,
} FirmwareUpdateProgress_t;

//--------------------------------------
// The miniport adapter structure
//--------------------------------------
typedef struct _MP_ADAPTER
{
    LIST_ENTRY              List;
    
    // Handle given by NDIS when the Adapter registered itself.
    NDIS_HANDLE             AdapterHandle;
    PDEVICE_OBJECT          PhysicalDeviceObject;
    
    PDEVICE_OBJECT          Fdo;
    PDEVICE_OBJECT          NextDeviceObject;
    WDFDEVICE               WdfDevice;
    
    PDMA_ADAPTER            DmaAdapterObject;
    ULONG                   NumPagesPerBank;
    ULONG                   BoardNumber;
    
    //eeprom
    BOOLEAN                 EEPROM_UpdateInProgress;
    DWORD                   EEPROM_UpdateProgress; //FirmwareUpdateProgress_t
    DWORD                   EEPROM_UpdatePercent;
    DWORD                   EEPROM_SubprocessPercent;
    DWORD                   EEPROM_SubprocessTotal;
    WDFWORKITEM             EEPROM_WorkItem;

    //flags 
    ULONG                   Flags;

    // configuration 
    UCHAR                   PermanentAddress[ETH_LENGTH_OF_ADDRESS];
    UCHAR                   CurrentAddress[ETH_LENGTH_OF_ADDRESS];
    BOOLEAN                 bOverrideAddress;

    NDIS_EVENT              ExitEvent;

    // SEND                       
    LONG                    nBusySend;
    LONG                    nWaitSend;
    QUEUE_HEADER            SendWaitQueue;
    QUEUE_HEADER            SendCancelQueue;

    // RECV
    LIST_ENTRY              RecvList;
    LIST_ENTRY              RecvPendList;
    LONG                    nReadyRecv;
    LONG                    RefCount;

    LONG                    NumRfd;
    LONG                    CurrNumRfd;
    LONG                    MaxNumRfd;
    ULONG                   RfdBufferSize;
    PUCHAR                  RfdBuffer;
    ULONG                   NumRfdBuffers;
    
    BOOLEAN                 bAllocNewRfd;
    LONG                    RfdShrinkCount;

    NDIS_HANDLE             RecvNetBufferListPool;
    NDIS_HANDLE             RpcSendNetBufferListPool;

    PNDIS_OID_REQUEST       PendingRequest;

    // spin locks
    NDIS_SPIN_LOCK          Lock;
    NDIS_SPIN_LOCK          MpegLock;
    NDIS_SPIN_LOCK          ControlLock;
    NDIS_SPIN_LOCK          RtpLock;

    // lookaside lists                               
    NPAGED_LOOKASIDE_LIST   RecvLookaside;

    // Packet Filter and look ahead size.

    ULONG                   PacketFilter;
    ULONG                   OldPacketFilter;
    ULONG                   ulLookAhead;
    USHORT                  usLinkSpeed;
    USHORT                  usDuplexMode;

    // Packet counts
    ULONG64                 GoodTransmits;
    ULONG64                 GoodReceives;
    ULONG                   NumTxSinceLastAdjust;
    ULONG64                 InUcastPkts;
    ULONG64                 InUcastOctets;
    ULONG64                 InMulticastPkts;  
    ULONG64                 InMulticastOctets;  
    ULONG64                 InBroadcastPkts;  
    ULONG64                 InBroadcastOctets;  
    ULONG64                 OutUcastPkts;     
    ULONG64                 OutUcastOctets;     
    ULONG64                 OutMulticastPkts; 
    ULONG64                 OutMulticastOctets; 
    ULONG64                 OutBroadcastPkts; 
    ULONG64                 OutBroadcastOctets; 

    // Count of transmit errors
    ULONG                   TxAbortExcessCollisions;
    ULONG                   TxLateCollisions;
    ULONG                   TxDmaUnderrun;
    ULONG                   TxLostCRS;
    ULONG                   TxOKButDeferred;
    ULONG                   OneRetry;
    ULONG                   MoreThanOneRetry;
    ULONG                   TotalRetries;

    // Count of receive errors
    ULONG                   RcvCrcErrors;
    ULONG                   RcvAlignmentErrors;
    ULONG                   RcvResourceErrors;
    ULONG                   RcvDmaOverrunErrors;
    ULONG                   RcvCdtFrames;
    ULONG                   RcvRuntErrors;
    ULONG                   RcvExpandPool;

    ULONG                   InterruptLevel;
    ULONG                   InterruptVector;
    NDIS_PHYSICAL_ADDRESS   MemPhysAddress;
    ULONG                   MemPhysSize;

    NDIS_PHYSICAL_ADDRESS   TranslationMemPhysAddress;
    ULONG                   TranslationMemPhysSize;

	ULONG					ReleaseVersion;
    ULONG                   ChipVersion;
    ULONG                   TicksPerUS;

    PUCHAR                  RegisterBase;
    PUCHAR                  TranslationMap;
    PUCHAR                  MsgBase;
    PUCHAR                  DmaBase;
    PUCHAR                  TimerBase;
    PUCHAR                  SystemControlBase;
    PUCHAR                  EPCSBase;
    NDIS_HANDLE             NdisInterruptHandle;
    PIO_INTERRUPT_MESSAGE_INFO MessageInfoTable;  // if the driver supports MSI
    NDIS_INTERRUPT_TYPE        InterruptType;     // if the driver supports MSI

    ULONG                   TranslationBase;
    ULONG                   TranslationMask;

    PUCHAR                  RpcBuffer;
    ULONG                   RpcSize;
    ULONG                   RpcOffset;

    // Revision ID
    UCHAR                   RevsionID;

    USHORT                  SubVendorID;
    USHORT                  SubSystemID;

    BOOLEAN                 SupportBridging;
    USHORT                  ReportedSpeed;
    BOOLEAN                 RevertBridging;
    BOOLEAN                 LastSupportBridging;

    //
    // new fields for NDIS 6.0 version to report unknown 
    // states and speed
    //
    NDIS_MEDIA_CONNECT_STATE  MediaState;
    NDIS_MEDIA_DUPLEX_STATE   MediaDuplexState;
    ULONG64                   LinkSpeed;
    
    NDIS_DEVICE_POWER_STATE CurrentPowerState;
    NDIS_DEVICE_POWER_STATE NextPowerState;

    // WMI support
    ULONG                   HwErrCount;

    NDIS_SPIN_LOCK          SendLock;
    NDIS_SPIN_LOCK          RcvLock;
    ULONG                   RcvRefCount;  // number of packets that have not been returned back
    NDIS_EVENT              AllPacketsReturnedEvent;
    PNET_BUFFER_LIST        SendingNetBufferList;

    NIC_STATE               AdapterState;
    BOOLEAN                 ForceDisconnectCable;

    UCHAR                   MsgBufferRecvPending;
    UCHAR                   MsgBufferSendPending;
    UCHAR                   MsgBufferControlPending;
    UCHAR                   MsgBufferControlRecvPending;

    ULONG                   DmaTimeout;
    MP_MPEG_BUFFER          MpegBuffers[NUM_MPEG_STREAMS];
    ULONG                   DropCount[NUM_MPEG_STREAMS];

    NDIS_HANDLE             ControlDeviceHandle;
    PDEVICE_OBJECT          ControlDeviceObject;

} MP_ADAPTER, *PMP_ADAPTER;

typedef struct _WDF_DEVICE_INFO{

    PMP_ADAPTER Adapter;

} WDF_DEVICE_INFO, *PWDF_DEVICE_INFO;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WDF_DEVICE_INFO, GetWdfDeviceInfo)


//--------------------------------------
// Stall execution and wait with timeout
//--------------------------------------
/*++
    _condition  - condition to wait for 
    _timeout_ms - timeout value in milliseconds
    _result     - TRUE if condition becomes true before it times out
--*/
#define MP_STALL_AND_WAIT(_condition, _timeout_ms, _result)     \
{                                                               \
    int counter;                                                \
    _result = FALSE;                                            \
    for(counter = _timeout_ms * 50; counter != 0; counter--)    \
    {                                                           \
        if(_condition)                                          \
        {                                                       \
            _result = TRUE;                                     \
            break;                                              \
        }                                                       \
        NdisStallExecution(20);                                 \
    }                                                           \
}

__inline VOID MP_STALL_EXECUTION(
   IN UINT MsecDelay)
{
    // Delay in 100 usec increments
    MsecDelay *= 10;
    while (MsecDelay)
    {
        NdisStallExecution(100);
        MsecDelay--;
    }
}

#define MP_GET_ADAPTER_HANDLE(_A) (_A)->AdapterHandle

__inline NDIS_STATUS MP_GET_STATUS_FROM_FLAGS(PMP_ADAPTER Adapter)
{
    NDIS_STATUS Status = NDIS_STATUS_FAILURE;

    if(MP_TEST_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS))
    {
        Status = NDIS_STATUS_RESET_IN_PROGRESS;      
    }
    else if(MP_TEST_FLAG(Adapter, fMP_ADAPTER_HARDWARE_ERROR))
    {
        Status = NDIS_STATUS_DEVICE_FAILED;
    }
    else if(MP_TEST_FLAG(Adapter, fMP_ADAPTER_NO_CABLE))
    {
        Status = NDIS_STATUS_NO_CABLE;
    }

    return Status;
}   

//--------------------------------------
// Miniport routines in MP_MAIN.C
//--------------------------------------

NDIS_STATUS
DriverEntry(
    IN  PDRIVER_OBJECT      DriverObject,
    IN  PUNICODE_STRING     RegistryPath
    );

VOID 
MPAllocateComplete(
    NDIS_HANDLE     MiniportAdapterContext,
    IN PVOID        VirtualAddress,
    IN PNDIS_PHYSICAL_ADDRESS PhysicalAddress,
    IN ULONG        Length,
    IN PVOID        Context
    );

BOOLEAN 
MPCheckForHang(
    IN NDIS_HANDLE MiniportAdapterContext
    );

VOID
MPHalt(
    IN  NDIS_HANDLE             MiniportAdapterContext,
    IN  NDIS_HALT_ACTION        HaltAction
    );


NDIS_STATUS
MpSetOptions(
    IN NDIS_HANDLE  NdisMiniportDriverHandle,
    IN NDIS_HANDLE  MiniportDriverContext
    );

NDIS_STATUS 
MPInitialize(
    IN  NDIS_HANDLE                        MiniportAdapterHandle,
    IN  NDIS_HANDLE                        MiniportDriverContext,
    IN  PNDIS_MINIPORT_INIT_PARAMETERS     MiniportInitParameters
    );

NDIS_STATUS 
MPPause(
    IN  NDIS_HANDLE                         MiniportAdapterContext,    
    IN  PNDIS_MINIPORT_PAUSE_PARAMETERS     MiniportPauseParameters
    );

NDIS_STATUS 
MPRestart(
    IN  NDIS_HANDLE                         MiniportAdapterContext,    
    IN  PNDIS_MINIPORT_RESTART_PARAMETERS   MiniportRestartParameters
    );

NDIS_STATUS
MPOidRequest(
    IN  NDIS_HANDLE         MiniportInterruptContext,
    IN  PNDIS_OID_REQUEST   NdisRequest
    );

VOID
MPHandleInterrupt(
    IN  NDIS_HANDLE     MiniportInterruptContext,
    IN  PVOID           MiniportDpcContext,
    IN  PULONG          NdisReserved1,
    IN  PULONG          NdisReserved2
    );

BOOLEAN 
MPIsr(
    IN  NDIS_HANDLE     MiniportInterruptContext,
    OUT PBOOLEAN        QueueMiniportInterruptDpcHandler,
    IN  PULONG          TargetProcessors
    );

NDIS_STATUS
MpQueryInformation(
    IN NDIS_HANDLE         MiniportAdapterContext,
    IN PNDIS_OID_REQUEST   NdisRequest
    );

NDIS_STATUS 
MPReset(
    IN  NDIS_HANDLE     MiniportAdapterContext,
    OUT PBOOLEAN        AddressingReset
    );

VOID 
MPReturnNetBufferLists(
    IN  NDIS_HANDLE             MiniportAdapterContext,
    IN  PNET_BUFFER_LIST        NetBufferLists,
    IN  ULONG                   ReturnFlags
    );

NDIS_STATUS 
MpSetInformation(
    IN NDIS_HANDLE         MiniportAdapterContext,
    IN PNDIS_OID_REQUEST   NdisRequest
    );

VOID 
MPShutdown(
    IN  NDIS_HANDLE             MiniportAdapterContext,
    IN  NDIS_SHUTDOWN_ACTION    ShutdownAction
    );

VOID
MPSendNetBufferLists(
    IN  NDIS_HANDLE         MiniportAdapterContext,
    IN  PNET_BUFFER_LIST    NetBufferList,
    IN  NDIS_PORT_NUMBER    PortNumber,
    IN  ULONG               SendFlags
    );

VOID
MPCancelSendNetBufferLists(
    IN  NDIS_HANDLE         MiniportAdapterContext,
    IN  PVOID               CancelId
    );

VOID
MPPnPEventNotify(
    IN  NDIS_HANDLE                 MiniportAdapterContext,
    IN  PNET_DEVICE_PNP_EVENT       NetDevicePnPEvent
    );

BOOLEAN  
MPIsPoMgmtSupported(
   IN PMP_ADAPTER pAdapter
   );

NDIS_STATUS
MpMethodRequest(
    IN  PMP_ADAPTER         Adapter,
    IN  PNDIS_OID_REQUEST   Request
    );
VOID
MpSetPowerLowComplete(
    IN PMP_ADAPTER          Adapter
    );

VOID MPUnload(IN PDRIVER_OBJECT DriverObject);

VOID
MPCancelOidRequest(
    IN  NDIS_HANDLE            MiniportAdapterContext,
    IN  PVOID                  RequestId
    );

#define sdump_buffer(buffer, length, name) ctn91xx_mod_sdump_buffer( "ctn91xx", __FUNCTION__, __LINE__, (buffer), (length), (name))
void ctn91xx_mod_sdump_buffer( const char * module_name, const char * function, int line, const PUCHAR buf, ULONG length, const char* name);

VOID
NICUpdateLinkState( PMP_ADAPTER Adapter, BOOLEAN DispatchLevel );

#endif  // _MP_H



