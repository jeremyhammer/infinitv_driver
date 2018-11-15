/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_nic.c

Abstract:
    This module contains miniport send/receive routines

Revision History:

Notes:

--*/

#include "precomp.h"
#include <xfilter.h>
#if DBG
#define _FILENUMBER     'CINM'
#endif

__inline 
MP_SEND_STATS(
    /*IN*/ PMP_ADAPTER  Adapter,
    /*IN*/ PNET_BUFFER  NetBuffer
    )
/*++
Routine Description:
    This function updates the send statistics on the Adapter.
    This is called only after a NetBuffer has been sent out. 

    These should be maintained in hardware to increase performance.
    They are demonstrated here as all NDIS 6.0 miniports are required
    to support these statistics.

Arguments:

    Adapter     Pointer to our adapter
    NetBuffer   Pointer the the sent NetBuffer
Return Value:
    None

--*/
{
    PUCHAR  EthHeader;
    ULONG   Length;
    PMDL    Mdl = NET_BUFFER_CURRENT_MDL(NetBuffer);

    NdisQueryMdl(Mdl, &EthHeader, &Length, NormalPagePriority);
    if (EthHeader != NULL)
    {
        EthHeader += NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer);
        if (ETH_IS_BROADCAST(EthHeader))
        {
            Adapter->OutBroadcastPkts++;
            Adapter->OutBroadcastOctets +=NET_BUFFER_DATA_LENGTH(NetBuffer);
        }
        else if (ETH_IS_MULTICAST(EthHeader))
        {
            Adapter->OutMulticastPkts++;
            Adapter->OutMulticastOctets +=NET_BUFFER_DATA_LENGTH(NetBuffer);
        }
        else
        {
            Adapter->OutUcastPkts++;
            Adapter->OutUcastOctets += NET_BUFFER_DATA_LENGTH(NetBuffer);
        }
    }

}

__inline VOID
MP_RECEIVE_STATS(
    /*IN*/ PMP_ADAPTER  Adapter,
    /*IN*/ PNET_BUFFER  NetBuffer
    )
/*++
Routine Description:
    This function updates the receive statistics on the Adapter.
    This is incremented before the NetBufferList is indicated to NDIS. 

    These should be maintained in hardware to increase performance.
    They are demonstrated here as all NDIS 6.0 miniports are required
    to support these statistics.

Arguments:

    Adapter     Pointer to our adapter
    NetBuffer   Pointer the the sent NetBuffer
Return Value:
    None

--*/
{
    PUCHAR  EthHeader = NULL;
    ULONG   Length = 0;
    PMDL    Mdl = NET_BUFFER_CURRENT_MDL(NetBuffer);
    ULONG   NbLength = NET_BUFFER_DATA_LENGTH(NetBuffer);

    NdisQueryMdl(Mdl, &EthHeader, &Length, NormalPagePriority);
    if (EthHeader == NULL) {
        return;
    }
    
    EthHeader += NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer);
    if (ETH_IS_BROADCAST(EthHeader))
    {
        Adapter->InBroadcastPkts++;
        Adapter->InBroadcastOctets +=NbLength;
    }
    else if (ETH_IS_MULTICAST(EthHeader))
    {
        Adapter->InMulticastPkts++;
        Adapter->InMulticastOctets +=NbLength;
    }
    else
    {
        Adapter->InUcastPkts++;
        Adapter->InUcastOctets += NbLength;
    }

}

__inline PNET_BUFFER_LIST 
MP_FREE_SEND_NET_BUFFER(
    /*IN*/  PMP_ADAPTER Adapter
    )
/*++
Routine Description:

    Recycle a MP_TCB and complete the packet if necessary
    Assumption: Send spinlock has been acquired 

Arguments:

    Adapter     Pointer to our adapter
    pMpTcb      Pointer to MP_TCB        

Return Value:

    Return NULL if no NET_BUFFER_LIST is completed.
    Otherwise, return a pointer to a NET_BUFFER_LIST which has been completed.

--*/
{
    PNET_BUFFER_LIST    NetBufferList;

    NetBufferList = Adapter->SendingNetBufferList;

    Adapter->SendingNetBufferList = NULL;

    Adapter->nBusySend--;
    ASSERT(Adapter->nBusySend >= 0);
    
    //
    // SendLock is hold
    //
    if (NetBufferList)
    {
        MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList)--;
        if (MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList) == 0)
        {
            DBGPRINT(MP_TRACE, ("Completing NetBufferList= "PTR_FORMAT"\n", NetBufferList));

            NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;
            return NetBufferList;
        }
    }

    return NULL;
}

NDIS_STATUS
MiniportSendNetBufferList(
    /*IN*/  PMP_ADAPTER         Adapter,
    /*IN*/  PNET_BUFFER_LIST    NetBufferList,
    /*IN*/  BOOLEAN             bFromQueue
    )
/*++
Routine Description:

    Do the work to send a packet
    Assumption: Send spinlock has been acquired 

Arguments:

    Adapter             Pointer to our adapter
    NetBufferList       Pointer to the NetBufferList is going to send.
    bFromQueue          TRUE if it's taken from the send wait queue

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING         Put into the send wait queue
    NDIS_STATUS_HARD_ERRORS

    NOTE: SendLock is held, called at DISPATCH level
--*/
{
    NDIS_STATUS     Status = NDIS_STATUS_PENDING;
    NDIS_STATUS     SendStatus;
    ULONG           BytesCopied = 0;
    ULONG           Offset;
    PNET_BUFFER     NetBufferToSend = NULL;
    PUCHAR pDest = NULL;
    PUCHAR pSrc = NULL;
    ULONG MdlLength = 0;
    ULONG DataLength;
    PMDL CurrentMdl;

    ULONG msg_buffer_buffer = MSG_BUFFER_BUFFER;
    ULONG msg_buffer_buffer_length = MSG_BUFFER_BUFFER;
   
    DBGPRINT(MP_TRACE, ("--> MiniportSendNetBufferList, NetBufferList="PTR_FORMAT"\n", 
                            NetBufferList));
    
    SendStatus = NDIS_STATUS_SUCCESS;

    if( !MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) ) {
        DBGPRINT( MP_ERROR, ("net buffer list private data was NULL at send\n"));
        return NDIS_STATUS_FAILURE;
    }


    if( MP_GET_NET_BUFFER_LIST_TYPE( NetBufferList ) == SEND_BUFFER_ORIGIN_ETHERNET ) {

        if (bFromQueue)
        {
            NetBufferToSend = MP_GET_NET_BUFFER_LIST_NEXT_SEND(NetBufferList);
            MP_GET_NET_BUFFER_LIST_NEXT_SEND(NetBufferList) = NULL;
        }
        else
        {
            NetBufferToSend = NET_BUFFER_LIST_FIRST_NB(NetBufferList);
        }

        DataLength = NET_BUFFER_DATA_LENGTH( NetBufferToSend );
        Offset = NET_BUFFER_DATA_OFFSET( NetBufferToSend );
        CurrentMdl = NET_BUFFER_FIRST_MDL( NetBufferToSend );
        pDest = Adapter->MsgBase + msg_buffer_buffer;

        if( DataLength > msg_buffer_buffer_length ) {
            DBGPRINT( MP_ERROR, ("DataLength %d was too long to send\n", DataLength));
            Status = NDIS_STATUS_RESOURCES;
            goto finish;
        }

        while( CurrentMdl && DataLength > 0 ) {

            NdisQueryMdl( CurrentMdl, &pSrc, &MdlLength, NormalPagePriority );

            if( pSrc == NULL ) {
                BytesCopied = 0;
                break;
            }

            if( MdlLength > Offset ) {
                pSrc += Offset;
                MdlLength -= Offset;

                if( MdlLength > DataLength ) {
                    MdlLength = DataLength;
                }

                DataLength -= MdlLength;
                NdisMoveMemory( pDest, pSrc, MdlLength );
                BytesCopied += MdlLength;

                pDest += MdlLength;
                Offset = 0;

            } else {
                Offset -= MdlLength;
            }

            NdisGetNextMdl( CurrentMdl, &CurrentMdl );
        }

        ctn91xx_write32( MSG_BUFFER_TAG_ETHERNET, Adapter->MsgBase, MSG_BUFFER_LOCAL_TAG );
        ctn91xx_write32( MSG_BUFFER_TAG_ETHERNET, Adapter->MsgBase, MSG_BUFFER_TAG );
    } else if( MP_GET_NET_BUFFER_LIST_TYPE( NetBufferList ) == SEND_BUFFER_ORIGIN_RPC ) {

        PSEND_BUFFER_CONTEXT SendBufferContext = (PSEND_BUFFER_CONTEXT)NET_BUFFER_LIST_CONTEXT_DATA_START( NetBufferList );
        BOOLEAN PayloadStart = SendBufferContext->Offset == 0;
        ULONG PktDataLength = PayloadStart ? 
            ( msg_buffer_buffer_length - SIZEOF_CONTROL_MSG_FIRST_HEADER ) :
            ( msg_buffer_buffer_length - SIZEOF_CONTROL_MSG_HEADER );

        ULONG BytesLeft = SendBufferContext->Length - SendBufferContext->Offset;
        ULONG CurBufferLength = min( BytesLeft, PktDataLength );

            
        NetBufferToSend = NET_BUFFER_LIST_FIRST_NB( NetBufferList );

        if( BytesLeft <= PktDataLength ) {
            NET_BUFFER_NEXT_NB( NetBufferToSend ) = NULL;
        } else {
            //we aren't done yet, just make the code below requeue us
            NET_BUFFER_NEXT_NB( NetBufferToSend ) = NetBufferToSend;
            MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList)++;
        }

        if( SendBufferContext->Offset == 0 ) {
            //do payload start and length
            ctn91xx_write8( 1, Adapter->MsgBase, msg_buffer_buffer );
            ctn91xx_write32( RtlUlongByteSwap( SendBufferContext->Length ), Adapter->MsgBase, msg_buffer_buffer + 4 );
            MP_MEMCOPY( Adapter->MsgBase + msg_buffer_buffer + 8, SendBufferContext->Buffer + SendBufferContext->Offset, CurBufferLength );
            BytesCopied = CurBufferLength + 8;
        } else {
            //just payload start
            ctn91xx_write8( 0, Adapter->MsgBase, msg_buffer_buffer );
            MP_MEMCOPY( Adapter->MsgBase + msg_buffer_buffer + 4, SendBufferContext->Buffer + SendBufferContext->Offset, CurBufferLength );
            BytesCopied = CurBufferLength + 4;
        }

        SendBufferContext->Offset += CurBufferLength;
        DBGPRINT( MP_WARN, ("sending control msg length: %d\n", CurBufferLength ));

        ctn91xx_write32( MSG_BUFFER_TAG_CONTROL, Adapter->MsgBase, MSG_BUFFER_LOCAL_TAG );
        ctn91xx_write32( MSG_BUFFER_TAG_CONTROL, Adapter->MsgBase, MSG_BUFFER_TAG );
    } else {
        Status = NDIS_STATUS_RESOURCES;
        goto finish;
    }

    ctn91xx_write16( (USHORT)BytesCopied, Adapter->MsgBase, MSG_BUFFER_MSG_LEN );
    ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_MSG_AVAIL );
   
    //
    // Look at the next entry
    //
    if( NetBufferToSend ) {
        NetBufferToSend = NET_BUFFER_NEXT_NB( NetBufferToSend );
    }

    //
    // If there are more net buffers to send
    // 
    if( NetBufferToSend )
    {
        //
        // Put NetBufferList into wait queue
        //
        MP_GET_NET_BUFFER_LIST_NEXT_SEND(NetBufferList) = NetBufferToSend;
        if (!bFromQueue)
        {
            InsertHeadQueue(&Adapter->SendWaitQueue,
                            MP_GET_NET_BUFFER_LIST_LINK(NetBufferList));
            Adapter->nWaitSend++;
        }
        //
        // The NetBufferList is already in the queue, we don't do anything
        //
    }

    Adapter->nBusySend++;

finish:
    //
    // All the NetBuffers in the NetBufferList has been processed,
    // If the NetBufferList is in queue now, dequeue it.
    //
    if (NetBufferToSend == NULL)
    {
        DBGPRINT(MP_TRACE, ("Sent all buffers in NetBufferList\n"));

        if (bFromQueue)
        {
            RemoveHeadQueue(&Adapter->SendWaitQueue);
            Adapter->nWaitSend--;
        }
    }

    //
    // As far as the miniport knows, the NetBufferList has been sent out.
    // Complete the NetBufferList now
    //
    if (MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList) == 0)
    {
        MP_RELEASE_SPIN_LOCK(&Adapter->SendLock, TRUE);
        NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;
        MPHandleSendNetBufferListsComplete( Adapter, NetBufferList, SendStatus, TRUE );
        MP_ACQUIRE_SPIN_LOCK(&Adapter->SendLock, TRUE);
    } 
            
    DBGPRINT(MP_TRACE, ("<-- MiniportSendNetBufferList\n"));
    return Status;
}  


NDIS_STATUS
MpHandleSendInterrupt(
    /*IN*/  PMP_ADAPTER  Adapter
    )
/*++
Routine Description:

    Interrupt handler for sending processing
    Re-claim the send resources, complete sends and get more to send from the send wait queue
    Assumption: Send spinlock has been acquired 

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_HARD_ERRORS
    NDIS_STATUS_PENDING

--*/
{
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;
    PNET_BUFFER_LIST    NetBufferList;
    PNET_BUFFER_LIST    LastNetBufferList = NULL;
    PNET_BUFFER_LIST    CompleteNetBufferLists = NULL;

    DBGPRINT(MP_TRACE, ("---> MpHandleSendInterrupt\n"));

    //
    // Any packets being sent? Any packet waiting in the send queue?
    //
    if (Adapter->nBusySend == 0 &&
        IsQueueEmpty(&Adapter->SendWaitQueue))
    {
        DBGPRINT(MP_TRACE, ("<--- MpHandleSendInterrupt\n"));
        return Status;
    }

    if( Adapter->MsgBufferSendPending ) {

        Adapter->MsgBufferSendPending = 0;

        Adapter->GoodTransmits++;

        //decrements nBusySend
        NetBufferList = MP_FREE_SEND_NET_BUFFER(Adapter);

        if( NetBufferList != NULL ) {

            NET_BUFFER_LIST_STATUS( NetBufferList ) = NDIS_STATUS_SUCCESS;

            CompleteNetBufferLists = NetBufferList;
            NET_BUFFER_LIST_NEXT_NBL( NetBufferList ) = NULL;
            LastNetBufferList = NetBufferList;
        }

    } else {
        return Status;
    }

    //
    // If we queued any transmits because we didn't have any TCBs earlier,
    // dequeue and send those packets now, as long as we have free TCBs.
    //
    {
        PQUEUE_ENTRY pEntry; 
        
        //
        // We cannot remove it now, we just need to get the head
        //
        pEntry = GetHeadQueue(&Adapter->SendWaitQueue);
        if( pEntry ) {
            NetBufferList = MP_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK (pEntry);
    
            DBGPRINT(MP_TRACE, ("MpHandleSendInterrupt - send a queued NetBufferList\n"));
            
            if( Adapter->SendingNetBufferList != NULL ) {

                DBGPRINT( MP_WARN, ("Completing net buffer list when shouldn't\n"));

                NET_BUFFER_LIST_STATUS(Adapter->SendingNetBufferList) = NDIS_STATUS_FAILURE;
                NET_BUFFER_LIST_NEXT_NBL(Adapter->SendingNetBufferList) = NULL;
                Adapter->nBusySend--;
               
                NdisDprReleaseSpinLock(&Adapter->SendLock);
                MPHandleSendNetBufferListsComplete( Adapter, Adapter->SendingNetBufferList, NDIS_STATUS_FAILURE, TRUE );
                NdisDprAcquireSpinLock(&Adapter->SendLock);

                Adapter->SendingNetBufferList = NULL;
            }

            ASSERT( Adapter->SendingNetBufferList == NULL );
            Adapter->SendingNetBufferList = NetBufferList;
            Status = MiniportSendNetBufferList(Adapter, NetBufferList, TRUE);
        }
    }

    //
    // Complete the NET_BUFFER_LISTs 
    //
    if (CompleteNetBufferLists != NULL)
    {
        NdisDprReleaseSpinLock(&Adapter->SendLock);
        
        MPHandleSendNetBufferListsComplete( Adapter, CompleteNetBufferLists, Status, TRUE );

        NdisDprAcquireSpinLock(&Adapter->SendLock);
    }

    DBGPRINT(MP_TRACE, ("<--- MpHandleSendInterrupt\n"));
    return Status;
}

VOID MpHandleControlMsg(
    /*IN*/  PMP_ADAPTER     Adapter )
{
    switch( Adapter->RpcBuffer[0] ) {
        case CONTROL_MSG_DISCONNECT_CABLE:
            DBGPRINT( MP_WARN, ("control msg: disconnect cable\n") );
            Adapter->ForceDisconnectCable = TRUE;
            NICUpdateLinkState( Adapter, TRUE );
            break;
        case CONTROL_MSG_CONNECT_CABLE:
            DBGPRINT( MP_WARN, ("control msg: connect cable\n") );
            Adapter->ForceDisconnectCable = FALSE;
            NICUpdateLinkState( Adapter, TRUE );
            break;
        case CONTROL_MSG_MASK_TABLE_SETUP:
            DBGPRINT( MP_WARN, ("control msg: mask table setup\n") );
            MpegDeinitDma( Adapter );
            MpegInitDma( Adapter );
            MPSendSlotNumberMsg( Adapter );
            break;
        case CONTROL_MSG_RTP_SETUP:
            DBGPRINT( MP_WARN, ("control msg: rtp setup\n"));
            RtpSetup( Adapter, Adapter->RpcBuffer + 4, Adapter->RpcSize - 4 );
            break;
        case CONTROL_MSG_RTP_DESTROY:
            DBGPRINT( MP_WARN, ("control msg: rtp destroy\n"));
            RtpDestroy( Adapter, Adapter->RpcBuffer + 4, Adapter->RpcSize - 4 );
            break;
        case CONTROL_MSG_PBDA_TAG_TABLE:
            DBGPRINT( MP_WARN, ("control msg: pbda tag table\n"));
            RtpInject( Adapter, Adapter->RpcBuffer + 4, Adapter->RpcSize - 4 );
            break;
        default:
            break;
    }
}

VOID MpHandleControlInterrupt(
    /*IN*/  PMP_ADAPTER     Adapter)
{
    ULONG msg_buffer_buffer = MSG_BUFFER_BUFFER;

    if( Adapter->MsgBufferControlPending ) {
        BOOLEAN PayloadStart = FALSE;
        USHORT MsgChunkLen = 0;

        Adapter->MsgBufferControlPending = 0;

        PayloadStart = ctn91xx_read8( Adapter->MsgBase, msg_buffer_buffer );
        MsgChunkLen = ctn91xx_read16( Adapter->MsgBase, MSG_BUFFER_MSG_LEN );

        if( Adapter->RpcBuffer ) {
            if( !PayloadStart ) {
                MP_MEMCOPY( Adapter->RpcBuffer + Adapter->RpcOffset, Adapter->MsgBase + msg_buffer_buffer + 4, MsgChunkLen - 4 );
                Adapter->RpcOffset += MsgChunkLen - 4;
                if( Adapter->RpcSize == Adapter->RpcOffset ) {
                    MpHandleControlMsg( Adapter );
                    MP_FREEMEM( Adapter->RpcBuffer );
                    Adapter->RpcOffset = 0;
                    Adapter->RpcSize = 0;
                } else if( Adapter->RpcSize < Adapter->RpcOffset ) {
                    DBGPRINT( MP_WARN, ("RPC length exceeded specified size\n"));
                }
            }
        } else {
            if( PayloadStart ) {
                ULONG MsgLen = RtlUlongByteSwap( ctn91xx_read32( Adapter->MsgBase, msg_buffer_buffer + 4 ) );
                Adapter->RpcBuffer = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MsgLen );
                if( Adapter->RpcBuffer ) {
                    Adapter->RpcSize = MsgLen;
                    Adapter->RpcOffset = MsgChunkLen - 8;
                    MP_MEMCOPY( Adapter->RpcBuffer, Adapter->MsgBase + msg_buffer_buffer + 8, MsgChunkLen - 8 );

                    if( Adapter->RpcSize == Adapter->RpcOffset ) {
                        MpHandleControlMsg( Adapter );
                        MP_FREEMEM( Adapter->RpcBuffer );
                        Adapter->RpcOffset = 0;
                        Adapter->RpcSize = 0;
                    } else if( Adapter->RpcSize < Adapter->RpcOffset ) {
                        DBGPRINT( MP_WARN, ("RPC length exceeded specified size\n"));
                    }
                } else {
                    DBGPRINT( MP_WARN, ("dropping rpc because allocation failed\n"));
                }
            }
        }

        //Let the other side know we got it
        //
        ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_MSG_RECV );
    }
}

VOID
MpHandleRecvBuffer(
    /*IN*/  PMP_ADAPTER         Adapter,
    /*IN*/  MP_BUFFER_PACKER    BufferPacker,
    /*IN*/  ULONG               Length,
    /*IN*/  PVOID               Context
    )
{
    PMP_RFD             pMpRfd;

    PNET_BUFFER_LIST    FreeNetBufferList;
    UINT                NetBufferListCount;
    UINT                NetBufferListFreeCount;

    BOOLEAN             bAllocNewRfd = FALSE;
    PNET_BUFFER_LIST    NetBufferList = NULL; 
    PNET_BUFFER_LIST    PrevNetBufferList = NULL;
    PNET_BUFFER_LIST    PrevFreeNetBufferList = NULL;
    PNET_BUFFER         NetBuffer;
    LONG                Count;
    ULONG               ReceiveFlags = NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL;

    //
    // add an extra receive ref just in case we indicate up with status resources
    //
    MP_INC_RCV_REF(Adapter);
    
    NetBufferListCount = 0;
    NetBufferListFreeCount = 0;

    NetBufferList = NULL;
    FreeNetBufferList = NULL;
    

    if (IsListEmpty(&Adapter->RecvList))
    {
        ASSERT(Adapter->nReadyRecv == 0);
        goto cleanup;
    }

    //
    // Get the next MP_RFD to process
    //
    pMpRfd = (PMP_RFD)GetListHeadEntry(&Adapter->RecvList);
        
    Adapter->GoodReceives++;
	
    //
    // Remove the RFD from the head of the List
    //
    RemoveEntryList((PLIST_ENTRY)pMpRfd);
    Adapter->nReadyRecv--;
    ASSERT(Adapter->nReadyRecv >= 0);
    
    ASSERT(MP_TEST_FLAG(pMpRfd, fMP_RFD_RECV_READY));
    MP_CLEAR_FLAG(pMpRfd, fMP_RFD_RECV_READY);

    //
    // Get the packet size
    //
    //
    pMpRfd->PacketSize = Length;

    NetBuffer = NET_BUFFER_LIST_FIRST_NB(pMpRfd->NetBufferList);
    //
    // During the call NdisAllocateNetBufferAndNetBufferList to allocate the NET_BUFFER_LIST, NDIS already
    // initializes DataOffset, CurrentMdl and CurrentMdlOffset, here the driver needs to update DataLength
    // in the NET_BUFFER to reflect the received frame size.
    // 
    NET_BUFFER_DATA_LENGTH(NetBuffer) = pMpRfd->PacketSize;
    NdisAdjustMdlLength(pMpRfd->Mdl, pMpRfd->PacketSize);
    NdisFlushBuffer(pMpRfd->Mdl, FALSE);

    //
    //Copy the data in
    //
    BufferPacker( Adapter, pMpRfd->RfdBuffer, pMpRfd->PacketSize, Context );
    
    //
    // Update the statistics
    //
    MP_RECEIVE_STATS(Adapter, NetBuffer);

    //
    // set the status on the packet, either resources or success
    //
    if (Adapter->nReadyRecv >= MIN_NUM_RFD)
    {
        //
        // Success case: NDIS will pend the NetBufferLists
        // 
        MP_SET_FLAG(pMpRfd, fMP_RFD_RECV_PEND);
        
        InsertTailList(&Adapter->RecvPendList, (PLIST_ENTRY)pMpRfd);
        MP_INC_RCV_REF(Adapter);
        if (NetBufferList == NULL)
        {
            NetBufferList = pMpRfd->NetBufferList;
        }
        else
        {
            NET_BUFFER_LIST_NEXT_NBL(PrevNetBufferList) = pMpRfd->NetBufferList;
        }
        NET_BUFFER_LIST_NEXT_NBL(pMpRfd->NetBufferList) = NULL;
        MP_CLEAR_FLAG(pMpRfd->NetBufferList, (NET_BUFFER_LIST_FLAGS(pMpRfd->NetBufferList) & NBL_FLAGS_MINIPORT_RESERVED));
        PrevNetBufferList = pMpRfd->NetBufferList;
    }
    else
    {
        //
        // Resources case: Miniport will retain ownership of the NetBufferLists
        //
        MP_SET_FLAG(pMpRfd, fMP_RFD_RESOURCES);

        if (FreeNetBufferList == NULL)
        {
            FreeNetBufferList = pMpRfd->NetBufferList;
        }
        else
        {
            NET_BUFFER_LIST_NEXT_NBL(PrevFreeNetBufferList) = pMpRfd->NetBufferList;
        }

        NET_BUFFER_LIST_NEXT_NBL(pMpRfd->NetBufferList) = NULL;                
        PrevFreeNetBufferList = pMpRfd->NetBufferList;
        MP_CLEAR_FLAG(pMpRfd->NetBufferList, (NET_BUFFER_LIST_FLAGS(pMpRfd->NetBufferList) & NBL_FLAGS_MINIPORT_RESERVED));
        NetBufferListFreeCount++;
        //
        // Reset the RFD shrink count - don't attempt to shrink RFD
        //
        Adapter->RfdShrinkCount = 0;
        
        //
        // Remember to allocate a new RFD later
        //
        bAllocNewRfd = TRUE;

        Adapter->RcvResourceErrors++;
    }

    NetBufferListCount++;
    
    //
    // if we didn't process any receives, just return from here
    //
    if (NetBufferListCount == 0) 
    {
        goto cleanup;
    }

    NdisDprReleaseSpinLock(&Adapter->RcvLock);


    //
    // Indicate the NetBufferLists to NDIS
    //
    if (NetBufferList != NULL)
    {

        NdisMIndicateReceiveNetBufferLists(
                MP_GET_ADAPTER_HANDLE(Adapter),
                NetBufferList,
                NDIS_DEFAULT_PORT_NUMBER,
                NetBufferListCount-NetBufferListFreeCount,
                ReceiveFlags); 
        
    }
    
    if (FreeNetBufferList != NULL)
    {
        NDIS_SET_RECEIVE_FLAG(ReceiveFlags, NDIS_RECEIVE_FLAGS_RESOURCES);
        
        NdisMIndicateReceiveNetBufferLists(
                MP_GET_ADAPTER_HANDLE(Adapter),
                FreeNetBufferList,          
                NDIS_DEFAULT_PORT_NUMBER,
                NetBufferListFreeCount,
                ReceiveFlags); 
    }
    
    NdisDprAcquireSpinLock(&Adapter->RcvLock);

    //
    // NDIS won't take ownership for the packets with NDIS_STATUS_RESOURCES.
    // For other packets, NDIS always takes the ownership and gives them back 
    // by calling MPReturnPackets
    //
       
    for (; FreeNetBufferList != NULL; FreeNetBufferList = NET_BUFFER_LIST_NEXT_NBL(FreeNetBufferList))
    {

        //
        // Get the MP_RFD saved in this packet, in NICAllocRfd
        //
        pMpRfd = MP_GET_NET_BUFFER_LIST_RFD(FreeNetBufferList);
        
        ASSERT(MP_TEST_FLAG(pMpRfd, fMP_RFD_RESOURCES));
        MP_CLEAR_FLAG(pMpRfd, fMP_RFD_RESOURCES);

        NICReturnRFD(Adapter, pMpRfd);
    }

        
#if EXPAND_RFD_POOL
    //
    // If we ran low on RFD's, we need to allocate a new RFD
    //
    if (bAllocNewRfd)
    {
        DBGPRINT(MP_TRACE, ("Had to alloc more rfd\n"));
        Adapter->RcvExpandPool++;
        //
        // Allocate one more RFD only if no pending new RFD allocation AND
        // it doesn't exceed the max RFD limit
        //
        if (!Adapter->bAllocNewRfd && Adapter->CurrNumRfd < Adapter->MaxNumRfd)
        {
            PMP_RFD TempMpRfd;
            NDIS_STATUS TempStatus;

            TempMpRfd = NdisAllocateFromNPagedLookasideList(&Adapter->RecvLookaside);
			
			if (TempMpRfd) {
				TempMpRfd->RfdBuffer = MP_ALLOCMEMTAG( Adapter->AdapterHandle, Adapter->RfdBufferSize );
				
				if (!TempMpRfd->RfdBuffer) {
					NdisFreeToNPagedLookasideList(&Adapter->RecvLookaside, TempMpRfd);
					TempMpRfd = NULL;
				} else {
                    
                    if( NICAllocRfd( Adapter, TempMpRfd ) == 0 ) {
                   
                        // Add this RFD to the RecvList
                        Adapter->CurrNumRfd++;
                        NICReturnRFD( Adapter, TempMpRfd );

                        ASSERT(Adapter->CurrNumRfd <= Adapter->MaxNumRfd);
                        DBGPRINT(MP_TRACE, ("CurrNumRfd=%d\n", Adapter->CurrNumRfd));

                    } else {
                        MP_FREEMEM(TempMpRfd->RfdBuffer);
                        TempMpRfd->RfdBuffer = NULL;

                        NdisFreeToNPagedLookasideList(&Adapter->RecvLookaside, TempMpRfd);
                        TempMpRfd = NULL;
                    }
                }
			}
        }
    }
#endif

    //
    // get rid of the extra receive ref count we added at the beginning of this
    // function and check to see if we need to complete the pause.
    // Note that we don't have to worry about a blocked Halt here because
    // we are handling an interrupt DPC which means interrupt deregistertion is not
    // completed yet so even if our halt handler is running, NDIS will block the
    // interrupt deregisteration till we return back from this DPC.
    //
    
cleanup:
    MP_DEC_RCV_REF(Adapter);
        
    Count =  MP_GET_RCV_REF(Adapter);
    if ((Count == 0) && (Adapter->AdapterState == NicPausing))
    {
        //
        // If all the NetBufferLists are returned and miniport is pausing,
        // complete the pause
        // 
        
        Adapter->AdapterState = NicPaused;
        NdisDprReleaseSpinLock(&Adapter->RcvLock);
        NdisMPauseComplete(Adapter->AdapterHandle);
        NdisDprAcquireSpinLock(&Adapter->RcvLock);
    }
        
    ASSERT(Adapter->nReadyRecv >= NIC_MIN_RFDS);
}

VOID
MsgBufferRecvBufferPacker(
        PMP_ADAPTER Adapter,
        PUCHAR DestBuffer,
        ULONG PacketLength,
        PVOID Context)
{
    ULONG msg_buffer_buffer = MSG_BUFFER_BUFFER;
 
    NdisMoveMemory( DestBuffer, Adapter->MsgBase + msg_buffer_buffer, PacketLength );

    //
    //Let the other side know we got it
    //
    ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_MSG_RECV );
}

VOID 
MpHandleRecvInterrupt(
    /*IN*/  PMP_ADAPTER  Adapter
    )
/*++
Routine Description:

    Interrupt handler for receive processing
    Put the received packets into an array and call NdisMIndicateReceivePacket
    If we run low on RFDs, allocate another one
    Assumption: Rcv spinlock has been acquired 

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    None
    
--*/
{
    //
    // If NDIS is pausing the miniport or miniport is paused
    // IGNORE the recvs
    // 
    if ((Adapter->AdapterState == NicPausing) ||
        (Adapter->AdapterState == NicPaused))
    {
        return;
    }
    
    //
    // Do not receive any packets until we are at D0
    //
    if (Adapter->CurrentPowerState != NdisDeviceStateD0)
    {
        return;
    }
    

    if( Adapter->MsgBufferRecvPending ) {
        
        USHORT MsgLen = 0;

        Adapter->MsgBufferRecvPending = 0;

        MsgLen = ctn91xx_read16( Adapter->MsgBase, MSG_BUFFER_MSG_LEN );
    
        if( MsgLen ) {
            //cleared in callback
            MpHandleRecvBuffer( Adapter, MsgBufferRecvBufferPacker, MsgLen, NULL );
        } else {
            DBGPRINT(MP_WARN, ("Recv'd 0 length pkt\n"));
            //clear the 0 length
            ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_MSG_RECV );
        }
    }
}



VOID 
NICReturnRFD(
    /*IN*/  PMP_ADAPTER  Adapter,
    /*IN*/  PMP_RFD		pMpRfd
    )
/*++
Routine Description:

    Recycle a RFD and put it back onto the receive list 
    Assumption: Rcv spinlock has been acquired 

Arguments:

    Adapter     Pointer to our adapter
    pMpRfd      Pointer to the RFD 

Return Value:

    None
    
    NOTE: During return, we should check if we need to allocate new net buffer list
          for the RFD.
--*/
{
    MP_SET_FLAG(pMpRfd, fMP_RFD_RECV_READY);
    
    //
    // The processing on this RFD is done, so put it back on the tail of
    // our list
    //
    InsertTailList(&Adapter->RecvList, (PLIST_ENTRY)pMpRfd);
    Adapter->nReadyRecv++;
    ASSERT(Adapter->nReadyRecv <= Adapter->CurrNumRfd);
}

VOID 
MpFreeQueuedSendNetBufferLists(
    /*IN*/  PMP_ADAPTER  Adapter
    )
/*++
Routine Description:

    Free and complete the pended sends on SendWaitQueue
    Assumption: spinlock has been acquired 
    
Arguments:

    Adapter     Pointer to our adapter

Return Value:

     None
NOTE: Run at DPC     

--*/
{
    PQUEUE_ENTRY        pEntry;
    PNET_BUFFER_LIST    NetBufferList;
    PNET_BUFFER_LIST    NetBufferListToComplete = NULL;
    PNET_BUFFER_LIST    LastNetBufferList = NULL;
    NDIS_STATUS         Status = MP_GET_STATUS_FROM_FLAGS(Adapter);
    PNET_BUFFER         NetBuffer;

    DBGPRINT(MP_TRACE, ("--> MpFreeQueuedSendNetBufferLists\n"));

    while (!IsQueueEmpty(&Adapter->SendWaitQueue))
    {
        pEntry = RemoveHeadQueue(&Adapter->SendWaitQueue); 
        ASSERT(pEntry);
        
        Adapter->nWaitSend--;

        NetBufferList = MP_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(pEntry);

        NET_BUFFER_LIST_STATUS(NetBufferList) = Status;
        //
        // The sendLock is held
        // 
        NetBuffer = MP_GET_NET_BUFFER_LIST_NEXT_SEND(NetBufferList);
        
        for (; NetBuffer != NULL; NetBuffer = NET_BUFFER_NEXT_NB(NetBuffer))
        {
            MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList)--;
        }
        //
        // If Ref count goes to 0, then complete it.
        // Otherwise, Send interrupt DPC would complete it later
        // 
        if (MP_GET_NET_BUFFER_LIST_REF_COUNT(NetBufferList) == 0)
        {
            if (NetBufferListToComplete == NULL)
            {
                NetBufferListToComplete = NetBufferList;
            }
            else
            {
                NET_BUFFER_LIST_NEXT_NBL(LastNetBufferList) = NetBufferList;
            }
            NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;
            LastNetBufferList = NetBufferList;

        }
    }

    if (NetBufferListToComplete != NULL)
    {
        MP_RELEASE_SPIN_LOCK(&Adapter->SendLock, TRUE);
        MPHandleSendNetBufferListsComplete( Adapter, NetBufferListToComplete, Status, TRUE );
        MP_ACQUIRE_SPIN_LOCK(&Adapter->SendLock, TRUE);
    }
    
    DBGPRINT(MP_TRACE, ("<-- MpFreeQueuedSendNetBufferLists\n"));

}

void 
MpFreeBusySendNetBufferLists(
    /*IN*/  PMP_ADAPTER  Adapter
    )
/*++
Routine Description:

    Free and complete the stopped active sends
    Assumption: Send spinlock has been acquired 
    
Arguments:

    Adapter     Pointer to our adapter

Return Value:

     None

--*/
{
    PNET_BUFFER_LIST   NetBufferList;
    PNET_BUFFER_LIST   CompleteNetBufferLists = NULL;
    PNET_BUFFER_LIST   LastNetBufferList = NULL;

    DBGPRINT(MP_TRACE, ("--> MpFreeBusySendNetBufferLists\n"));

    //
    // Any NET_BUFFER being sent? Check the first TCB on the send list
    //
    while (Adapter->nBusySend > 0)
    {
        //
        // To change this to complete a list of NET_BUFFER_LISTs
        //
        NetBufferList = MP_FREE_SEND_NET_BUFFER(Adapter);
        //
        // If one NET_BUFFER_LIST got complete
        //
        if (NetBufferList != NULL)
        {
            NET_BUFFER_LIST_STATUS(NetBufferList) = NDIS_STATUS_REQUEST_ABORTED;
            if (CompleteNetBufferLists == NULL)
            {
                CompleteNetBufferLists = NetBufferList;
            }
            else
            {
                NET_BUFFER_LIST_NEXT_NBL(LastNetBufferList) = NetBufferList;
            }
            NET_BUFFER_LIST_NEXT_NBL(NetBufferList) = NULL;
            LastNetBufferList = NetBufferList;
        }
    }

    //
    // Complete the NET_BUFFER_LISTs
    //
    if (CompleteNetBufferLists != NULL)
    {
        NdisReleaseSpinLock(&Adapter->SendLock);
        MPHandleSendNetBufferListsComplete( Adapter, CompleteNetBufferLists, NDIS_STATUS_REQUEST_ABORTED, FALSE );
        NdisAcquireSpinLock(&Adapter->SendLock);
    }

    DBGPRINT(MP_TRACE, ("<-- MpFreeBusySendNetBufferLists\n"));
}

VOID 
NICResetRecv(
    /*IN*/  PMP_ADAPTER   Adapter
    )
/*++
Routine Description:

    Reset the receive list                    
    Assumption: Rcv spinlock has been acquired 
    
Arguments:

    Adapter     Pointer to our adapter

Return Value:

     None

--*/
{
    PMP_RFD   pMpRfd;      
    LONG      RfdCount;

    DBGPRINT(MP_TRACE, ("--> NICResetRecv\n"));

    ASSERT(!IsListEmpty(&Adapter->RecvList));
    
    //
    // Get the MP_RFD head
    //
    pMpRfd = (PMP_RFD)GetListHeadEntry(&Adapter->RecvList);
    for (RfdCount = 0; RfdCount < Adapter->nReadyRecv; RfdCount++)
    {
        pMpRfd = (PMP_RFD)GetListFLink(&pMpRfd->List);
    }

    DBGPRINT(MP_TRACE, ("<-- NICResetRecv\n"));
}

__drv_raisesIRQL(DISPATCH_LEVEL) 
__drv_requiresIRQL(DISPATCH_LEVEL) 
VOID
MPIndicateLinkState(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  BOOLEAN         DispatchLevel
    )    
/*++
Routine Description:
    This routine sends a NDIS_STATUS_LINK_STATE status up to NDIS
    
Arguments:
    
    Adapter         Pointer to our adapter
             
Return Value:

     None

    NOTE: called at DISPATCH level
--*/

{
    
    NDIS_LINK_STATE                LinkState;
    NDIS_STATUS_INDICATION         StatusIndication;

    NdisZeroMemory(&LinkState, sizeof(NDIS_LINK_STATE));
    
    LinkState.Header.Revision = NDIS_LINK_STATE_REVISION_1;
    LinkState.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    LinkState.Header.Size = sizeof(NDIS_LINK_STATE);
    
    DBGPRINT(MP_WARN, ("Media state changed to %s\n",
              ((Adapter->MediaState == MediaConnectStateConnected)? 
              "Connected": "Disconnected")));


    if (Adapter->MediaState == MediaConnectStateConnected)
    {
        MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_NO_CABLE);
        
        if (Adapter->usDuplexMode == 1)
        {
            Adapter->MediaDuplexState = MediaDuplexStateHalf;
        }
        else if (Adapter->usDuplexMode == 2)
        {
            Adapter->MediaDuplexState = MediaDuplexStateFull;
        }
        else
        {
            Adapter->MediaDuplexState = MediaDuplexStateUnknown;
        }
        //
        // NDIS 6.0 miniports report xmit and recv link speeds in bps
        //
        Adapter->LinkSpeed = Adapter->usLinkSpeed * 1000000;
    }
    else
    {
        MP_SET_FLAG(Adapter, fMP_ADAPTER_NO_CABLE);
        Adapter->MediaState = MediaConnectStateDisconnected;
        Adapter->MediaDuplexState = MediaDuplexStateUnknown;
        Adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
    }

    LinkState.MediaConnectState = Adapter->MediaState;
    LinkState.MediaDuplexState = Adapter->MediaDuplexState;
    LinkState.XmitLinkSpeed = LinkState.RcvLinkSpeed = Adapter->LinkSpeed;
   
    MP_RELEASE_SPIN_LOCK( &Adapter->Lock, DispatchLevel );
    MP_INIT_NDIS_STATUS_INDICATION(&StatusIndication,
                                   Adapter->AdapterHandle,
                                   NDIS_STATUS_LINK_STATE,
                                   (PVOID)&LinkState,
                                   sizeof(LinkState));
                                   
    NdisMIndicateStatusEx(Adapter->AdapterHandle, &StatusIndication);
    MP_ACQUIRE_SPIN_LOCK( &Adapter->Lock, DispatchLevel );
    
    return;
}

VOID
MPHandleSendNetBufferListsComplete(
    /*IN*/ PMP_ADAPTER Adapter,
    /*IN*/ PNET_BUFFER_LIST NetBufferList,
    /*IN*/ NDIS_STATUS Status,
    /*IN*/ BOOLEAN DispatchLevel)
{
    ULONG Type = SEND_BUFFER_ORIGIN_ETHERNET;//default

    if( !NetBufferList ) {
        return;
    }
    
    if( !MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) ) {
        DBGPRINT( MP_WARN, ("net buffer list private data was NULL (at complete)\n"));
    } else {
        Type = MP_GET_NET_BUFFER_LIST_TYPE( NetBufferList );
    }

    if( Type == SEND_BUFFER_ORIGIN_ETHERNET ) {
        PNET_BUFFER_LIST    CurrNetBufferList;
        PNET_BUFFER_LIST    NextNetBufferList = NULL;
        ULONG SendCompleteFlags = 0;
    
        for (CurrNetBufferList = NetBufferList;
                 CurrNetBufferList != NULL;
                 CurrNetBufferList = NextNetBufferList) {

            NextNetBufferList = NET_BUFFER_LIST_NEXT_NBL(CurrNetBufferList);
            NET_BUFFER_LIST_STATUS(CurrNetBufferList) = Status;
        }

        if( DispatchLevel ) {

            NDIS_SET_SEND_COMPLETE_FLAG(SendCompleteFlags, NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
        }

        MP_FREEMEM( MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) );
                  
        NdisMSendNetBufferListsComplete(
            MP_GET_ADAPTER_HANDLE(Adapter),
            NetBufferList,
            SendCompleteFlags);   
    } else if( Type == SEND_BUFFER_ORIGIN_RPC ) {

        PSEND_BUFFER_CONTEXT SendBufferContext = (PSEND_BUFFER_CONTEXT)NET_BUFFER_LIST_CONTEXT_DATA_START( NetBufferList );

        if( SendBufferContext->Mdl ) {
            NdisFreeMdl( SendBufferContext->Mdl );
        }

        MP_FREEMEM( SendBufferContext->Buffer );
        MP_FREEMEM( MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) );

        if( NetBufferList) {
            NdisFreeNetBufferList( NetBufferList );
        }

    } else {
        DBGPRINT( MP_WARN, ("unknown private data\n"));
    }
}

