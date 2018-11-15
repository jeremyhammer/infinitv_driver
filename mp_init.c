/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_init.c

Abstract:
    This module contains miniport initialization related routines

Revision History:

Notes:

--*/

#include "precomp.h"

#if DBG
#define _FILENUMBER     'TINI'
#endif

typedef struct _MP_REG_ENTRY
{
    NDIS_STRING RegName;                // variable name text
    BOOLEAN     bRequired;              // 1 -> required, 0 -> optional
    UINT        FieldOffset;            // offset to MP_ADAPTER field
    UINT        FieldSize;              // size (in bytes) of the field
    UINT        Default;                // default value to use
    UINT        Min;                    // minimum value allowed
    UINT        Max;                    // maximum value allowed
} MP_REG_ENTRY, *PMP_REG_ENTRY;

MP_REG_ENTRY NICRegTable[] = {
// reg value name                               Offset in MP_ADAPTER                Field size                      Default Value   Min             Max
    {NDIS_STRING_CONST("*SupportBridging"),     0,  MP_OFFSET(SupportBridging),     MP_SIZE(SupportBridging),       0,              0,              1},
    {NDIS_STRING_CONST("*ReportedSpeed"),       0,  MP_OFFSET(ReportedSpeed),       MP_SIZE(ReportedSpeed),         1000,           10,             1000},
    {NDIS_STRING_CONST("*RevertBridging"),      0,  MP_OFFSET(RevertBridging),      MP_SIZE(RevertBridging),        0,              0,              1},
    {NDIS_STRING_CONST("*LastSupportBridging"), 0,  MP_OFFSET(LastSupportBridging), MP_SIZE(LastSupportBridging),   0,              0,              1},
};

#define NIC_NUM_REG_PARAMS (sizeof (NICRegTable) / sizeof(MP_REG_ENTRY))


NDIS_STATUS 
MpFindAdapter(
    /*IN*/  PMP_ADAPTER             Adapter,
    /*IN*/  PNDIS_RESOURCE_LIST     resList
    )
/*++
Routine Description:

    Find the adapter and get all the assigned resources

Arguments:

    Adapter     Pointer to our adapter
    resList     Pointer to our resources

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_ADAPTER_NOT_FOUND (event is logged as well)    

--*/    
{

    
    NDIS_STATUS         Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
    ULONG               ErrorCode = 0;
    ULONG               ErrorValue = 0;

    ULONG               ulResult;
    typedef __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) CommonConfigCharBuf;        
    CommonConfigCharBuf buffer[NIC_PCI_HDR_LENGTH ];
    PPCI_COMMON_CONFIG  pPciConfig = (PPCI_COMMON_CONFIG) buffer;
    USHORT              usPciCommand;
       
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pResDesc;
    ULONG               index;
    BOOLEAN             bResInterrupt = FALSE, bResMemory = FALSE;

    DBGPRINT(MP_TRACE, ("---> MpFindAdapter\n"));

    do
    {
        //
        // Make sure the adpater is present
        //

        ulResult = NdisMGetBusData(
                       Adapter->AdapterHandle,
                       PCI_WHICHSPACE_CONFIG,
                       FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID),
                       buffer,
                       NIC_PCI_HDR_LENGTH );
                       
                       


        if (ulResult != NIC_PCI_HDR_LENGTH )
        {
            DBGPRINT(MP_ERROR, 
                ("NdisMGetBusData (PCI_COMMON_CONFIG) ulResult=%d\n", ulResult));

            ErrorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
            ErrorValue = ERRLOG_READ_PCI_SLOT_FAILED;
                   
            break;
        }

        //     
        // Right type of adapter?
        //
        if ((pPciConfig->VendorID != NIC_PCI_VENDOR_ID &&
            pPciConfig->VendorID != ALTERA_VENDOR_ID) ||
            pPciConfig->DeviceID != CTN91XX_DEVICE_ID)
        {
            DBGPRINT(MP_ERROR, ("VendorID/DeviceID don't match - %x/%x\n", 
                pPciConfig->VendorID, pPciConfig->DeviceID));

            ErrorCode = NDIS_ERROR_CODE_ADAPTER_NOT_FOUND;
            ErrorValue = ERRLOG_VENDOR_DEVICE_NOMATCH;

            break;
        }

        DBGPRINT(MP_INFO, ("Adapter is found - VendorID/DeviceID=%x/%x\n", 
            pPciConfig->VendorID, pPciConfig->DeviceID));

        // save info from config space
        Adapter->RevsionID = pPciConfig->RevisionID;
        Adapter->SubVendorID = pPciConfig->u.type0.SubVendorID;
        Adapter->SubSystemID = pPciConfig->u.type0.SubSystemID;

        if (resList == NULL)
        {
            DBGPRINT(MP_ERROR, ("Resource conflict\n"));
            ErrorCode = NDIS_ERROR_CODE_RESOURCE_CONFLICT;
            ErrorValue = ERRLOG_QUERY_ADAPTER_RESOURCES;
            break;
        }

        for (index=0; index < resList->Count; index++)
        {
            pResDesc = &resList->PartialDescriptors[index];

            switch(pResDesc->Type)
            {
                case CmResourceTypeInterrupt:
                    Adapter->InterruptLevel = pResDesc->u.Interrupt.Level;
                    Adapter->InterruptVector = pResDesc->u.Interrupt.Vector;
                    bResInterrupt = TRUE;
                    
                    DBGPRINT(MP_INFO, ("InterruptLevel = 0x%x\n", Adapter->InterruptLevel));
                    break;

                case CmResourceTypeMemory:
                    // NOAH REQUIRES INVERTING THIS CHECK
                    if (pResDesc->u.Memory.Length == CTN91XX_REG_REGION_SIZE)
                    {
                        Adapter->MemPhysAddress = pResDesc->u.Memory.Start;
                        Adapter->MemPhysSize = pResDesc->u.Memory.Length;
                        bResMemory = TRUE;
                        
                        DBGPRINT(MP_INFO, 
                            ("MemPhysAddress(Low) = 0x%0x\n", NdisGetPhysicalAddressLow(Adapter->MemPhysAddress)));
                        DBGPRINT(MP_INFO, 
                            ("MemPhysAddress(High) = 0x%0x\n", NdisGetPhysicalAddressHigh(Adapter->MemPhysAddress)));
                    } else if (pResDesc->u.Memory.Length == CTN91XX_TRANSLATION_REG_REGION_SIZE) {
                        Adapter->TranslationMemPhysAddress = pResDesc->u.Memory.Start;
                        Adapter->TranslationMemPhysSize = pResDesc->u.Memory.Length;
                        DBGPRINT(MP_INFO, 
                            ("TranslationMemPhysAddress(Low) = 0x%0x\n", NdisGetPhysicalAddressLow(Adapter->TranslationMemPhysAddress)));
                        DBGPRINT(MP_INFO, 
                            ("TranslationMemPhysAddress(High) = 0x%0x\n", NdisGetPhysicalAddressHigh(Adapter->TranslationMemPhysAddress)));
                    }
                    break;
                default:
                    break;
            }
        } 
        
        if (!bResInterrupt || !bResMemory)
        {
            Status = NDIS_STATUS_RESOURCE_CONFLICT;
            ErrorCode = NDIS_ERROR_CODE_RESOURCE_CONFLICT;
            
            if (!bResInterrupt)
            {
                DBGPRINT(MP_ERROR, ("ERRLOG_NO_INTERRUPT_RESOURCE\n"));
                ErrorValue = ERRLOG_NO_INTERRUPT_RESOURCE;
            }
            else 
            {
                DBGPRINT(MP_ERROR, ("ERRLOG_NO_MEMORY_RESOURCE\n"));
                ErrorValue = ERRLOG_NO_MEMORY_RESOURCE;
            }
            
            break;
        }
        
        Status = NDIS_STATUS_SUCCESS;

    } while (FALSE);
    
    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            Adapter->AdapterHandle,
            ErrorCode,
            1,
            ErrorValue);
    }

    DBGPRINT_S(Status, ("<--- MpFindAdapter, Status=%x\n", Status));

    return Status;

}

NDIS_STATUS NICReadAdapterInfo(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Read the mac addresss from the adapter

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_INVALID_ADDRESS

--*/    
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    USHORT          usValue; 
    int             i;

    DBGPRINT(MP_TRACE, ("--> NICReadAdapterInfo\n"));

    Adapter->PermanentAddress[0] = 0x00;
    Adapter->PermanentAddress[1] = 0x22;
    Adapter->PermanentAddress[2] = 0x2c;
    Adapter->PermanentAddress[3] = 0xff;
    Adapter->PermanentAddress[4] = 0xff;
    Adapter->PermanentAddress[5] = (UCHAR)(0xff - Adapter->BoardNumber);

    DBGPRINT(MP_INFO, ("Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n", 
        Adapter->PermanentAddress[0], Adapter->PermanentAddress[1], 
        Adapter->PermanentAddress[2], Adapter->PermanentAddress[3], 
        Adapter->PermanentAddress[4], Adapter->PermanentAddress[5]));

    if (ETH_IS_MULTICAST(Adapter->PermanentAddress) || 
        ETH_IS_BROADCAST(Adapter->PermanentAddress))
    {
        DBGPRINT(MP_ERROR, ("Permanent address is invalid\n")); 

        NdisWriteErrorLogEntry(
            Adapter->AdapterHandle,
            NDIS_ERROR_CODE_NETWORK_ADDRESS,
            0);
        Status = NDIS_STATUS_INVALID_ADDRESS;         
    }
    else
    {
        if (!Adapter->bOverrideAddress)
        {
            ETH_COPY_NETWORK_ADDRESS(Adapter->CurrentAddress, Adapter->PermanentAddress);
        }

        DBGPRINT(MP_INFO, ("Current Address = %02x-%02x-%02x-%02x-%02x-%02x\n", 
            Adapter->CurrentAddress[0], Adapter->CurrentAddress[1],
            Adapter->CurrentAddress[2], Adapter->CurrentAddress[3],
            Adapter->CurrentAddress[4], Adapter->CurrentAddress[5]));
    }

    DBGPRINT_S(Status, ("<-- NICReadAdapterInfo, Status=%x\n", Status));

    return Status;
}

NDIS_STATUS MpAllocAdapterBlock(
    /*OUT*/ PMP_ADAPTER     *pAdapter,
    /*IN*/  NDIS_HANDLE     MiniportAdapterHandle
    )
/*++
Routine Description:

    Allocate MP_ADAPTER data block and do some initialization

Arguments:

    Adapter     Pointer to receive pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE

--*/    
{
    PMP_ADAPTER     Adapter;
    NDIS_STATUS     Status;
    int i;

    DBGPRINT(MP_TRACE, ("--> NICAllocAdapter\n"));

    *pAdapter = NULL;

    do
    {
        // Allocate MP_ADAPTER block
        Adapter = MP_ALLOCMEMTAG(MiniportAdapterHandle, sizeof(MP_ADAPTER));
        if (Adapter == NULL)
        {
            Status = NDIS_STATUS_RESOURCES;
            DBGPRINT(MP_ERROR, ("Failed to allocate memory - ADAPTER\n"));
            break;
        }
        
        Status = NDIS_STATUS_SUCCESS;

        // Clean up the memory block
        NdisZeroMemory(Adapter, sizeof(MP_ADAPTER));

        MP_INC_REF(Adapter);
        
        Adapter->EEPROM_WorkItem = NULL;

        //
        // Init lists, spinlocks, etc.
        // 
        InitializeQueueHeader(&Adapter->SendWaitQueue);
        InitializeQueueHeader(&Adapter->SendCancelQueue);
        
        InitializeListHead(&Adapter->RecvList);
        InitializeListHead(&Adapter->RecvPendList);

        NdisInitializeEvent(&Adapter->ExitEvent);
        NdisInitializeEvent(&Adapter->AllPacketsReturnedEvent);

        NdisAllocateSpinLock(&Adapter->Lock);
        NdisAllocateSpinLock(&Adapter->MpegLock);
        NdisAllocateSpinLock(&Adapter->SendLock);
        NdisAllocateSpinLock(&Adapter->RcvLock);
        NdisAllocateSpinLock(&Adapter->ControlLock);
        NdisAllocateSpinLock(&Adapter->RtpLock);
        
        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

            NdisAllocateSpinLock( &Adapter->MpegBuffers[i].Lock );
            NdisAllocateSpinLock( &Adapter->MpegBuffers[i].RtpState.RunningLock );
        }        

    } while (FALSE);

    *pAdapter = Adapter;

    DBGPRINT_S(Status, ("<-- NICAllocAdapter, Status=%x\n", Status));

    return Status;
}

VOID MpFreeAdapter(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Free all the resources and MP_ADAPTER data block

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    None                                                    

--*/    
{
    PMP_RFD         pMpRfd;
    int i;

    DBGPRINT(MP_TRACE, ("--> NICFreeAdapter\n"));

    // No active and waiting sends
    ASSERT(Adapter->nBusySend == 0);
    ASSERT(Adapter->nWaitSend == 0);
    ASSERT(IsQueueEmpty(&Adapter->SendWaitQueue));
    ASSERT(IsQueueEmpty(&Adapter->SendCancelQueue));

    // No other pending operations
    ASSERT(IsListEmpty(&Adapter->RecvPendList));
    ASSERT(Adapter->bAllocNewRfd == FALSE);
    ASSERT(MP_GET_REF(Adapter) == 0);

    //
    // Free hardware resources
    //      
    if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_INTERRUPT_IN_USE))
    {
        NdisMDeregisterInterruptEx(Adapter->NdisInterruptHandle);
        MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_INTERRUPT_IN_USE);
    }

    if (Adapter->RegisterBase)
    {
        NdisMUnmapIoSpace(
            Adapter->AdapterHandle,
            Adapter->RegisterBase,
            Adapter->MemPhysSize);
        Adapter->RegisterBase = NULL;
    }

    if (Adapter->TranslationMap)
    {
        NdisMUnmapIoSpace(
                Adapter->AdapterHandle,
                Adapter->TranslationMap,
                Adapter->TranslationMemPhysSize);
        Adapter->TranslationMap = NULL;
    }

    //               
    // Free RECV memory/NDIS buffer/NDIS packets/shared memory
    //
    ASSERT(Adapter->nReadyRecv == Adapter->CurrNumRfd);

    while (!IsListEmpty(&Adapter->RecvList))
    {
        pMpRfd = (PMP_RFD)RemoveHeadList(&Adapter->RecvList);
        NICFreeRfd(Adapter, pMpRfd, FALSE);
    }
    //
    // Free the chunk of memory
    // 
    if( Adapter->RfdBuffer )
    {
        MP_FREEMEM( Adapter->RfdBuffer );
    }

    //
    // Free receive packet pool
    // 
    if (Adapter->RecvNetBufferListPool)
    {
        NdisFreeNetBufferListPool(Adapter->RecvNetBufferListPool);
        Adapter->RecvNetBufferListPool = NULL;
    }

    if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_RECV_LOOKASIDE))
    {
        NdisDeleteNPagedLookasideList(&Adapter->RecvLookaside);
        MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_RECV_LOOKASIDE);
    }
 
    //
    // Free send packet pool
    // 
    if (Adapter->RpcSendNetBufferListPool)
    {
        NdisFreeNetBufferListPool(Adapter->RpcSendNetBufferListPool);
        Adapter->RpcSendNetBufferListPool = NULL;
    }

    NdisFreeSpinLock(&Adapter->Lock);
    NdisFreeSpinLock(&Adapter->SendLock);
    NdisFreeSpinLock(&Adapter->RcvLock);
    NdisFreeSpinLock(&Adapter->MpegLock);
    NdisFreeSpinLock(&Adapter->ControlLock);
    NdisFreeSpinLock(&Adapter->RtpLock);

    for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

        PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];
        ULONG j;
        for( j=0; j<MpegBuffer->NumPages; j++ ) {
            PHYSICAL_ADDRESS PhysAddr;
            PhysAddr.u.HighPart = 0;
            PhysAddr.u.LowPart = MpegBuffer->PhysicalAddresses[j];
            Adapter->DmaAdapterObject->DmaOperations->FreeCommonBuffer(
                    Adapter->DmaAdapterObject,
                    PAGE_SIZE,
                    PhysAddr,
                    MpegBuffer->Buffers[j],
                    FALSE );
        }

        MP_FREEMEM( MpegBuffer->Buffers );
        MP_FREEMEM( MpegBuffer->LockCnt );
        MP_FREEMEM( MpegBuffer->Dropped );
        MP_FREEMEM( MpegBuffer->Remaining );
        MP_FREEMEM( MpegBuffer->Sizes );
        MP_FREEMEM( MpegBuffer->PhysicalAddresses );

        MP_FREEMEM( MpegBuffer->RtpState.Buffer );

        NdisFreeSpinLock( &MpegBuffer->Lock );
        NdisFreeSpinLock( &MpegBuffer->RtpState.RunningLock );
    }

    if (Adapter->DmaAdapterObject) {
        Adapter->DmaAdapterObject->DmaOperations->PutDmaAdapter( Adapter->DmaAdapterObject );
    }

    RtpFreeMem( Adapter );

    if( Adapter->RpcBuffer ) {
        MP_FREEMEM( Adapter->RpcBuffer );
    }

    MP_FREEMEM(Adapter);  

#if DBG
    if (MPInitDone)
    {
        NdisFreeSpinLock(&MPMemoryLock);
    }
#endif

    DBGPRINT(MP_TRACE, ("<-- NICFreeAdapter\n"));
}

NDIS_STATUS
NICReadRegParameters(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Read the following from the registry
    1. All the parameters
    2. NetworkAddres

Arguments:

    Adapter                         Pointer to our adapter
    MiniportAdapterHandle           For use by NdisOpenConfiguration
    
Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_RESOURCES                                       

--*/    
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    NDIS_HANDLE     ConfigurationHandle;
    PMP_REG_ENTRY   pRegEntry;
    NDIS_STRING     regkey_value_true = NDIS_STRING_CONST("1");
    NDIS_STRING     regkey_value_false = NDIS_STRING_CONST("0");
    UINT            i;
    UINT            value;
    PUCHAR          pointer;
    PNDIS_CONFIGURATION_PARAMETER ReturnedValue;
    PUCHAR          NetworkAddress;
    UINT            Length;
    NDIS_CONFIGURATION_OBJECT ConfigObject;

    DBGPRINT(MP_TRACE, ("--> NICReadRegParameters\n"));

    ConfigObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    ConfigObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    ConfigObject.Header.Size = sizeof(NDIS_CONFIGURATION_OBJECT);
    ConfigObject.NdisHandle = Adapter->AdapterHandle;
    ConfigObject.Flags = 0;

    Status = NdisOpenConfigurationEx(&ConfigObject, &ConfigurationHandle);
    
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DBGPRINT(MP_ERROR, ("NdisOpenConfiguration failed\n"));
        DBGPRINT_S(Status, ("<-- NICReadRegParameters, Status=%x\n", Status));
        return Status;
    }

    // read all the registry values 
    for (i = 0, pRegEntry = NICRegTable; i < NIC_NUM_REG_PARAMS; i++, pRegEntry++)
    {
        //
        // Driver should NOT fail the initialization only because it can not
        // read the registry
        //
#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW, "pRegEntry is bounded by the check above");        
        pointer = (PUCHAR) Adapter + pRegEntry->FieldOffset;

        DBGPRINT_UNICODE(MP_INFO, &pRegEntry->RegName);

        // Get the configuration value for a specific parameter.  Under NT the
        // parameters are all read in as DWORDs.
        NdisReadConfiguration(
            &Status,
            &ReturnedValue,
            ConfigurationHandle,
            &pRegEntry->RegName,
            NdisParameterInteger);

        // If the parameter was present, then check its value for validity.
        if (NT_SUCCESS(Status))
        {
            // Check that param value is not too small or too large
            if (ReturnedValue->ParameterData.IntegerData < pRegEntry->Min ||
                ReturnedValue->ParameterData.IntegerData > pRegEntry->Max)
            {
                value = pRegEntry->Default;
            }
            else
            {
                value = ReturnedValue->ParameterData.IntegerData;
            }

            DBGPRINT_RAW(MP_INFO, ("= 0x%x\n", value));
        }
        else if (pRegEntry->bRequired)
        {
            DBGPRINT_RAW(MP_ERROR, (" -- failed\n"));

            ASSERT(FALSE);

            Status = NDIS_STATUS_FAILURE;
            break;
        }
        else
        {
            value = pRegEntry->Default;
            DBGPRINT_RAW(MP_INFO, ("= 0x%x (default)\n", value));
            Status = NDIS_STATUS_SUCCESS;
        }

        //
        // Store the value in the adapter structure.
        //
        switch(pRegEntry->FieldSize)
        {
            case 1:
                *((PUCHAR) pointer) = (UCHAR) value;
                break;

            case 2:
                *((PUSHORT) pointer) = (USHORT) value;
                break;

            case 4:
                *((PULONG) pointer) = (ULONG) value;
                break;

            default:
                DBGPRINT(MP_ERROR, ("Bogus field size %d\n", pRegEntry->FieldSize));
                break;
        }
    }

    if (!Adapter->SupportBridging) {
        if (STAMP_LAST_BRIDGING_SUPPORT) {
            NDIS_STATUS ndis_status = NDIS_STATUS_SUCCESS;
            NDIS_CONFIGURATION_PARAMETER parameter;
            NDIS_STRING regkey_name = NDIS_STRING_CONST("*LastSupportBridging");

            NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
            parameter.ParameterType = NdisParameterString;
            parameter.ParameterData.StringData = Adapter->SupportBridging ? regkey_value_true : regkey_value_false;

            NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);
        }
        if (STAMP_NUM_TS_PKTS_PER_RTP_PKT) {
            NDIS_STATUS ndis_status = NDIS_STATUS_SUCCESS;
            NDIS_CONFIGURATION_PARAMETER parameter;
            NDIS_STRING regkey_name = NDIS_STRING_CONST("*NumberOfTSPacketsPerRTPPacket");

            NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
            parameter.ParameterType = NdisParameterInteger;
            parameter.ParameterData.IntegerData = NUM_TS_PKTS_PER_RTP_PKT(Adapter);

            NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);
        }
    } else if (Adapter->RevertBridging) {
        NDIS_STATUS ndis_status = NDIS_STATUS_SUCCESS;
        NDIS_CONFIGURATION_PARAMETER parameter;
        NDIS_STRING regkey_name = NDIS_STRING_CONST("*SupportBridging");

        NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
        parameter.ParameterType = NdisParameterString;
        parameter.ParameterData.StringData = regkey_value_false;

        NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);

        // Set SupportBridging to FALSE so we don't continue to hang system
        Adapter->SupportBridging = FALSE;
    } else if (Adapter->SupportBridging) {
        NDIS_STATUS ndis_status = NDIS_STATUS_SUCCESS;
        NDIS_CONFIGURATION_PARAMETER parameter;
        NDIS_STRING regkey_name = NDIS_STRING_CONST("*RevertBridging");

        NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
        parameter.ParameterType = NdisParameterString;
        parameter.ParameterData.StringData = regkey_value_true;

        NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);
    }

    Adapter->NumRfd = NIC_DEF_RFDS;
    DBGPRINT(MP_WARN, ("SupportBridging: %d\n", Adapter->SupportBridging));

    // Read NetworkAddress registry value 
    // Use it as the current address if any
    if (NT_SUCCESS(Status))
    {
        NdisReadNetworkAddress(
            &Status,
            &NetworkAddress,
            &Length,
            ConfigurationHandle);
        // If there is a NetworkAddress override in registry, use it 
        if ((Status == NDIS_STATUS_SUCCESS) && (Length == ETH_LENGTH_OF_ADDRESS))
        {
            if ((ETH_IS_MULTICAST(NetworkAddress) 
                    || ETH_IS_BROADCAST(NetworkAddress))
                    || !ETH_IS_LOCALLY_ADMINISTERED (NetworkAddress))
            {
                DBGPRINT(MP_ERROR, 
                    ("Overriding NetworkAddress is invalid - %02x-%02x-%02x-%02x-%02x-%02x\n", 
                    NetworkAddress[0], NetworkAddress[1], NetworkAddress[2],
                    NetworkAddress[3], NetworkAddress[4], NetworkAddress[5]));
            }
            else
            {
                ETH_COPY_NETWORK_ADDRESS(Adapter->CurrentAddress, NetworkAddress);
                Adapter->bOverrideAddress = TRUE;
            }
        }

        Status = NDIS_STATUS_SUCCESS;
    }

    // Close the registry
    NdisCloseConfiguration(ConfigurationHandle);
    
    DBGPRINT_S(Status, ("<-- NICReadRegParameters, Status=%x\n", Status));

    return Status;
}

NDIS_STATUS
NICWriteRegParameters(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Write the following to the registry
    1. SupportBridging

Arguments:

    Adapter                         Pointer to our adapter
    MiniportAdapterHandle           For use by NdisOpenConfiguration
    
Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_RESOURCES                                       

--*/    
{
    NDIS_STATUS                 ndis_status = NDIS_STATUS_FAILURE;

    NDIS_HANDLE                 ConfigurationHandle;
    NDIS_CONFIGURATION_OBJECT   ConfigObject;

    NDIS_STRING                 regkey_value_true = NDIS_STRING_CONST("1");
    NDIS_STRING                 regkey_value_false = NDIS_STRING_CONST("0");

    DBGPRINT(MP_TRACE, ("--> NICWriteRegParameters\n"));

    ConfigObject.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
    ConfigObject.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
    ConfigObject.Header.Size = sizeof(NDIS_CONFIGURATION_OBJECT);
    ConfigObject.NdisHandle = Adapter->AdapterHandle;
    ConfigObject.Flags = 0;

    ndis_status = NdisOpenConfigurationEx(&ConfigObject, &ConfigurationHandle);
    if (NT_ERROR(ndis_status)) 
	{
        DBGPRINT(MP_ERROR, ("NdisOpenConfiguration failed\n"));
        DBGPRINT_S(ndis_status, ("<-- NICWriteRegParameters, Status=%x\n", ndis_status));
        return ndis_status;
    }

    if (Adapter->RevertBridging) 
	{
        NDIS_CONFIGURATION_PARAMETER parameter;
        NDIS_STRING regkey_name = NDIS_STRING_CONST("*RevertBridging");

        NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
        parameter.ParameterType = NdisParameterString;
        parameter.ParameterData.StringData = regkey_value_false;

        NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);
    }

    if (STAMP_LAST_BRIDGING_SUPPORT) 
	{
        NDIS_CONFIGURATION_PARAMETER parameter;
        NDIS_STRING regkey_name = NDIS_STRING_CONST("*LastSupportBridging");

        NdisZeroMemory(&parameter, sizeof(NDIS_CONFIGURATION_PARAMETER));
        parameter.ParameterType = NdisParameterString;
        parameter.ParameterData.StringData = Adapter->SupportBridging ? regkey_value_true : regkey_value_false;

        NdisWriteConfiguration(&ndis_status, ConfigurationHandle, &regkey_name, &parameter);
    }

    // Close the registry
    NdisCloseConfiguration(ConfigurationHandle);
    
    DBGPRINT_S(ndis_status, ("<-- NICWriteRegParameters, Status=%x\n", ndis_status));

    return ndis_status;
}

NDIS_STATUS 
NICAllocAdapterMemory(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Allocate all the memory blocks for send, receive and others

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_FAILURE
    NDIS_STATUS_RESOURCES

--*/    
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    PUCHAR          pMem;
    ULONG           MemPhys;
    LONG            index;
    ULONG           ErrorValue = 0;
    UINT            MaxNumBuffers;
    ULONG           pMemSize;
    PUCHAR          AllocVa;
    NDIS_PHYSICAL_ADDRESS   AllocPa;
    NET_BUFFER_LIST_POOL_PARAMETERS PoolParameters;
    int i;
    ULONG j;
    
    DBGPRINT(MP_TRACE, ("--> NICAllocMemory\n"));

    do
    {
        //
        // Recv
        //
        NdisInitializeNPagedLookasideList(
            &Adapter->RecvLookaside,
            NULL,
            NULL,
            0,
            sizeof(MP_RFD),
            NIC_TAG, 
            0);

        MP_SET_FLAG(Adapter, fMP_ADAPTER_RECV_LOOKASIDE);

        //
        // set the max number of RFDs
        // disable the RFD grow/shrink scheme if user specifies a NumRfd value 
        // larger than NIC_MAX_GROW_RFDS
        // 
        Adapter->MaxNumRfd = max(Adapter->NumRfd, NIC_MAX_GROW_RFDS);
        DBGPRINT(MP_INFO, ("NumRfd = %d\n", Adapter->NumRfd));
        DBGPRINT(MP_INFO, ("MaxNumRfd = %d\n", Adapter->MaxNumRfd));

        //
        // The driver should allocate more data than sizeof(RFD_STRUC) to allow the
        // driver to align the data(after ethernet header) at 8 byte boundary
        //
        Adapter->RfdBufferSize = NIC_BUFFER_SIZE(Adapter) + MORE_DATA_FOR_ALIGN;    

        //
        // alloc the recv net buffer list pool with net buffer inside the list
        //
        NdisZeroMemory(&PoolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
        PoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
        PoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
        PoolParameters.Header.Size = sizeof(PoolParameters);
        PoolParameters.ProtocolId = 0;
        PoolParameters.ContextSize = 0;
        PoolParameters.fAllocateNetBuffer = TRUE;
        PoolParameters.PoolTag = NIC_TAG;

        Adapter->RecvNetBufferListPool = NdisAllocateNetBufferListPool(
                    MP_GET_ADAPTER_HANDLE(Adapter),
                    &PoolParameters); 


        if (Adapter->RecvNetBufferListPool == NULL)
        {
            DBGPRINT(MP_INFO, ("NetBufferListPool Alloc failed\n"));
            ErrorValue = ERRLOG_OUT_OF_PACKET_POOL;
            break;
        }

        //
        // alloc the send net buffer list pool with net buffer inside the list
        //
        NdisZeroMemory(&PoolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
        PoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
        PoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
        PoolParameters.Header.Size = sizeof(PoolParameters);
        PoolParameters.ProtocolId = 0;
        PoolParameters.ContextSize = 0;
        PoolParameters.fAllocateNetBuffer = TRUE;
        PoolParameters.PoolTag = NIC_TAG;

        Adapter->RpcSendNetBufferListPool = NdisAllocateNetBufferListPool(
                    MP_GET_ADAPTER_HANDLE(Adapter),
                    &PoolParameters); 


        if (Adapter->RpcSendNetBufferListPool == NULL)
        {
            DBGPRINT(MP_INFO, ("Send NetBufferListPool Alloc failed\n"));
            ErrorValue = ERRLOG_OUT_OF_PACKET_POOL;
            break;
        }

        Adapter->NumPagesPerBank = DEFAULT_NUM_PAGES_PER_BANK;

        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {

            PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];
            MpegBuffer->Adapter = Adapter;


            MpegBuffer->NumPages = PAGES_PER_MPEG_DEVICE;
            MpegBuffer->LockCnt = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MpegBuffer->NumPages );
            MpegBuffer->Dropped = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MpegBuffer->NumPages );
            MpegBuffer->Remaining = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MpegBuffer->NumPages * sizeof( USHORT ) );
            MpegBuffer->Sizes = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MpegBuffer->NumPages * sizeof( USHORT ) );
            MpegBuffer->PhysicalAddresses = MP_ALLOCMEMTAG( Adapter->AdapterHandle, MpegBuffer->NumPages * sizeof( ULONG ) );
            MpegBuffer->RtpState.Buffer = MP_ALLOCMEMTAG( MpegBuffer->Adapter->AdapterHandle, RTP_SINK_BUFFER_SIZE(Adapter) );

            MpegBuffer->RtpState.InjectMode = 0;

            if( !MpegBuffer->LockCnt || 
                !MpegBuffer->Dropped ||
                !MpegBuffer->Remaining ||
                !MpegBuffer->Sizes ||
                !MpegBuffer->PhysicalAddresses ||
                !MpegBuffer->RtpState.Buffer ) {

                DBGPRINT( MP_ERROR, ("Could not alloc state arrays\n") );
                Status = NDIS_STATUS_RESOURCES;
                break;
            }

            MpegBuffer->TunerIndex = i;

            for( j=0; j<MpegBuffer->NumPages; j++ ) {
                MpegBuffer->LockCnt[j] = 0;
                MpegBuffer->Dropped[j] = 0;
                MpegBuffer->Sizes[j] = 0;
                MpegBuffer->Remaining[j] = 0;
                MpegBuffer->PhysicalAddresses[j] = 0;
            }

            MpegBuffer->TotalNotifies = 0;
        }

        if( Status != NDIS_STATUS_SUCCESS ) {
            //handle any errors in the mpeg loop
            break;
        }
        {
            DEVICE_DESCRIPTION devDescription;
            ULONG MapRegisters;
            RtlZeroMemory( &devDescription, sizeof( DEVICE_DESCRIPTION ) );
            
            devDescription.Version = DEVICE_DESCRIPTION_VERSION;
            devDescription.Master = TRUE;
            devDescription.ScatterGather = TRUE;
            devDescription.Dma32BitAddresses = TRUE;
            devDescription.Dma64BitAddresses = FALSE;
            devDescription.InterfaceType = PCIBus;
            devDescription.MaximumLength = PAGES_PER_MPEG_DEVICE * NUM_MPEG_STREAMS * PAGE_SIZE;

            Adapter->DmaAdapterObject = IoGetDmaAdapter( 
                    Adapter->PhysicalDeviceObject,
                    &devDescription,
                    &MapRegisters);

            if( Adapter->DmaAdapterObject == NULL ) {
                DBGPRINT( MP_ERROR, ("IoGetDmaAdapter failed\n"));
                Status = NDIS_STATUS_RESOURCES;
                break;
            }

            if( MapRegisters < ( devDescription.MaximumLength / PAGE_SIZE ) ) {
                //Adapter->NumPagesPerBank = 16;//MapRegisters / NUM_MPEG_STREAMS / 2;
                DBGPRINT( MP_WARN, ("Didn't get all my map registers (got %d)\n", MapRegisters ));
                DBGPRINT( MP_WARN, ("Adjusting pages per bank to %d\n", Adapter->NumPagesPerBank ));
            }
        }

        for( i=0; i<NUM_MPEG_STREAMS; i++ ) {
            ULONG j;
            PMP_MPEG_BUFFER MpegBuffer = &Adapter->MpegBuffers[i];
            MpegBuffer->Buffers = MP_ALLOCMEMTAG( MpegBuffer->Adapter->AdapterHandle, MpegBuffer->NumPages * sizeof(PUCHAR));
            if( !MpegBuffer->Buffers ) {
                Status = NDIS_STATUS_RESOURCES;
                break;
            }

            for( j=0; j<MpegBuffer->NumPages; j++ ) {
                PHYSICAL_ADDRESS PhysAddr;
                MpegBuffer->Buffers[j] = Adapter->DmaAdapterObject->DmaOperations->AllocateCommonBuffer(
                        Adapter->DmaAdapterObject,
                        PAGE_SIZE,
                        &PhysAddr,
                        FALSE);

                if( !MpegBuffer->Buffers[j] ) {
                    DBGPRINT( MP_ERROR, ("Failed to allocate needed memory for stream %d index %d\n", i, j));
                    Status = NDIS_STATUS_RESOURCES;
                    break;
                }

                MpegBuffer->PhysicalAddresses[j] = PhysAddr.u.LowPart;
            }

            if( Status != NDIS_STATUS_SUCCESS ) {
                break;
            }
        }

    } while (FALSE);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            Adapter->AdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            1,
            ErrorValue);
    }
    DBGPRINT_S(Status, ("<-- NICAllocMemory, Status=%x\n", Status));

    return Status;

}

NDIS_STATUS NICInitRecv(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Initialize receive data structures

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_RESOURCES

--*/    
{
    NDIS_STATUS             Status = NDIS_STATUS_RESOURCES;

    PMP_RFD                 pMpRfd;      
    ULONG                   RfdCount;
    ULONG                   ErrorValue = 0;
    PUCHAR                  RfdBuffer;
    NDIS_PHYSICAL_ADDRESS   RfdBufferPa;
    ULONGLONG               RfdLength;
    
    DBGPRINT(MP_TRACE, ("--> NICInitRecv\n"));

    do
    {
        //
        // Allocate shared memory for all the RFDs
        // If the allocation fails, try a smaller size
        //
        for (RfdCount = Adapter->NumRfd; RfdCount > NIC_MIN_RFDS; RfdCount >>= 1)
        {

            //
            // Integer overflow check
            //
            RfdLength = (ULONGLONG)Adapter->RfdBufferSize * (ULONGLONG)RfdCount;
            if (RfdLength > (ULONG)-1)
            {
                Adapter->RfdBuffer = NULL;
                DBGPRINT(MP_WARN, ("int overflow\n"));
                break;
            }

            Adapter->RfdBuffer = MP_ALLOCMEMTAG( Adapter->AdapterHandle, Adapter->RfdBufferSize * RfdCount );

            if( Adapter->RfdBuffer )
            {
                break;
            }
        }
        Adapter->NumRfdBuffers = RfdCount;
        
        if( !Adapter->RfdBuffer )
        {
            DBGPRINT(MP_WARN, ("failed alloc of rfdbuffer\n"));
            ErrorValue = ERRLOG_OUT_OF_SHARED_MEMORY;
            break;
        }
        
        //
        // Setup each RFD
        // 
        RfdBuffer =  Adapter->RfdBuffer;
        for (RfdCount = 0; RfdCount < Adapter->NumRfdBuffers; RfdCount++)
        {
            pMpRfd = NdisAllocateFromNPagedLookasideList(&Adapter->RecvLookaside);
            if (!pMpRfd)
            {
                DBGPRINT(MP_WARN, ("failed alloc mprfd\n"));
                ErrorValue = ERRLOG_OUT_OF_LOOKASIDE_MEMORY;
                continue;
            }

            pMpRfd->RfdBuffer = RfdBuffer;
        
            ErrorValue = NICAllocRfd(Adapter, pMpRfd);
            if (ErrorValue)
            {
                DBGPRINT(MP_WARN, (" failed alloc rfd\n"));
                NdisFreeToNPagedLookasideList(&Adapter->RecvLookaside, pMpRfd);
                continue;
            }
            //
            // Add this RFD to the RecvList
            // 
            Adapter->CurrNumRfd++;                      
            RfdBuffer += Adapter->RfdBufferSize;
            
            MP_SET_FLAG(pMpRfd, fMP_RFD_ALLOCED_FROM_POOL);

            NICReturnRFD(Adapter, pMpRfd);
        }
    }
    while (FALSE);

    if (Adapter->CurrNumRfd > NIC_MIN_RFDS)
    {
        Status = NDIS_STATUS_SUCCESS;
    }

    //
    // Adapter->CurrNumRfd < NIC_MIN_RFDs
    //
    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            Adapter->AdapterHandle,
            NDIS_ERROR_CODE_OUT_OF_RESOURCES,
            1,
            ErrorValue);
    }

    DBGPRINT_S(Status, ("<-- NICInitRecv, Status=%x\n", Status));

    return Status;
}

ULONG NICAllocRfd(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  PMP_RFD         pMpRfd
    )
/*++
Routine Description:

    Allocate NET_BUFFER_LIST, NET_BUFFER and MDL associated with a RFD

Arguments:

    Adapter     Pointer to our adapter
    pMpRfd      pointer to a RFD

Return Value:

    ERRLOG_OUT_OF_NDIS_PACKET
    ERRLOG_OUT_OF_NDIS_BUFFER

--*/    
{
    NDIS_STATUS         Status;
    PVOID               RfdBuffer;
    ULONG               ErrorValue = 0;

    do
    {
        RfdBuffer = pMpRfd->RfdBuffer;
		
        pMpRfd->Flags = 0;
        pMpRfd->NetBufferList = NULL;
        pMpRfd->Mdl = NULL;
        
        //
        // point our buffer for receives at this Rfd
        // 
        pMpRfd->Mdl = NdisAllocateMdl(Adapter->AdapterHandle, RfdBuffer, NIC_BUFFER_SIZE(Adapter));
        
        if (pMpRfd->Mdl == NULL)
        {
            DBGPRINT(MP_TRACE, ("failed to allocate mdl\n"));
            ErrorValue = ERRLOG_OUT_OF_NDIS_BUFFER;
            break;
        }
        //
        // the NetBufferList may change later(RequestControlSize is not enough)
        //
        pMpRfd->NetBufferList = NdisAllocateNetBufferAndNetBufferList(
                                        Adapter->RecvNetBufferListPool,
                                        0,                      // RequestContolOffsetDelta
                                        0,                      // BackFill Size
                                        pMpRfd->Mdl,     // MdlChain
                                        0,                      // DataOffset
                                        0);                     // DataLength
        
        if (pMpRfd->NetBufferList == NULL)
        {
            DBGPRINT(MP_TRACE, ("failed to allocate net buffer\n"));
            ErrorValue = ERRLOG_OUT_OF_NDIS_PACKET;
            break;
        }
        pMpRfd->NetBufferList->SourceHandle = MP_GET_ADAPTER_HANDLE(Adapter);
        
        //
        // Initialize the NetBufferList
        //
        // Save ptr to MP_RFD in the NBL, used in MPReturnNetBufferLists 
        // 
        MP_GET_NET_BUFFER_LIST_RFD(pMpRfd->NetBufferList) = pMpRfd;


    } while (FALSE);

    if (ErrorValue)
    {
        if (pMpRfd->Mdl)
        {
            NdisFreeMdl(pMpRfd->Mdl);
        }
    }

    return ErrorValue;

}

VOID NICFreeRfd(
    /*IN*/  PMP_ADAPTER     Adapter,
    /*IN*/  PMP_RFD         pMpRfd,
    /*IN*/  BOOLEAN         DispatchLevel
    )
/*++
Routine Description:

    Free a RFD and assocaited NET_BUFFER_LIST

Arguments:

    Adapter         Pointer to our adapter
    pMpRfd          Pointer to a RFD
    DisptchLevel    Specify if the caller is at DISPATCH level

Return Value:

    None                                                    

--*/    
{
    ASSERT(pMpRfd->Mdl);      
    ASSERT(pMpRfd->NetBufferList);  

    NdisFreeMdl(pMpRfd->Mdl);
    NdisFreeNetBufferList(pMpRfd->NetBufferList);
    
    pMpRfd->Mdl = NULL;
    pMpRfd->NetBufferList = NULL;
	
    /*
     * This RFD was from an overflow allocation if this flag is not set
     */
	if (!MP_TEST_FLAG(pMpRfd, fMP_RFD_ALLOCED_FROM_POOL)) {
		MP_FREEMEM( pMpRfd->RfdBuffer );
		pMpRfd->RfdBuffer = NULL;
	}

    NdisFreeToNPagedLookasideList(&Adapter->RecvLookaside, pMpRfd);
}



NDIS_STATUS NICInitializeAdapter(
    /*IN*/  PMP_ADAPTER     Adapter
    )
/*++
Routine Description:

    Initialize the adapter and set up everything

Arguments:

    Adapter     Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_HARD_ERRORS

--*/    
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    int i;

    DBGPRINT(MP_TRACE, ("--> NICInitializeAdapter\n"));

    do
    {
        // set up our link indication variable
        Adapter->MediaState = MediaConnectStateUnknown;
        Adapter->MediaDuplexState = MediaDuplexStateFull;
        Adapter->usDuplexMode = 2;
        Adapter->usLinkSpeed = Adapter->ReportedSpeed;
        Adapter->LinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
            
        Adapter->CurrentPowerState = NdisDeviceStateD0;
        Adapter->NextPowerState    = NdisDeviceStateD0;

        MpegInitDma( Adapter );
        MPSendSlotNumberMsg( Adapter );

    } while (FALSE);
    
    if (Status != NDIS_STATUS_SUCCESS)
    {
        NdisWriteErrorLogEntry(
            Adapter->AdapterHandle,
            NDIS_ERROR_CODE_HARDWARE_FAILURE,
            1,
            ERRLOG_INITIALIZE_ADAPTER);
    }

    DBGPRINT_S(Status, ("<-- NICInitializeAdapter, Status=%x\n", Status));

    return Status;
}    


