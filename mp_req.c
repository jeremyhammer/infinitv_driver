/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_req.c

Abstract:
    This module contains miniport OID related handlers

Revision History:

Notes:

--*/


    
#include "precomp.h"


#if DBG
#define _FILENUMBER     'QERM'
#endif

ULONG VendorDriverVersion = NIC_VENDOR_DRIVER_VERSION;

NDIS_OID NICSupportedOids[58] =
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_LOOKAHEAD,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BUFFER_SPACE,
    OID_GEN_RECEIVE_BUFFER_SPACE,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_CURRENT_LOOKAHEAD,
    OID_GEN_DRIVER_VERSION,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_GEN_XMIT_OK,
    OID_GEN_RCV_OK,
    OID_GEN_XMIT_ERROR,
    OID_GEN_RCV_ERROR,
    OID_GEN_RCV_NO_BUFFER,
    OID_GEN_STATISTICS,
    OID_GEN_RCV_CRC_ERROR,
    OID_GEN_TRANSMIT_QUEUE_LENGTH,
    OID_GEN_PHYSICAL_MEDIUM,
    OID_GEN_DIRECTED_BYTES_XMIT,
    OID_GEN_DIRECTED_FRAMES_XMIT,
    OID_GEN_MULTICAST_BYTES_XMIT,
    OID_GEN_MULTICAST_FRAMES_XMIT,
    OID_GEN_BROADCAST_BYTES_XMIT,
    OID_GEN_BROADCAST_FRAMES_XMIT,
    OID_GEN_DIRECTED_BYTES_RCV,
    OID_GEN_DIRECTED_FRAMES_RCV,
    OID_GEN_MULTICAST_BYTES_RCV,
    OID_GEN_MULTICAST_FRAMES_RCV,
    OID_GEN_BROADCAST_BYTES_RCV,
    OID_GEN_BROADCAST_FRAMES_RCV,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_RCV_ERROR_ALIGNMENT,
    OID_802_3_XMIT_ONE_COLLISION,
    OID_802_3_XMIT_MORE_COLLISIONS,
    OID_802_3_XMIT_DEFERRED,
    OID_802_3_XMIT_MAX_COLLISIONS,
    OID_802_3_RCV_OVERRUN,
    OID_802_3_XMIT_UNDERRUN,
    OID_802_3_XMIT_HEARTBEAT_FAILURE,
    OID_802_3_XMIT_TIMES_CRS_LOST,
    OID_802_3_XMIT_LATE_COLLISIONS,
 //
 // TBD: Supporting this OID is mandatory for NDIS 6.0 miniports
 // OID_GEN_LINK_PARAMETERS,
 // 


/* powermanagement */

    OID_PNP_CAPABILITIES,
    OID_PNP_SET_POWER,
    OID_PNP_QUERY_POWER,

};

/**
Local Prototypes
**/
NDIS_STATUS
MPSetPower(
    PMP_ADAPTER     Adapter ,
    NDIS_DEVICE_POWER_STATE   PowerState 
    );

//
// Macros used to walk a doubly linked list. Only macros that are not defined in ndis.h
// The List Next macro will work on Single and Doubly linked list as Flink is a common
// field name in both
//

/*
PLIST_ENTRY
ListNext (
    IN PLIST_ENTRY
    );

PSINGLE_LIST_ENTRY
ListNext (
    IN PSINGLE_LIST_ENTRY
    );
*/
#define ListNext(_pL)                       (_pL)->Flink

/*
PLIST_ENTRY
ListPrev (
    IN LIST_ENTRY *
    );
*/
#define ListPrev(_pL)                       (_pL)->Blink


__inline 
BOOLEAN  
MPIsPoMgmtSupported(
   /*IN*/ PMP_ADAPTER pAdapter
   )
{
    return FALSE;
}

NDIS_STATUS 
MPOidRequest(
    /*IN*/  NDIS_HANDLE        MiniportAdapterContext,
    /*IN*/  PNDIS_OID_REQUEST  NdisRequest
    )

/*++
Routine Description:

    MiniportRequest dispatch handler            

Arguments:

    MiniportAdapterContext  Pointer to the adapter structure
    NdisRequest             Pointer to NDIS_OID_REQUEST
    
Return Value:
    
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_BUFFER_TOO_SHORT
    
--*/
{
    PMP_ADAPTER             Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
    NDIS_REQUEST_TYPE       RequestType;

 //   DBGPRINT(MP_TRACE, ("====> MPOidRequest\n"));

    //
    // Should abort the request if reset is in process
    //
    NdisAcquireSpinLock(&Adapter->Lock);


    if (MP_TEST_FLAG(Adapter, 
        (fMP_ADAPTER_RESET_IN_PROGRESS | fMP_ADAPTER_REMOVE_IN_PROGRESS)))
    {
        NdisReleaseSpinLock(&Adapter->Lock);
        return NDIS_STATUS_REQUEST_ABORTED;
    }

    
    NdisReleaseSpinLock(&Adapter->Lock);    
    
    RequestType = NdisRequest->RequestType;
    
    switch (RequestType)
    {
        case NdisRequestMethod:
            break;

        case NdisRequestSetInformation:            
            Status = MpSetInformation(Adapter, NdisRequest);
            break;
                
        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
            Status = MpQueryInformation(Adapter, NdisRequest);
            break;

        default:
            //
            // Later the entry point may by used by all the requests
            //
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

  //  DBGPRINT(MP_TRACE, ("<==== MPOidRequest\n"));
    return Status;
    
}



NDIS_STATUS MpQueryInformation(
    /*IN*/  NDIS_HANDLE         MiniportAdapterContext,
    /*IN*/  PNDIS_OID_REQUEST   NdisRequest
    )
/*++
Routine Description:

    MiniportQueryInformation handler            

Arguments:

    MiniportAdapterContext  Pointer to the adapter structure
    NdisRequest             Pointer to the query request

Return Value:
    
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_NOT_SUPPORTED
    NDIS_STATUS_BUFFER_TOO_SHORT
    
--*/
{
    NDIS_STATUS                 Status = NDIS_STATUS_SUCCESS;
    PMP_ADAPTER                 Adapter;
    NDIS_OID                    Oid;
    PVOID                       InformationBuffer;
    ULONG                       InformationBufferLength;
    ULONG                       BytesWritten;
    ULONG                       BytesNeeded;

    NDIS_HARDWARE_STATUS        HardwareStatus = NdisHardwareStatusReady;
    NDIS_MEDIUM                 Medium = NIC_MEDIA_TYPE;
    NDIS_PHYSICAL_MEDIUM        PhysMedium = NdisPhysicalMediumUnspecified;
    UCHAR                       VendorDesc[] = NIC_VENDOR_DESC;
    ULONG                       VendorDescSize;
    ULONG                       i;
    ANSI_STRING                 strAnsi;
    WCHAR                      *WcharBuf;
    NDIS_PNP_CAPABILITIES       Power_Management_Capabilities;

    ULONG                       ulInfo = 0;
    ULONG64                     ul64Info = 0;
    
    USHORT                      usInfo = 0;                                              
    PVOID                       pInfo = (PVOID) &ulInfo;
    ULONG                       ulInfoLen = sizeof(ulInfo);
    ULONG                       ulBytesAvailable = ulInfoLen;
    NDIS_MEDIA_CONNECT_STATE    CurrMediaState;
    NDIS_MEDIA_STATE            LegacyMediaState;
    BOOLEAN                     DoCopy = TRUE;
    NDIS_LINK_SPEED             LinkSpeed;

//    DBGPRINT(MP_TRACE, ("====> MpQueryInformation\n"));

    Adapter = (PMP_ADAPTER) MiniportAdapterContext;

    Oid = NdisRequest->DATA.QUERY_INFORMATION.Oid;
    InformationBuffer = NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer;
    InformationBufferLength = NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength;

    //
    // Initialize the result
    //
    BytesWritten = 0;
    BytesNeeded = 0;

    //
    // Process different type of requests
    //
    switch(Oid)
    {
        case OID_GEN_SUPPORTED_LIST:
            pInfo = (PVOID) NICSupportedOids;
            ulBytesAvailable = ulInfoLen = sizeof(NICSupportedOids);
            break;

        case OID_GEN_HARDWARE_STATUS:
            pInfo = (PVOID) &HardwareStatus;
            ulBytesAvailable = ulInfoLen = sizeof(NDIS_HARDWARE_STATUS);
            break;

        case OID_GEN_MEDIA_SUPPORTED:
        case OID_GEN_MEDIA_IN_USE:
            pInfo = (PVOID) &Medium;
            ulBytesAvailable = ulInfoLen = sizeof(NDIS_MEDIUM);
            break;

        case OID_GEN_PHYSICAL_MEDIUM:
            pInfo = (PVOID) &PhysMedium;
            ulBytesAvailable = ulInfoLen = sizeof(NDIS_PHYSICAL_MEDIUM);
            break;

        case OID_GEN_CURRENT_LOOKAHEAD:
        case OID_GEN_MAXIMUM_LOOKAHEAD:
            if (Adapter->ulLookAhead == 0)
            {
                Adapter->ulLookAhead = NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE;
            }
            ulInfo = Adapter->ulLookAhead;
            break;         

        case OID_GEN_MAXIMUM_FRAME_SIZE:
            ulInfo = NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE;
            break;

        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            ulInfo = (ULONG) NIC_MAX_PACKET_SIZE;
            break;

        case OID_GEN_MAC_OPTIONS:
            // Notes: 
            // The protocol driver is free to access indicated data by any means. 
            // Some fast-copy functions have trouble accessing on-board device 
            // memory. NIC drivers that indicate data out of mapped device memory 
            // should never set this flag. If a NIC driver does set this flag, it 
            // relaxes the restriction on fast-copy functions. 

            // This miniport indicates receive with NdisMIndicateReceiveNetBufferLists 
            // function. It has no MiniportTransferData function. Such a driver 
            // should set this flag. 

            ulInfo = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | 
                     NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                     NDIS_MAC_OPTION_NO_LOOPBACK;
            
            break;
            
        case OID_GEN_LINK_SPEED:
            ulInfo = Adapter->usLinkSpeed * 10000;
            break;
        case OID_GEN_MEDIA_CONNECT_STATUS:
            if (InformationBufferLength < ulInfoLen)
            {
                break;
            }

            CurrMediaState = Adapter->MediaState;
            if (CurrMediaState == MediaConnectStateConnected)
            {
                LegacyMediaState = NdisMediaStateConnected;
            }
            else
            {
                //
                // treat unknown media state the same as disconnected
                //
                LegacyMediaState = NdisMediaStateDisconnected;
            }

            ulInfo = LegacyMediaState;
            break;

        case OID_GEN_TRANSMIT_BUFFER_SPACE:
            ulInfo = NIC_MAX_PACKET_SIZE;
            break;

        case OID_GEN_RECEIVE_BUFFER_SPACE:
            ulInfo = NIC_MAX_PACKET_SIZE * Adapter->CurrNumRfd;
            break;

        case OID_GEN_VENDOR_ID:
            NdisMoveMemory(&ulInfo, Adapter->PermanentAddress, 3);
            break;

        case OID_GEN_VENDOR_DESCRIPTION:
            pInfo = VendorDesc;
            ulBytesAvailable = ulInfoLen = sizeof(VendorDesc);
            break;

        case OID_GEN_VENDOR_DRIVER_VERSION:
            ulInfo = VendorDriverVersion;
            break;

        case OID_GEN_DRIVER_VERSION:
            usInfo = (USHORT) NIC_DRIVER_VERSION;
            pInfo = (PVOID) &usInfo;
            ulBytesAvailable = ulInfoLen = sizeof(USHORT);
            break;

        case OID_802_3_PERMANENT_ADDRESS:
            pInfo = Adapter->PermanentAddress;
            ulBytesAvailable = ulInfoLen = ETH_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_CURRENT_ADDRESS:
            pInfo = Adapter->CurrentAddress;
            ulBytesAvailable = ulInfoLen = ETH_LENGTH_OF_ADDRESS;
            break;

        case OID_802_3_MAXIMUM_LIST_SIZE:
            ulInfo = NIC_MAX_MCAST_LIST;
            break;

        case OID_GEN_MAXIMUM_SEND_PACKETS:
            ulInfo = NIC_MAX_SEND_PACKETS;
            break;
        case OID_PNP_CAPABILITIES:

            MPFillPoMgmtCaps (Adapter, 
                                &Power_Management_Capabilities, 
                                &Status,
                                &ulInfoLen);
            if (NT_SUCCESS(Status))
            {
                pInfo = (PVOID) &Power_Management_Capabilities;
            }
            else
            {
                pInfo = NULL;
            }
            break;

        case OID_PNP_QUERY_POWER:

            Status = NDIS_STATUS_SUCCESS; 

            break;

        case OID_GEN_XMIT_OK:
        case OID_GEN_RCV_OK:
        case OID_GEN_XMIT_ERROR:
        case OID_GEN_RCV_ERROR:
        case OID_GEN_RCV_NO_BUFFER:
        case OID_GEN_RCV_CRC_ERROR:
        case OID_GEN_TRANSMIT_QUEUE_LENGTH:
        case OID_802_3_RCV_ERROR_ALIGNMENT:
        case OID_802_3_XMIT_ONE_COLLISION:
        case OID_802_3_XMIT_MORE_COLLISIONS:
        case OID_802_3_XMIT_DEFERRED:
        case OID_802_3_XMIT_MAX_COLLISIONS:
        case OID_802_3_RCV_OVERRUN:
        case OID_802_3_XMIT_UNDERRUN:
        case OID_802_3_XMIT_HEARTBEAT_FAILURE:
        case OID_802_3_XMIT_TIMES_CRS_LOST:
        case OID_802_3_XMIT_LATE_COLLISIONS:
        case OID_GEN_BYTES_RCV:
        case OID_GEN_BYTES_XMIT:
        case OID_GEN_RCV_DISCARDS:
        case OID_GEN_DIRECTED_BYTES_XMIT:
        case OID_GEN_DIRECTED_FRAMES_XMIT:
        case OID_GEN_MULTICAST_BYTES_XMIT:
        case OID_GEN_MULTICAST_FRAMES_XMIT:
        case OID_GEN_BROADCAST_BYTES_XMIT:
        case OID_GEN_BROADCAST_FRAMES_XMIT:
        case OID_GEN_DIRECTED_BYTES_RCV:
        case OID_GEN_DIRECTED_FRAMES_RCV:
        case OID_GEN_MULTICAST_BYTES_RCV:
        case OID_GEN_MULTICAST_FRAMES_RCV:
        case OID_GEN_BROADCAST_BYTES_RCV:
        case OID_GEN_BROADCAST_FRAMES_RCV:

            Status = NICGetStatsCounters(Adapter, Oid, &ul64Info);
            ulBytesAvailable = ulInfoLen = sizeof(ul64Info);
            if (NT_SUCCESS(Status))
            {
                if (InformationBufferLength < sizeof(ULONG))
                {
                    Status = NDIS_STATUS_BUFFER_TOO_SHORT;
                    BytesNeeded = ulBytesAvailable;
                    break;
                }

                ulInfoLen = MIN(InformationBufferLength, ulBytesAvailable);
                pInfo = &ul64Info;
            }
                    
        break;         

        case OID_GEN_STATISTICS:
            // we are going to directly fill the information buffer
            DoCopy = FALSE;
            
            ulBytesAvailable = ulInfoLen = sizeof(NDIS_STATISTICS_INFO);
            if (InformationBufferLength < ulInfoLen)
            {
                break;
            }
            Status = NICGetStatsCounters(Adapter, Oid, (PULONG64)InformationBuffer);
            break;

            
        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    if (NT_SUCCESS(Status))
    {
        BytesNeeded = ulBytesAvailable;
        if (ulInfoLen <= InformationBufferLength)
        {
            //
            // Copy result into InformationBuffer
            //
            BytesWritten = ulInfoLen;
            if (ulInfoLen && DoCopy)
            {
                NdisMoveMemory(InformationBuffer, pInfo, ulInfoLen);
            }
        }
        else
        {
            //
            // too short
            //
            BytesNeeded = ulInfoLen;
            Status = NDIS_STATUS_BUFFER_TOO_SHORT;
        }
    }
    NdisRequest->DATA.QUERY_INFORMATION.BytesWritten = BytesWritten;
    NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded = BytesNeeded;

    if( Status != NDIS_STATUS_NOT_SUPPORTED ) {
        DBGPRINT(MP_TRACE, ("==== MPQueryInformation, OID=0x%08x, Status=%x\n", Oid, Status));
    }

    return(Status);
}   

NDIS_STATUS NICGetStatsCounters(
    /*IN*/  PMP_ADAPTER  Adapter, 
    /*IN*/  NDIS_OID     Oid,
    /*OUT*/ PULONG64     pCounter
    )
/*++
Routine Description:

    Get the value for a statistics OID

Arguments:

    Adapter     Pointer to our adapter 
    Oid         Self-explanatory   
    pCounter    Pointer to receive the value
    
Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_NOT_SUPPORTED
    
--*/
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    PNDIS_STATISTICS_INFO StatisticsInfo;

    DBGPRINT(MP_TRACE, ("--> NICGetStatsCounters\n"));

    *pCounter = 0; 

    switch(Oid)
    {
        case OID_GEN_XMIT_OK:
            *pCounter = Adapter->GoodTransmits;
            break;

        case OID_GEN_RCV_OK:
            *pCounter = Adapter->GoodReceives;
            break;

        case OID_GEN_XMIT_ERROR:
            *pCounter = Adapter->TxAbortExcessCollisions +
                        Adapter->TxDmaUnderrun +
                        Adapter->TxLostCRS +
                        Adapter->TxLateCollisions;
            break;

        case OID_GEN_RCV_ERROR:
            *pCounter = Adapter->RcvCrcErrors +
                        Adapter->RcvAlignmentErrors +
                        Adapter->RcvDmaOverrunErrors +
                        Adapter->RcvRuntErrors;
            break;

        case OID_GEN_RCV_NO_BUFFER:
            *pCounter = Adapter->RcvResourceErrors;
            break;

        case OID_GEN_RCV_CRC_ERROR:
            *pCounter = Adapter->RcvCrcErrors;
            break;

        case OID_GEN_TRANSMIT_QUEUE_LENGTH:
            *pCounter = Adapter->nWaitSend;
            break;

        case OID_802_3_RCV_ERROR_ALIGNMENT:
            *pCounter = Adapter->RcvAlignmentErrors;
            break;

        case OID_802_3_XMIT_ONE_COLLISION:
            *pCounter = Adapter->OneRetry;
            break;

        case OID_802_3_XMIT_MORE_COLLISIONS:
            *pCounter = Adapter->MoreThanOneRetry;
            break;

        case OID_802_3_XMIT_DEFERRED:
            *pCounter = Adapter->TxOKButDeferred;
            break;

        case OID_802_3_XMIT_MAX_COLLISIONS:
            *pCounter = Adapter->TxAbortExcessCollisions;
            break;

        case OID_802_3_RCV_OVERRUN:
            *pCounter = Adapter->RcvDmaOverrunErrors;
            break;

        case OID_802_3_XMIT_UNDERRUN:
            *pCounter = Adapter->TxDmaUnderrun;
            break;

        case OID_802_3_XMIT_HEARTBEAT_FAILURE:
            *pCounter = Adapter->TxLostCRS;
            break;

        case OID_802_3_XMIT_TIMES_CRS_LOST:
            *pCounter = Adapter->TxLostCRS;
            break;

        case OID_802_3_XMIT_LATE_COLLISIONS:
            *pCounter = Adapter->TxLateCollisions;
            break;

        case OID_GEN_RCV_DISCARDS:
            *pCounter = Adapter->RcvCrcErrors +
                        Adapter->RcvAlignmentErrors +
                        Adapter->RcvResourceErrors +
                        Adapter->RcvDmaOverrunErrors +
                        Adapter->RcvRuntErrors;
            break;

        case OID_GEN_DIRECTED_BYTES_XMIT:
            *pCounter = Adapter->OutUcastOctets;
                break;
        case OID_GEN_DIRECTED_FRAMES_XMIT:
            *pCounter = Adapter->OutUcastPkts;
                break;
        case OID_GEN_MULTICAST_BYTES_XMIT:
            *pCounter = Adapter->OutMulticastOctets;
                break;
        case OID_GEN_MULTICAST_FRAMES_XMIT:
            *pCounter = Adapter->OutMulticastPkts;
                break;
        case OID_GEN_BROADCAST_BYTES_XMIT:
            *pCounter = Adapter->OutBroadcastOctets;
                break;
        case OID_GEN_BROADCAST_FRAMES_XMIT:
            *pCounter = Adapter->OutBroadcastPkts;
                break;
        case OID_GEN_DIRECTED_BYTES_RCV:
            *pCounter = Adapter->InUcastOctets;
                break;
        case OID_GEN_DIRECTED_FRAMES_RCV:
            *pCounter = Adapter->InUcastPkts;
            break;
        case OID_GEN_MULTICAST_BYTES_RCV:
            *pCounter = Adapter->InMulticastOctets;
                break;
        case OID_GEN_MULTICAST_FRAMES_RCV:
            *pCounter = Adapter->InMulticastOctets;
            break;
        case OID_GEN_BROADCAST_BYTES_RCV:
            *pCounter = Adapter->InBroadcastOctets;
            break;
        case OID_GEN_BROADCAST_FRAMES_RCV:
            *pCounter = Adapter->InBroadcastPkts;
            break;
        case OID_GEN_STATISTICS:

            StatisticsInfo = (PNDIS_STATISTICS_INFO)pCounter;
            StatisticsInfo->Header.Revision = NDIS_OBJECT_REVISION_1;
            StatisticsInfo->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
            StatisticsInfo->Header.Size = sizeof(NDIS_STATISTICS_INFO);
            StatisticsInfo->SupportedStatistics = NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS          |
                                                  NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR             |
                                                  NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR            |
                                                  NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV   |
                                                  NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV  |
                                                  NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV  |
                                                  NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV             | 
                                                  NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT  | 
                                                  NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT |
                                                  NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT;            

            StatisticsInfo->ifInDiscards = Adapter->RcvCrcErrors +
                                           Adapter->RcvAlignmentErrors +
                                           Adapter->RcvResourceErrors +
                                           Adapter->RcvDmaOverrunErrors +
                                           Adapter->RcvRuntErrors;
            
            StatisticsInfo->ifInErrors = StatisticsInfo->ifInDiscards -
                                         Adapter->RcvResourceErrors;
            
            StatisticsInfo->ifHCInOctets = Adapter->InBroadcastOctets + Adapter->InMulticastOctets + Adapter->InUcastOctets;         
            StatisticsInfo->ifHCInUcastPkts = Adapter->InUcastPkts;
            StatisticsInfo->ifHCInMulticastPkts = Adapter->InMulticastPkts;  
            StatisticsInfo->ifHCInBroadcastPkts = Adapter->InBroadcastPkts;  
            StatisticsInfo->ifHCOutOctets = Adapter->OutMulticastOctets + Adapter->OutBroadcastOctets + Adapter->OutUcastOctets;        
            StatisticsInfo->ifHCOutUcastPkts = Adapter->OutUcastPkts;     
            StatisticsInfo->ifHCOutMulticastPkts = Adapter->OutMulticastPkts; 
            StatisticsInfo->ifHCOutBroadcastPkts = Adapter->OutBroadcastPkts; 
            StatisticsInfo->ifOutErrors = Adapter->TxAbortExcessCollisions +
                                          Adapter->TxDmaUnderrun +
                                          Adapter->TxLostCRS +
                                          Adapter->TxLateCollisions;
            StatisticsInfo->ifOutDiscards = 0;


            break;


        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;
    }

    DBGPRINT(MP_TRACE, ("<-- NICGetStatsCounters\n"));

    return(Status);
}

NDIS_STATUS NICSetPacketFilter(
    /*IN*/ PMP_ADAPTER Adapter,
    /*IN*/ ULONG PacketFilter
    )
/*++
Routine Description:

    This routine will set up the adapter so that it accepts packets 
    that match the specified packet filter.  The only filter bits   
    that can truly be toggled are for broadcast and promiscuous     

Arguments:
    
    Adapter         Pointer to our adapter
    PacketFilter    The new packet filter 
    
Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_NOT_SUPPORTED
    
--*/
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;

    DBGPRINT(MP_TRACE, ("--> NICSetPacketFilter, PacketFilter=%08x\n", PacketFilter));

    DBGPRINT_S(Status, ("<-- NICSetPacketFilter, Status=%x\n", Status));

    return(Status);
}

NDIS_STATUS MpSetInformation(
    /*IN*/ NDIS_HANDLE           MiniportAdapterContext,
    /*IN*/ PNDIS_OID_REQUEST     NdisRequest
    )
/*++
Routine Description:

    This is the handler for an OID set operation.
    The only operations that really change the configuration of the adapter are
    set PACKET_FILTER, and SET_MULTICAST.       
    
Arguments:
    
    MiniportAdapterContext  Pointer to the adapter structure
    NdisRequest             Pointer to the request needed to process
    
Return Value:

    NDIS_STATUS_SUCCESS        
    NDIS_STATUS_INVALID_LENGTH 
    NDIS_STATUS_INVALID_OID    
    NDIS_STATUS_NOT_SUPPORTED  
    NDIS_STATUS_NOT_ACCEPTED   
    
--*/
{
    NDIS_STATUS                 Status = NDIS_STATUS_SUCCESS;
    PMP_ADAPTER                 Adapter = (PMP_ADAPTER) MiniportAdapterContext;
    NDIS_OID                    Oid;
    PVOID                       InformationBuffer;
    ULONG                       InformationBufferLength;
    ULONG                       BytesRead;
    ULONG                       BytesNeeded;
    
    ULONG                       PacketFilter;
    NDIS_DEVICE_POWER_STATE     NewPowerState;
    ULONG                       CustomDriverSet;
    
    // DEBUGP(MP_VERY_LOUD, ("---> MPSetInformation %s\n", DbgGetOidName(Oid)));

    Oid = NdisRequest->DATA.SET_INFORMATION.Oid;
    InformationBuffer = NdisRequest->DATA.SET_INFORMATION.InformationBuffer;
    InformationBufferLength = NdisRequest->DATA.SET_INFORMATION.InformationBufferLength;

    BytesRead = 0;
    BytesNeeded = 0;

    switch(Oid)
    {
        case OID_GEN_CURRENT_LOOKAHEAD:
            //
            // Verify the Length
            //
            if (InformationBufferLength < sizeof(ULONG))
            {
                BytesNeeded = sizeof(ULONG);
                Status = NDIS_STATUS_INVALID_LENGTH;
                break;
            }

            if (*(UNALIGNED PULONG)InformationBuffer > (NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE))
            {
                Status = NDIS_STATUS_INVALID_DATA;
                break;
            }
                    
            NdisMoveMemory(&Adapter->ulLookAhead, InformationBuffer, sizeof(ULONG));
            
            BytesRead = sizeof(ULONG);
            Status = NDIS_STATUS_SUCCESS;
            break;

        case OID_GEN_CURRENT_PACKET_FILTER:
            //
            // Verify the Length
            //
            if (InformationBufferLength != sizeof(ULONG))
            {
                return(NDIS_STATUS_INVALID_LENGTH);
            }

            BytesRead = InformationBufferLength;

            NdisMoveMemory(&PacketFilter,
                          InformationBuffer,
                          sizeof(ULONG));

            //
            // any bits not supported?
            //
            if (PacketFilter & ~NIC_SUPPORTED_FILTERS)
            {
                return(NDIS_STATUS_NOT_SUPPORTED);
            }

            //
            // any filtering changes?
            //
            if (PacketFilter == Adapter->PacketFilter)
            {
                return(NDIS_STATUS_SUCCESS);
            }

            NdisAcquireSpinLock(&Adapter->Lock);
            NdisAcquireSpinLock(&Adapter->RcvLock);
            
            Status = NICSetPacketFilter(
                         Adapter,
                         PacketFilter);

            NdisReleaseSpinLock(&Adapter->RcvLock);
            NdisReleaseSpinLock(&Adapter->Lock);
            if (NT_SUCCESS(Status))
            {
                Adapter->PacketFilter = PacketFilter;
            }

            break;

        case OID_PNP_SET_POWER:

            DBGPRINT(MP_LOUD, ("SET: Power State change, "PTR_FORMAT"\n", InformationBuffer));

            if (InformationBufferLength != sizeof(NDIS_DEVICE_POWER_STATE ))
            {
                return(NDIS_STATUS_INVALID_LENGTH);
            }

            NewPowerState = *(PNDIS_DEVICE_POWER_STATE UNALIGNED)InformationBuffer;

            //
            // Set the power state - Cannot fail this request
            //
            Status = MPSetPower( Adapter, NewPowerState );

            if( Status == NDIS_STATUS_PENDING ) {
                Adapter->PendingRequest = NdisRequest;
                break;
            }

            if( Status != NDIS_STATUS_SUCCESS ) {
                DBGPRINT(MP_ERROR, ("SET power: Hardware error !!!\n"));
                break;
            }
        
            BytesRead = sizeof(NDIS_DEVICE_POWER_STATE);
            Status = NDIS_STATUS_SUCCESS; 
            break;
        case OID_802_3_ADD_MULTICAST_ADDRESS:
            Status = NDIS_STATUS_SUCCESS;
            break;
        case OID_802_3_DELETE_MULTICAST_ADDRESS:
            Status = NDIS_STATUS_SUCCESS;
            break;
        default:
            Status = NDIS_STATUS_NOT_SUPPORTED;
            break;

    }

    if (NT_SUCCESS(Status))
    {
        NdisRequest->DATA.SET_INFORMATION.BytesRead = BytesRead;
    }
    NdisRequest->DATA.SET_INFORMATION.BytesNeeded = BytesNeeded;

    if( Status != NDIS_STATUS_NOT_SUPPORTED ) {
        DBGPRINT(MP_TRACE, ("==== MpSetInformationSet, OID=0x%08x, Status=%x\n", Oid, Status));
    }

    return(Status);
}

PNET_BUFFER_LIST
MPFillSendBuffer(
    PMP_ADAPTER Adapter,
    PUCHAR SendBuffer,
    ULONG SendBufferLength)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    PNET_BUFFER NetBuffer;
    PMDL Mdl;
    PSEND_BUFFER_CONTEXT SendBufferContext;
    PNET_BUFFER_LIST NetBufferList = NULL;
    PMP_NET_BUFFER_LIST_PRIVATE Private;

    Mdl = NdisAllocateMdl( Adapter->AdapterHandle, SendBuffer, SendBufferLength );

    if( Mdl == NULL ) {
        DBGPRINT( MP_WARN, ("failed to allocate send mdl\n"));
        Status = NDIS_STATUS_RESOURCES;
        goto end;
    }

    NetBufferList = NdisAllocateNetBufferAndNetBufferList(
            Adapter->RpcSendNetBufferListPool,
            sizeof(SEND_BUFFER_CONTEXT), //ContextSize
            0, //backfill size
            Mdl,
            0, 
            0 );

    if( NetBufferList == NULL ) {
        DBGPRINT( MP_WARN, ("failed to allocate net buffer\n"));
        NdisFreeMdl( Mdl );
        Status = NDIS_STATUS_RESOURCES;
        goto end;
    }

    SendBufferContext = (PSEND_BUFFER_CONTEXT)NET_BUFFER_LIST_CONTEXT_DATA_START( NetBufferList );
    SendBufferContext->Mdl = Mdl;
    SendBufferContext->Buffer = SendBuffer;
    SendBufferContext->Length = SendBufferLength;
    SendBufferContext->Offset = 0;
    
    
    NetBufferList->SourceHandle = MP_GET_ADAPTER_HANDLE( Adapter );
  
    Private = MP_ALLOCMEMTAG( Adapter->AdapterHandle, sizeof( MP_NET_BUFFER_LIST_PRIVATE ) );
    if( Private ) {
        Private->Type = SEND_BUFFER_ORIGIN_RPC;
        Private->RefCount = 0;
        Private->Next = NULL;
        MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) = (PVOID)Private;
    } else {
        DBGPRINT( MP_WARN, ("failed to allocate priv for net buffer\n"));
        NdisFreeMdl( Mdl );
        NdisFreeNetBufferList( NetBufferList );
        NetBufferList = NULL;
        Status = NDIS_STATUS_RESOURCES;
        goto end;
    }

    NetBuffer = NET_BUFFER_LIST_FIRST_NB( NetBufferList );

    //
    // During the call NdisAllocateNetBufferAndNetBufferList to allocate the NET_BUFFER_LIST, NDIS already
    // initializes DataOffset, CurrentMdl and CurrentMdlOffset, here the driver needs to update DataLength
    // in the NET_BUFFER to reflect the received frame size.
    // 
    NET_BUFFER_DATA_LENGTH(NetBuffer) = SendBufferLength;
    NdisAdjustMdlLength( Mdl, SendBufferLength );
    NdisFlushBuffer( Mdl, FALSE);

end:
    return NetBufferList;
}

VOID
MPSendSlotNumberMsg(
        /*IN*/ PMP_ADAPTER Adapter )
{
    PNET_BUFFER_LIST NetBufferList;
    PUCHAR SendBuffer;
    ULONG SendBufferLength;

    SendBufferLength = 8;
    SendBuffer = MP_ALLOCMEMTAG( Adapter->AdapterHandle, SendBufferLength );
    if (!SendBuffer) {
        return;
    }
    SendBuffer[0] = CONTROL_MSG_SLOT_NUMBER;//msg id
    *((ULONG*)&SendBuffer[4]) = RtlUlongByteSwap( Adapter->BoardNumber );

    NetBufferList = MPFillSendBuffer( Adapter, SendBuffer, SendBufferLength );

    if( NetBufferList ) {
        DBGPRINT( MP_WARN, ("Sending slot number msg in Net buffer\n") );
        //NDIS_SEND_FLAGS_DISPATCH_LEVEL or 0
        MPSendNetBufferListsCommon( Adapter, NetBufferList, 0, 0 );
    } else {
        MP_FREEMEM( SendBuffer );
        DBGPRINT( MP_WARN, ("Failed to allocate net buffer list for slot number msg\n"));
    }
}

#define CONTROL_MSG_POLL_MASK_TABLE_SETUP   0x0A
VOID
MPSendPollMaskSetupMsg(
        /*IN*/ PMP_ADAPTER Adapter )
{
    PNET_BUFFER_LIST NetBufferList;
    PUCHAR SendBuffer;
    ULONG SendBufferLength;

    SendBufferLength = 4;
    SendBuffer = MP_ALLOCMEMTAG( Adapter->AdapterHandle, SendBufferLength );
    if (!SendBuffer) {
        return;
    }
    SendBuffer[0] = CONTROL_MSG_POLL_MASK_TABLE_SETUP;//msg id

    NetBufferList = MPFillSendBuffer( Adapter, SendBuffer, SendBufferLength );

    if( NetBufferList ) {
        DBGPRINT( MP_WARN, ("Sending poll mask table setup msg in Net buffer\n") );
        //NDIS_SEND_FLAGS_DISPATCH_LEVEL or 0
        MPSendNetBufferListsCommon( Adapter, NetBufferList, 0, 0 );
    } else {
        MP_FREEMEM( SendBuffer );
        DBGPRINT( MP_WARN, ("Failed to allocate net buffer list for poll mask table setup msg\n"));
    }
}

VOID
MPSetPowerD0(
    PMP_ADAPTER  Adapter
    )
/*++
Routine Description:

    This routine is called when the adapter receives a SetPower 
    to D0.
    
Arguments:
    
    Adapter                 Pointer to the adapter structure
    
Return Value:

    
--*/
{
    DBGPRINT(MP_WARN, ("MPSetPowerD0\n"));
    ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_INT_ENABLE );
    MPSendPollMaskSetupMsg(Adapter);
}

NDIS_STATUS
MPSetPowerLow(
    PMP_ADAPTER              Adapter ,
    NDIS_DEVICE_POWER_STATE  PowerState 
    )
/*++
Routine Description:

    This routine is called when the adapter receives a SetPower 
    to a PowerState > D0
    
Arguments:
    
    Adapter                 Pointer to the adapter structure
    PowerState              NewPowerState
    
Return Value:
    
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_HARDWARE_ERRORS

    
--*/
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    int i;
    DBGPRINT( MP_WARN, ("MPSetPowerLow(%d)\n", PowerState));
    return Status;
}

NDIS_STATUS
MPSetPower(
    PMP_ADAPTER     Adapter ,
    NDIS_DEVICE_POWER_STATE   PowerState 
    )
/*++
Routine Description:

    This routine is called when the adapter receives a SetPower 
    request. It redirects the call to an appropriate routine to
    Set the New PowerState
    
Arguments:
    
    Adapter                 Pointer to the adapter structure
    PowerState              NewPowerState
    
Return Value:
 
    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_HARDWARE_ERRORS

    
--*/
{
    NDIS_STATUS       Status = NDIS_STATUS_SUCCESS;
    
    if ((PowerState < NdisDeviceStateD0  
        || PowerState > NdisDeviceStateD3))
    {
        Status = NDIS_STATUS_INVALID_DATA;
        return Status;
    }

    if (PowerState == NdisDeviceStateD0)
    {
        MPSetPowerD0 (Adapter);
    }
    else
    {
        Status = MPSetPowerLow (Adapter, PowerState);
    }

    return Status;
}


VOID
MPFillPoMgmtCaps (
    /*IN*/ PMP_ADAPTER                 pAdapter, 
    /*IN*/ /*OUT*/ PNDIS_PNP_CAPABILITIES  pPower_Management_Capabilities, 
    /*IN*/ /*OUT*/ PNDIS_STATUS            pStatus,
    /*IN*/ /*OUT*/ PULONG                  pulInfoLen
    )
/*++
Routine Description:

    Fills in the Power  Managment structure depending the capabilities of 
    the software driver and the card.

    Currently this is only supported on 82559 Version of the driver

Arguments:
    
    Adapter                 Pointer to the adapter structure
    pPower_Management_Capabilities - Power management struct as defined in the DDK, 
    pStatus                 Status to be returned by the request,
    pulInfoLen              Length of the pPowerManagmentCapabilites
    
Return Value:

    Success or failure depending on the type of card
--*/

{
    BOOLEAN bIsPoMgmtSupported; 
    
    bIsPoMgmtSupported = MPIsPoMgmtSupported(pAdapter);

    if (bIsPoMgmtSupported == TRUE)
    {
        NdisZeroMemory (pPower_Management_Capabilities, sizeof(*pPower_Management_Capabilities));
        *pulInfoLen = sizeof (*pPower_Management_Capabilities);
        *pStatus = NDIS_STATUS_SUCCESS;
    }
    else
    {
        NdisZeroMemory (pPower_Management_Capabilities, sizeof(*pPower_Management_Capabilities));
        *pStatus = NDIS_STATUS_NOT_SUPPORTED;
        *pulInfoLen = 0;
    }
}

VOID
MPCancelOidRequest(
    /*IN*/  NDIS_HANDLE            MiniportAdapterContext,
    /*IN*/  PVOID                  RequestId
    )
{
    PNDIS_OID_REQUEST    PendingRequest;
    PMP_ADAPTER         Adapter = (PMP_ADAPTER) MiniportAdapterContext;
    
    DBGPRINT(MP_TRACE, ("====> MPCancelOidRequest\n"));
    
    NdisAcquireSpinLock(&Adapter->Lock);
    
    if (Adapter->PendingRequest != NULL 
            && Adapter->PendingRequest->RequestId == RequestId)
    {
        PendingRequest = Adapter->PendingRequest;
        Adapter->PendingRequest = NULL;

        NdisReleaseSpinLock(&Adapter->Lock);
        
        NdisMOidRequestComplete(Adapter->AdapterHandle, 
                            PendingRequest, 
                            NDIS_STATUS_REQUEST_ABORTED);

    } 
    else
    {
        NdisReleaseSpinLock(&Adapter->Lock);
    }
    
    DBGPRINT(MP_TRACE, ("<==== MPCancelOidRequest\n"));
}

