/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_nic.h

Abstract:
    Function prototypes for mp_nic.c, mp_init.c and mp_req.c

Revision History:

Notes:

--*/

#ifndef _MP_NIC_H
#define _MP_NIC_H

//
//  MP_NIC.C
//                    
NDIS_STATUS MiniportSendNetBufferList(
    /*IN*/  PMP_ADAPTER         Adapter,
    /*IN*/  PNET_BUFFER_LIST    NetBufferList,
    /*IN*/  BOOLEAN             bFromQueue);
   
NDIS_STATUS MpHandleSendInterrupt(
    /*IN*/  PMP_ADAPTER     Adapter);
                   
VOID MpHandleRecvInterrupt(
    /*IN*/  PMP_ADAPTER     Adapter);

typedef VOID (*MP_BUFFER_PACKER)( PMP_ADAPTER Adapter, PUCHAR DestBuffer, ULONG PacketLength, PVOID Context );

VOID
MpHandleRecvBuffer(
    /*IN*/  PMP_ADAPTER         Adapter,
    /*IN*/  MP_BUFFER_PACKER    BufferPacker,
    /*IN*/  ULONG               Length,
    /*IN*/  PVOID               Context
    );
 
VOID MpHandleControlInterrupt(
    /*IN*/  PMP_ADAPTER     Adapter);

VOID NICReturnRFD(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  PMP_RFD         pMpRfd);
   
NDIS_STATUS NICStartRecv(
    /*IN*/  PMP_ADAPTER     Adapter);

VOID MpFreeQueuedSendNetBufferLists(
    /*IN*/  PMP_ADAPTER     Adapter);

void MpFreeBusySendNetBufferLists(
    /*IN*/  PMP_ADAPTER     Adapter);
                            
void NICResetRecv(
    /*IN*/  PMP_ADAPTER     Adapter);

VOID MpLinkDetectionDpc(
    /*IN*/  PVOID       SystemSpecific1,
    /*IN*/  PVOID       FunctionContext,
    /*IN*/  PVOID       SystemSpecific2, 
    /*IN*/  PVOID       SystemSpecific3);

VOID
MPIndicateLinkState(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  BOOLEAN         DispatchLevel
    );

//
// MP_/*IN*/IT.C
//                  
      
NDIS_STATUS 
MpFindAdapter(
    /*IN*/  PMP_ADAPTER             Adapter,
    /*IN*/  PNDIS_RESOURCE_LIST     resList
    );

NDIS_STATUS NICReadAdapterInfo(
    /*IN*/  PMP_ADAPTER     Adapter);
              
NDIS_STATUS MpAllocAdapterBlock(
    /*OUT*/ PMP_ADAPTER    *pAdapter,
    /*IN*/  NDIS_HANDLE     MiniportAdapterHandle
    );
    
void MpFreeAdapter(
    /*IN*/  PMP_ADAPTER     Adapter);
                                         
NDIS_STATUS NICReadRegParameters(
    /*IN*/  PMP_ADAPTER     Adapter);
                                              
NDIS_STATUS NICWriteRegParameters(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_STATUS NICAllocAdapterMemory(
    /*IN*/  PMP_ADAPTER     Adapter);
   
VOID NICInitSend(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_STATUS NICInitRecv(
    /*IN*/  PMP_ADAPTER     Adapter);

ULONG NICAllocRfd(
    /*IN*/  PMP_ADAPTER     Adapter, 
    /*IN*/  PMP_RFD         pMpRfd);
    
VOID NICFreeRfd(
    /*IN*/  PMP_ADAPTER     Adapter, 
    /*IN*/  PMP_RFD         pMpRfd,
    /*IN*/  BOOLEAN         DispatchLevel);
   
NDIS_STATUS NICSelfTest(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_MEDIA_CONNECT_STATE 
NICGetMediaState(
    /*IN*/ PMP_ADAPTER Adapter
    );

VOID
NICIssueFullReset(
    /*IN*/ PMP_ADAPTER Adapter
    );

NDIS_STATUS NICInitializeAdapter(
    /*IN*/  PMP_ADAPTER     Adapter);

VOID HwSoftwareReset(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_STATUS HwConfigure(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_STATUS HwSetupIAAddress(
    /*IN*/  PMP_ADAPTER     Adapter);

NDIS_STATUS HwClearAllCounters(
    /*IN*/  PMP_ADAPTER     Adapter);

//
// MP_REQ.C
//                  
    
VOID
MPSendRestoreMsg(
        /*IN*/ PMP_ADAPTER Adapter );

VOID
MPSendSlotNumberMsg(
        /*IN*/ PMP_ADAPTER Adapter );

NDIS_STATUS NICGetStatsCounters(
    /*IN*/  PMP_ADAPTER     Adapter, 
    /*IN*/  NDIS_OID        Oid,
    /*OUT*/ PULONG64        pCounter);
    
NDIS_STATUS NICSetPacketFilter(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  ULONG           PacketFilter);

NDIS_STATUS NICSetMulticastList(
    /*IN*/  PMP_ADAPTER     Adapter);
    
ULONG NICGetMediaConnectStatus(
    /*IN*/  PMP_ADAPTER     Adapter);
    
VOID
MPFillPoMgmtCaps (
    /*IN*/ PMP_ADAPTER Adapter, 
    /*IN*/ /*OUT*/ PNDIS_PNP_CAPABILITIES   pPower_Management_Capabilities, 
    /*IN*/ /*OUT*/  PNDIS_STATUS pStatus,
    /*IN*/ /*OUT*/  PULONG pulInfoLen
    );

VOID
MPSetPowerLowComplete(
        /*IN*/ PMP_ADAPTER Adapter
        );

NDIS_STATUS
MPStartSaveState(
    PMP_ADAPTER Adapter);

VOID
MPStartRestoreState(
        /*IN*/ PMP_ADAPTER Adapter );

VOID
MPHandleSendNetBufferListsComplete(
    /*IN*/ PMP_ADAPTER Adapter,
    /*IN*/ PNET_BUFFER_LIST NetBufferList,
    /*IN*/ NDIS_STATUS Status,
    /*IN*/ BOOLEAN DispatchLevel);

VOID 
MPSendNetBufferListsCommon(
    /*IN*/  NDIS_HANDLE         MiniportAdapterContext,
    /*IN*/  PNET_BUFFER_LIST    NetBufferList,
    /*IN*/  NDIS_PORT_NUMBER    PortNumber,
    /*IN*/  ULONG               SendFlags
    );
                    
#endif  // MP_NIC_H

