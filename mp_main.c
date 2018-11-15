/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_main.c

Abstract:
    This module contains NDIS miniport handlers

Revision History:

Notes:

--*/

#include "precomp.h"

#if DBG
#define _FILENUMBER     'NIAM'
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

#include <wdmsec.h>

#define IOCTL_CETON_STREAM_STATS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            0x0, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)

#define IOCTL_CETON_FIRMWARE_UPDATE \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            1, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)

#define IOCTL_CETON_FIRMWARE_UPDATE_STATUS \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            2, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)

#define IOCTL_CETON_DEVICE_INFO \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            3, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)

#define IOCTL_CETON_TRANSLATION_INIT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            4, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)

#define IOCTL_CETON_LOAD_RECOVERY \
    CTL_CODE(FILE_DEVICE_UNKNOWN, \
            5, \
            METHOD_BUFFERED, \
            FILE_ANY_ACCESS)


typedef struct {
    ULONG32 StreamIndex;
} StreamStatsInput;

typedef struct {
    ULONG32 PacketCount;
    ULONG32 DiscontinuityCount;
    ULONG32 RtpActive;
    ULONG32 TotalNotifies;
    ULONG32 RemoteIpAddr;
    ULONG32 LocalIpAddr;
    ULONG32 RemoteHwAddrLow;
    ULONG32 LocalHwAddrLow;
    ULONG32 RemoteHwAddrHigh;
    ULONG32 LocalHwAddrHigh;
    ULONG32 LocalPort;
    ULONG32 RemotePort;
} StreamStatsOutput;

typedef struct {
    ULONG32 Release;
    ULONG32 Chip;
    ULONG32 Board;
    ULONG32 Image;
} DeviceInfo;



NDIS_HANDLE     NdisMiniportDriverHandle = NULL;
NDIS_HANDLE     MiniportDriverContext = NULL;

#define MAX_BOARDS 20

static int BoardNumberSlots[MAX_BOARDS] = {0};

DRIVER_DISPATCH MPDispatch;

NDIS_STATUS DriverEntry(
    /*IN*/  PDRIVER_OBJECT   DriverObject,
    /*IN*/  PUNICODE_STRING  RegistryPath
    )
/*++
Routine Description:

Arguments:

    DriverObject    -   pointer to the driver object
    RegistryPath    -   pointer to the driver registry path
     
Return Value:
    
    NDIS_STATUS - the value returned by NdisMRegisterMiniport 
    
--*/
{
    NDIS_STATUS                         Status;
    NDIS_MINIPORT_DRIVER_CHARACTERISTICS MPChar;
    
    DBGPRINT(MP_INFO, ("====> DriverEntry %s %s\n", __DATE__, __TIME__));
    
    {
        NDIS_MINIPORT_DRIVER_CHARACTERISTICS    MPChar;
        WDF_DRIVER_CONFIG                       config;
        NTSTATUS                                ntStatus;
        
        DBGPRINT(MP_INFO, ("Ceton InfiniTV on "__DATE__" at "__TIME__ "\n"));
        DBGPRINT(MP_INFO, ("NDIS Version %d.%d\n", MP_NDIS_MAJOR_VERSION, MP_NDIS_MINOR_VERSION));

        //
        // Make sure we are compatible with the version of NDIS supported by OS.
        //
        if (NdisGetVersion() < ((MP_NDIS_MAJOR_VERSION << 16) | MP_NDIS_MINOR_VERSION)){
            DBGPRINT(MP_ERROR, ("This version of driver is not support on this OS\n"));
            return NDIS_STATUS_FAILURE;
        }

        WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
        
        //
        // Set WdfDriverInitNoDispatchOverride flag to tell the framework
        // not to provide dispatch routines for the driver. In other words,
        // the framework must not intercept IRPs that the I/O manager has
        // directed to the driver. In this case, it will be handled by NDIS
        // port driver.
        //
        config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;

        ntStatus = WdfDriverCreate(DriverObject,
                                                RegistryPath,
                                                WDF_NO_OBJECT_ATTRIBUTES,
                                                &config,
                                                WDF_NO_HANDLE);
        if (!NT_SUCCESS(ntStatus)){
            DBGPRINT(MP_ERROR, ("WdfDriverCreate failed\n"));
            return NDIS_STATUS_FAILURE;
        }
    }
    
    //
    // Fill in the Miniport characteristics structure with the version numbers 
    // and the entry points for driver-supplied MiniportXxx 
    //
    NdisZeroMemory(&MPChar, sizeof(MPChar));

    MPChar.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS,
    MPChar.Header.Size = sizeof(NDIS_MINIPORT_DRIVER_CHARACTERISTICS);
    MPChar.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;

    MPChar.MajorNdisVersion             = MP_NDIS_MAJOR_VERSION;
    MPChar.MinorNdisVersion             = MP_NDIS_MINOR_VERSION;
    MPChar.MajorDriverVersion           = NIC_MAJOR_DRIVER_VERSION;
    MPChar.MinorDriverVersion           = NIC_MINOR_DRIVER_VERISON;

    MPChar.SetOptionsHandler = MpSetOptions;
    
    MPChar.InitializeHandlerEx          = MPInitialize;
    MPChar.HaltHandlerEx                = MPHalt;

    MPChar.UnloadHandler                = MPUnload,
    
    MPChar.PauseHandler                 = MPPause;      
    MPChar.RestartHandler               = MPRestart;    
    MPChar.OidRequestHandler            = MPOidRequest;    
    MPChar.SendNetBufferListsHandler    = MPSendNetBufferLists;
    MPChar.ReturnNetBufferListsHandler  = MPReturnNetBufferLists;
    MPChar.CancelSendHandler            = MPCancelSendNetBufferLists;
    MPChar.DevicePnPEventNotifyHandler  = MPPnPEventNotify;
    MPChar.ShutdownHandlerEx            = MPShutdown;
    MPChar.CheckForHangHandlerEx        = MPCheckForHang;
    MPChar.ResetHandlerEx               = MPReset;
    MPChar.CancelOidRequestHandler      = MPCancelOidRequest;
    
    
    DBGPRINT(MP_INFO, ("Calling NdisMRegisterMiniportDriver...\n"));

    Status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, (PNDIS_HANDLE)MiniportDriverContext, &MPChar, &NdisMiniportDriverHandle);

    DBGPRINT(MP_INFO, ("<==== DriverEntry, Status=%x\n", Status));

    return Status;
}


NDIS_STATUS
MpSetOptions(
    /*IN*/ NDIS_HANDLE  NdisMiniportDriverHandle,
    /*IN*/ NDIS_HANDLE  MiniportDriverContext
    )
{
    UNREFERENCED_PARAMETER(NdisMiniportDriverHandle);
    UNREFERENCED_PARAMETER(MiniportDriverContext);
    
    DBGPRINT(MP_TRACE, ("====> MpSetOptions\n"));

    DBGPRINT(MP_TRACE, ("<==== MpSetOptions\n"));

    return (NDIS_STATUS_SUCCESS);
}


NTSTATUS
MPDispatch(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp)
{
    PMP_ADAPTER* DeviceExtension = (PMP_ADAPTER*)NdisGetDeviceReservedExtension( DeviceObject );
    PMP_ADAPTER Adapter = DeviceExtension[0];

    PIO_STACK_LOCATION irpStack;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG inlen;
    PVOID buffer;
    ULONG outlen = 0;


    irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch( irpStack->MajorFunction ) {
        case IRP_MJ_CREATE:
            break;
        case IRP_MJ_CLEANUP:
            break;
        case IRP_MJ_CLOSE:
            break;
        case IRP_MJ_DEVICE_CONTROL:
            buffer = Irp->AssociatedIrp.SystemBuffer;
            inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
            outlen = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
        
            DBGPRINT( MP_TRACE, ("IRP_MJ_DEVICE_CONTROL\n"));

            switch( irpStack->Parameters.DeviceIoControl.IoControlCode ) {
                case IOCTL_CETON_STREAM_STATS:
                    {
                        StreamStatsInput* input = Irp->AssociatedIrp.SystemBuffer;
                        StreamStatsOutput* output = Irp->AssociatedIrp.SystemBuffer;
                        
                        DBGPRINT( MP_TRACE, ("IOCTL_CETON_STREAM_STATS\n"));
                        
                        if (inlen < sizeof(StreamStatsInput)
                            || outlen < sizeof(StreamStatsOutput))
                        {
                            DBGPRINT( MP_WARN, ("Invalid IOCTL length (%d/%d,%d/%d)\n", inlen, sizeof(StreamStatsInput), outlen, sizeof(StreamStatsOutput)));
                            status = STATUS_INVALID_PARAMETER;
                            break;
                        }
                        
                        {
                            ULONG32 StreamIndex = input->StreamIndex;
                            
                            if( StreamIndex < NUM_MPEG_STREAMS ) {
                                output->PacketCount = Adapter->MpegBuffers[StreamIndex].PacketCount;
                                output->DiscontinuityCount = Adapter->MpegBuffers[StreamIndex].DiscontinuityCount;
                                output->RtpActive = Adapter->MpegBuffers[StreamIndex].RtpState.GotSetup;
                                output->TotalNotifies = Adapter->MpegBuffers[StreamIndex].TotalNotifies;
                                //TODO
                                //output->RemoteHwAddrLow = 
                                //output->LocalHwAddrLow = 
                                output->RemoteIpAddr = Adapter->MpegBuffers[StreamIndex].RtpState.RemoteIpAddr;
                                output->LocalIpAddr = Adapter->MpegBuffers[StreamIndex].RtpState.LocalIpAddr;
                                output->LocalPort = Adapter->MpegBuffers[StreamIndex].RtpState.LocalPort;
                                output->RemotePort = Adapter->MpegBuffers[StreamIndex].RtpState.RemotePort;
                            }
                        }
                    }
                    break;
                case IOCTL_CETON_FIRMWARE_UPDATE:
                    {
                        DBGPRINT( MP_TRACE, ("IOCTL_CETON_FIRMWARE_UPDATE\n"));
                        
                        status = MPHandleFirmwareUpdate(
                            Adapter,
                            Irp);
                    }
                    break;
                case IOCTL_CETON_FIRMWARE_UPDATE_STATUS:
                    {
                        status = MPHandleFirmwareUpdateStatus(
                                Adapter,
                                Irp,
                                outlen);
                    }
                    break;
                case IOCTL_CETON_DEVICE_INFO:
                    {
                        DeviceInfo* info = Irp->AssociatedIrp.SystemBuffer;

                        DBGPRINT( MP_TRACE, ("IOCTL_CETON_DEVICE_INFO\n"));

                        if (outlen < sizeof(DeviceInfo)) {
                            DBGPRINT(MP_WARN, ("Invalid IOCTL length (%d/%d)\n", outlen, sizeof(DeviceInfo)));
                            status = STATUS_INVALID_PARAMETER;
                            break;
                        }

                        info->Release = Adapter->ReleaseVersion;
                        info->Chip = Adapter->ChipVersion;
                        info->Board = ctn91xx_read8(Adapter->SystemControlBase, CTN91XX_BOARD_TYPE_OFFSET);
                        info->Image = ctn91xx_read8(Adapter->SystemControlBase, CTN91XX_IMAGE_TYPE_OFFSET);
                    }
                    break;
                case IOCTL_CETON_TRANSLATION_INIT:
                    {
                        DBGPRINT( MP_TRACE, ("IOCTL_CETON_TRANSLATION_INIT\n"));
                        
                        status = MPHandleTranslationInit(
                            Adapter,
                            Irp);

                    }
                    break;
                case IOCTL_CETON_LOAD_RECOVERY:
                    {

                        DBGPRINT( MP_TRACE, ("IOCTL_CETON_LOAD_RECOVERY\n"));
                        
                        status = MPHandleLoadRecovery(
                            Adapter,
                            Irp);

                    }
                    break;
                //handle ioctl commands here
                default:
                    DBGPRINT( MP_WARN, ("Got unknown ioctl code: %d inlen: %d outlen: %d\n", 
                                irpStack->Parameters.DeviceIoControl.IoControlCode,
                                inlen,
                                outlen));
                    break;
            }
            break;
        default:
            break;
    }

    Irp->IoStatus.Information = outlen;
    Irp->IoStatus.Status = status;
    IoCompleteRequest( Irp, IO_NO_INCREMENT );
    return status;
}

NDIS_STATUS
MPRegisterDeviceFile(
        PMP_ADAPTER Adapter)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
    NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceObjectAttributes;
    PDRIVER_DISPATCH DispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1];
    PMP_ADAPTER* DeviceExtension;

    DECLARE_UNICODE_STRING_SIZE(DeviceName, 255);
    DECLARE_UNICODE_STRING_SIZE(DeviceLinkUnicodeString, 255);

    NdisZeroMemory(DispatchTable, (IRP_MJ_MAXIMUM_FUNCTION+1) * sizeof(PDRIVER_DISPATCH));
    DispatchTable[IRP_MJ_CREATE] = MPDispatch;
    DispatchTable[IRP_MJ_CLEANUP] = MPDispatch;
    DispatchTable[IRP_MJ_CLOSE] = MPDispatch;
    DispatchTable[IRP_MJ_DEVICE_CONTROL] = MPDispatch;

    RtlUnicodeStringPrintf(&DeviceName, L"%ws%d", MP_NTDEVICE_STRING, Adapter->BoardNumber);
    RtlUnicodeStringPrintf(&DeviceName, L"%ws%d", MP_LINKNAME_STRING, Adapter->BoardNumber);

    NdisZeroMemory(&DeviceObjectAttributes, sizeof(NDIS_DEVICE_OBJECT_ATTRIBUTES));

    DeviceObjectAttributes.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    DeviceObjectAttributes.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceObjectAttributes.Header.Size = sizeof(NDIS_DEVICE_OBJECT_ATTRIBUTES);
    DeviceObjectAttributes.DeviceName = &DeviceName;
    DeviceObjectAttributes.SymbolicName = &DeviceLinkUnicodeString;
    DeviceObjectAttributes.MajorFunctions = DispatchTable;
    DeviceObjectAttributes.ExtensionSize = sizeof(PMP_ADAPTER);
    //SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RWX_RES_RWX
#if 0
    RtlInitUnicodeString(&SDDL, TEXT("D:P(A;;GA;;;SY)(A;;GRGWGX;;;BA)(A;;GRGWGX;;;WD)(A;;GRGWGX;;;RC)"));
    
    DeviceObjectAttributes.DefaultSDDLString = &SDDL;
#else
    DeviceObjectAttributes.DefaultSDDLString = NULL;
#endif
    DeviceObjectAttributes.DeviceClassGuid = 0;

    Status = NdisRegisterDeviceEx(
            Adapter->AdapterHandle,
            &DeviceObjectAttributes,
            &Adapter->ControlDeviceObject,
            &Adapter->ControlDeviceHandle);

    DeviceExtension = (PMP_ADAPTER*)NdisGetDeviceReservedExtension( Adapter->ControlDeviceObject );
    DeviceExtension[0] = Adapter;

    return Status;
}

NDIS_STATUS
MPUnRegisterDeviceFile(
        PMP_ADAPTER Adapter)
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    if( Adapter->ControlDeviceHandle ) {
        NdisDeregisterDeviceEx(Adapter->ControlDeviceHandle);
        Adapter->ControlDeviceHandle = NULL;
    }

    return Status;
}


void 
ctn91xx_dump_mask_table(PMP_ADAPTER Adapter) 
{
    int i,j;
    
    if (!Adapter) return;
	

	{

        /*
		char* procs[] = {
			"INTERNAL",
			"EXTERNAL"};
            */

        /*
		char* spaces[] = {
			"drm...............",
			"dma...............",
			"gpio..............",
			"cablecard_reg.....",
			"mpeg_reg..........",
			"debug_reg.........",
			"data_reg..........",
			"pid_filter........",
			"mpeg_split........",
			"i2c0..............",
			"i2c1..............",
			"i2c2..............",
			"spit0.............",
			"spit1.............",
			"spit2.............",
			"filter_dma........",
			"timers............"};*/

		for(i = 0; i < 2; i++) {

			//DBGPRINT( MP_INFO, ("%s%s:\n", procs[i], (ctn91xx_which_proc(card) & (1<<i) ? "(ME)" : ""));
			for(j = 0; j < 17; j++) {

				//YES I know this is terrible
				/*DBGPRINT( MP_INFO, ("\t%s%s%s%s\n", 
                    spaces[j],
                    (!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4)) ? "READ  " : "      ",
                    (!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4 + 1)) ? "WRITE " : "      ",
                    (!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4 + 2)) ? "IRQ   " : "      ",
                    ));*/

				/*if(!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4)) {
					
					DBGPRINT( MP_INFO, ("READ "));
				}

				if(!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4 + 1)) {
					
					DBGPRINT( MP_INFO, ("WRITE "));
				}

				if(!ctn91xx_read8(Adapter->MsgBase, MASK_TABLE_OFFSET + j*16 + (i+2)*4 + 2)) {
					
					DBGPRINT( MP_INFO, ("IRQ "));
				}

				DBGPRINT( MP_INFO, ("\n"));*/
			}
			
			DBGPRINT( MP_INFO, ("\n"));
		}
	}
}


NDIS_STATUS 
MPInitialize(
    /*IN*/  NDIS_HANDLE                        MiniportAdapterHandle,
    /*IN*/  NDIS_HANDLE                        MiniportDriverContext,
    /*IN*/  PNDIS_MINIPORT_INIT_PARAMETERS     MiniportInitParameters
    )
/*++
Routine Description:

    MiniportInitialize handler

Arguments:

    MiniportAdapterHandle   The handle NDIS uses to refer to us
    MiniportDriverContext   Handle passed to NDIS when we registered the driver
    MiniportInitParameters  Initialization parameters
    
Return Value:

    NDIS_STATUS_SUCCESS unless something goes wrong

--*/
{
    NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
    NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS  Interrupt;
    NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES   RegistrationAttributes;
    NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES        GeneralAttributes;
    NDIS_PNP_CAPABILITIES          PowerManagementCapabilities;    
    PMP_ADAPTER     Adapter = NULL;
    PVOID           NetworkAddress;
    UINT            index;
    UINT            uiPnpCommandValue;
    ULONG           ulInfoLen;
    ULONG           InterruptVersion;
    int i;
    
    DBGPRINT(MP_INFO, ("====> MPInitialize\n"));

    UNREFERENCED_PARAMETER(MiniportDriverContext);
    
    do
    {

        //
        // Allocate MP_ADAPTER structure
        //
        Status = MpAllocAdapterBlock(&Adapter, MiniportAdapterHandle);
        if (Status != NDIS_STATUS_SUCCESS) 
        {
            DBGPRINT( MP_ERROR, ("Failed to MpAllocAdapterBlock 0x%08x", Status));
            break;
        }

        Adapter->AdapterHandle = MiniportAdapterHandle;


        NdisZeroMemory(&RegistrationAttributes, sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES));
        NdisZeroMemory(&GeneralAttributes, sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES));

        //
        // setting registration attributes
        //
        RegistrationAttributes.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
        RegistrationAttributes.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
        RegistrationAttributes.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES);

        RegistrationAttributes.MiniportAdapterContext = (NDIS_HANDLE)Adapter;
        RegistrationAttributes.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE;
        
        RegistrationAttributes.CheckForHangTimeInSeconds = 2;
        RegistrationAttributes.InterfaceType = NIC_INTERFACE_TYPE;

        Status = NdisMSetMiniportAttributes(MiniportAdapterHandle, (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&RegistrationAttributes);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT( MP_ERROR, ("Failed to NdisMSetMiniportAttributes 0x%08x", Status));
            break;
        }
        
        //
        // Read the registry parameters
        //
        Status = NICReadRegParameters(Adapter);
        if (Status != NDIS_STATUS_SUCCESS) {
            DBGPRINT( MP_ERROR, ("Failed to NICReadRegParameters 0x%08x", Status));
            break;
        }

        //find free board number slot
        for( i=0; i<MAX_BOARDS; i++ ) {
            if( BoardNumberSlots[i] == 0 ) {
                BoardNumberSlots[i] = 1;
                Adapter->BoardNumber = i;
                break;
            }
        }

        MPRegisterDeviceFile(Adapter);

        //
        // Find the physical adapter
        //
        Status = MpFindAdapter(Adapter, MiniportInitParameters->AllocatedResources);
        if (Status != NDIS_STATUS_SUCCESS) {
            DBGPRINT( MP_ERROR, ("Failed to MpFindAdapter 0x%08x", Status));
            break;
        }

        //
        // Read additional info from NIC such as MAC address
        //
        Status = NICReadAdapterInfo(Adapter);
        if (Status != NDIS_STATUS_SUCCESS)  {
            DBGPRINT( MP_ERROR, ("Failed to NICReadAdapterInfo 0x%08x", Status));
            break;
        }

        //
        // set up generic attributes
        //
        GeneralAttributes.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
        GeneralAttributes.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
        GeneralAttributes.Header.Size = sizeof(NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES);

        GeneralAttributes.MediaType = NIC_MEDIA_TYPE;

        GeneralAttributes.MtuSize = NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE;
        GeneralAttributes.MaxXmitLinkSpeed = NIC_MEDIA_MAX_SPEED;
        GeneralAttributes.MaxRcvLinkSpeed = NIC_MEDIA_MAX_SPEED;
        GeneralAttributes.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
        GeneralAttributes.RcvLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
        GeneralAttributes.MediaConnectState = MediaConnectStateUnknown;
        GeneralAttributes.MediaDuplexState = MediaDuplexStateUnknown;
        GeneralAttributes.LookaheadSize = NIC_MAX_PACKET_SIZE - NIC_HEADER_SIZE;

        MPFillPoMgmtCaps (Adapter, 
                          &PowerManagementCapabilities, 
                          &Status,
                          &ulInfoLen);

        if (NT_SUCCESS(Status))
        {
            GeneralAttributes.PowerManagementCapabilities = &PowerManagementCapabilities;
        }
        else
        {
            GeneralAttributes.PowerManagementCapabilities = NULL;
        }

        //
        // do not fail the call because of failure to get PM caps
        //
        Status = NDIS_STATUS_SUCCESS;

        GeneralAttributes.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | 
                                       NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
                                       NDIS_MAC_OPTION_NO_LOOPBACK;

        GeneralAttributes.SupportedPacketFilters = NDIS_PACKET_TYPE_DIRECTED |
                                                   NDIS_PACKET_TYPE_MULTICAST |
                                                   NDIS_PACKET_TYPE_ALL_MULTICAST |
                                                   NDIS_PACKET_TYPE_BROADCAST;
        
        GeneralAttributes.MaxMulticastListSize = NIC_MAX_MCAST_LIST;
        GeneralAttributes.MacAddressLength = ETH_LENGTH_OF_ADDRESS;
        NdisMoveMemory(GeneralAttributes.PermanentMacAddress,
                       Adapter->PermanentAddress,
                       ETH_LENGTH_OF_ADDRESS);

        NdisMoveMemory(GeneralAttributes.CurrentMacAddress,
                       Adapter->CurrentAddress,
                       ETH_LENGTH_OF_ADDRESS);

        
        GeneralAttributes.PhysicalMediumType = NdisPhysicalMediumUnspecified;
        GeneralAttributes.RecvScaleCapabilities = NULL;
        GeneralAttributes.AccessType = NET_IF_ACCESS_BROADCAST; // NET_IF_ACCESS_BROADCAST for a typical ethernet adapter
        GeneralAttributes.DirectionType = NET_IF_DIRECTION_SENDRECEIVE; // NET_IF_DIRECTION_SENDRECEIVE for a typical ethernet adapter
        GeneralAttributes.ConnectionType = NET_IF_CONNECTION_DEDICATED;  // NET_IF_CONNECTION_DEDICATED for a typical ethernet adapter
        GeneralAttributes.IfType = IF_TYPE_ETHERNET_CSMACD; // IF_TYPE_ETHERNET_CSMACD for a typical ethernet adapter (regardless of speed)
        GeneralAttributes.IfConnectorPresent = TRUE; // RFC 2665 TRUE if physical adapter

        GeneralAttributes.SupportedStatistics = NDIS_STATISTICS_XMIT_OK_SUPPORTED |
                                                NDIS_STATISTICS_RCV_OK_SUPPORTED |
                                                NDIS_STATISTICS_XMIT_ERROR_SUPPORTED |
                                                NDIS_STATISTICS_RCV_ERROR_SUPPORTED |
                                                NDIS_STATISTICS_RCV_CRC_ERROR_SUPPORTED |
                                                NDIS_STATISTICS_RCV_NO_BUFFER_SUPPORTED |
                                                NDIS_STATISTICS_TRANSMIT_QUEUE_LENGTH_SUPPORTED |
                                                NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED;
                      
        GeneralAttributes.SupportedOidList = NICSupportedOids;
        GeneralAttributes.SupportedOidListLength = sizeof(NICSupportedOids);

        Status = NdisMSetMiniportAttributes(MiniportAdapterHandle,
                                            (PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&GeneralAttributes);
        if (Status != NDIS_STATUS_SUCCESS) 
        {
            DBGPRINT( MP_ERROR, ("Failed to set miniport attributes 0x%08x", Status));
        }

        NdisMGetDeviceProperty( 
                Adapter->AdapterHandle, 
                &Adapter->PhysicalDeviceObject, 
                &Adapter->Fdo,
               &Adapter->NextDeviceObject,
                NULL,
                NULL );
        
        {
            WDF_OBJECT_ATTRIBUTES  attributes;
            NTSTATUS ntStatus;
            
            WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, WDF_DEVICE_INFO);
            
            ntStatus = WdfDeviceMiniportCreate(WdfGetDriver(),
                 &attributes,
                 Adapter->Fdo,
                 Adapter->NextDeviceObject,
                 Adapter->PhysicalDeviceObject,
                 &Adapter->WdfDevice);
            
            if (!NT_SUCCESS (ntStatus))
            {
                DBGPRINT( MP_ERROR, ("WdfDeviceMiniportCreate failed (0x%x)\n",
                    ntStatus));
                Status = NDIS_STATUS_FAILURE;
                break;
            }
            
            GetWdfDeviceInfo(Adapter->WdfDevice)->Adapter = Adapter;
        }

        //
        // Allocate all other memory blocks including shared memory
        //
        Status = NICAllocAdapterMemory(Adapter);
        if (Status != NDIS_STATUS_SUCCESS) 
        {
            DBGPRINT( MP_ERROR, ("Failed to alloc adapter 0x%08x", Status));
            break;
        }


        //
        // Init receive data structures
        //
        Status = NICInitRecv(Adapter);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT( MP_ERROR, ("Failed to init recv 0x%08x", Status));
            break;
        }
        //
        // Map bus-relative registers to virtual system-space
        // 
        Status = NdisMMapIoSpace(
                     &Adapter->RegisterBase,
                     Adapter->AdapterHandle,
                     Adapter->MemPhysAddress,
                     Adapter->MemPhysSize);
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT(MP_ERROR, ("NdisMMapIoSpace failed\n"));
    
            NdisWriteErrorLogEntry(
                Adapter->AdapterHandle,
                NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                1,
                ERRLOG_MAP_IO_SPACE);
        
            break;
        }

        DBGPRINT(MP_INFO, ("RegisterBase="PTR_FORMAT"\n", Adapter->RegisterBase));
        
#if DBG
        {
            UINT test_value = ctn91xx_read32( Adapter->RegisterBase, 0x1000 );
            DBGPRINT( MP_INFO, ("test_value at 0x1000 %08x\n", test_value ));
        }
#endif

        if (Adapter->TranslationMemPhysSize)
        {
            Status = NdisMMapIoSpace(
                    &Adapter->TranslationMap,
                    Adapter->AdapterHandle,
                    Adapter->TranslationMemPhysAddress,
                    Adapter->TranslationMemPhysSize);
            if (Status != NDIS_STATUS_SUCCESS) {
                DBGPRINT(MP_ERROR, ("NdisMMapIoSpace for translation base failed\n"));

                NdisWriteErrorLogEntry(
                        Adapter->AdapterHandle,
                        NDIS_ERROR_CODE_RESOURCE_CONFLICT,
                        1,
                        ERRLOG_MAP_IO_SPACE);
                break;
            }
        }

        Adapter->DmaBase = Adapter->RegisterBase + DMA_REG_OFFSET;
        Adapter->TimerBase = Adapter->RegisterBase + TIMER_REG_OFFSET;
        Adapter->SystemControlBase = Adapter->RegisterBase + SYSTEM_CONTROL_OFFSET;
        Adapter->EPCSBase = Adapter->RegisterBase + EPCS_REG_OFFSET;

        Adapter->ReleaseVersion = ctn91xx_read16(Adapter->SystemControlBase, CTN91XX_RELEASE_VERSION_OFFSET);
        Adapter->ChipVersion = ctn91xx_read16(Adapter->SystemControlBase, CTN91XX_CHIP_VERSION_OFFSET);
        DBGPRINT( MP_INFO, ("Adapter->ReleaseVersion %08x\n", Adapter->ReleaseVersion ));
        DBGPRINT( MP_INFO, ("Adapter->RegisterBase %p\n", Adapter->RegisterBase ));
        
        Adapter->MsgBase = Adapter->RegisterBase + MSG_BUFFER_OFFSET;
        Adapter->TicksPerUS = (ctn91xx_read32(Adapter->SystemControlBase, SYSTEM_CONTROL_CLOCK_FREQ) / 1000000);

        //
        // Init the hardware and set up everything
        //
        Status = NICInitializeAdapter(Adapter);
        if (Status != NDIS_STATUS_SUCCESS) {
            DBGPRINT( MP_ERROR, ("Failed to init adaptor 0x%08x", Status));
            break;
        }
        
        //
        // Register the interrupt
        //
        //
        
        //
        // the embeded NDIS interrupt structure is already zero'ed out
        // as part of the adapter structure
        //
        NdisZeroMemory(&Interrupt, sizeof(NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS));
        
        Interrupt.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_INTERRUPT;
        Interrupt.Header.Revision = NDIS_MINIPORT_INTERRUPT_REVISION_1;
        Interrupt.Header.Size = sizeof(NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS);

        Interrupt.InterruptHandler = MPIsr;
        Interrupt.InterruptDpcHandler = MPHandleInterrupt;
        Interrupt.DisableInterruptHandler = NULL;
        Interrupt.EnableInterruptHandler = NULL;



        Status = NdisMRegisterInterruptEx(Adapter->AdapterHandle,
                                          Adapter,
                                          &Interrupt,
                                          &Adapter->NdisInterruptHandle
                                          );
        
                                        
        if (Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT(MP_ERROR, ("NdisMRegisterInterrupt failed\n"));
    
            NdisWriteErrorLogEntry(
                Adapter->AdapterHandle,
                NDIS_ERROR_CODE_INTERRUPT_CONNECT,
                0);
        
            break;
        }
        
        //
        // If the driver support MSI
        //
        Adapter->InterruptType = Interrupt.InterruptType;

        if (Adapter->InterruptType == NDIS_CONNECT_MESSAGE_BASED)
        {
            Adapter->MessageInfoTable = Interrupt.MessageInfoTable;
        }
        
        //
        // If the driver supports MSI, here it should what kind of interrupt is granted. If MSI is granted,
        // the driver can check Adapter->MessageInfoTable to get MSI information
        //
        
        
        MP_SET_FLAG(Adapter, fMP_ADAPTER_INTERRUPT_IN_USE);


        
        //
        // initial state is paused
        //
        Adapter->AdapterState = NicPaused;
 
        NICUpdateLinkState( Adapter, FALSE );

        ctn91xx_write8( 1, Adapter->MsgBase, MSG_BUFFER_INT_ENABLE );

        DBGPRINT( MP_INFO, ("CTN91XX_RELEASE_VERSION_OFFSET: %d\n", ctn91xx_read16( Adapter->SystemControlBase, CTN91XX_RELEASE_VERSION_OFFSET )));
        DBGPRINT( MP_INFO, ("CTN91XX_CHIP_VERSION_OFFSET: %d\n", ctn91xx_read16( Adapter->SystemControlBase, CTN91XX_CHIP_VERSION_OFFSET )));
        DBGPRINT( MP_INFO, ("MSG_BUFFER_INT_ENABLE: %d\n", ctn91xx_read8( Adapter->MsgBase, MSG_BUFFER_INT_ENABLE )));
        //ctn91xx_dump_mask_table(Adapter);

    } while (FALSE);

    if (Adapter && (Status != NDIS_STATUS_SUCCESS))
    {
        //
        // Undo everything if it failed
        //
        MP_DEC_REF(Adapter);
        MpFreeAdapter(Adapter);
    }

    DBGPRINT(MP_INFO, ("<==== MPInitialize, Status=%x\n", Status));
    
    return Status;
}


NDIS_STATUS 
MPPause(
    /*IN*/  NDIS_HANDLE                         MiniportAdapterContext,    
    /*IN*/  PNDIS_MINIPORT_PAUSE_PARAMETERS     MiniportPauseParameters
    )
/*++
 
Routine Description:
    
    MiniportPause handler
    The driver can't indicate any packet, send complete all the pending send requests. 
    and wait for all the packets returned to it.
    
Argument:

    MiniportAdapterContext  Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING

NOTE: A miniport can't fail the pause request

--*/
{
    PMP_ADAPTER         Adapter = (PMP_ADAPTER) MiniportAdapterContext;
    NDIS_STATUS         Status; 
    LONG                Count;

    UNREFERENCED_PARAMETER(MiniportPauseParameters);
    
    DBGPRINT(MP_WARN, ("====> MPPause\n"));

    ASSERT(Adapter->AdapterState == NicRunning);

    NdisAcquireSpinLock(&Adapter->RcvLock);
    Adapter->AdapterState = NicPausing;
    NdisReleaseSpinLock(&Adapter->RcvLock);    

    do 
    {
        //
        // Complete all the pending sends
        // 
        //NdisAcquireSpinLock(&Adapter->SendLock);
        //MpFreeQueuedSendNetBufferLists(Adapter);
        //NdisReleaseSpinLock(&Adapter->SendLock);

        NdisAcquireSpinLock(&Adapter->RcvLock);        
        MP_DEC_RCV_REF(Adapter);
        Count = MP_GET_RCV_REF(Adapter);
        if (Count ==0)
        {
            Adapter->AdapterState = NicPaused;            
            Status = NDIS_STATUS_SUCCESS;
        }
        else
        {
            Status = NDIS_STATUS_PENDING;
        }
        NdisReleaseSpinLock(&Adapter->RcvLock);    
        
    }
    while (FALSE);
    
    DBGPRINT(MP_WARN, ("<==== MPPause\n"));
    return Status;
}
        
    
NDIS_STATUS 
MPRestart(
    /*IN*/  NDIS_HANDLE                         MiniportAdapterContext,    
    /*IN*/  PNDIS_MINIPORT_RESTART_PARAMETERS   MiniportRestartParameters
    )
/*++
 
Routine Description:
    
    MiniportRestart handler
    The driver resumes its normal working state
    
Argument:

    MiniportAdapterContext  Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING  Can it return pending
    NDIS_STATUS_XXX      The driver fails to restart


--*/
{
    PMP_ADAPTER                  Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    PNDIS_RESTART_ATTRIBUTES     NdisRestartAttributes;
    PNDIS_RESTART_GENERAL_ATTRIBUTES  NdisGeneralAttributes;

    UNREFERENCED_PARAMETER(MiniportRestartParameters);

    DBGPRINT(MP_WARN, ("====> MPRestart\n"));
    
    NdisRestartAttributes = MiniportRestartParameters->RestartAttributes;

    //
    // If NdisRestartAttributes is not NULL, then miniport can modify generic attributes and add
    // new media specific info attributes at the end. Otherwise, NDIS restarts the miniport because 
    // of other reason, miniport should not try to modify/add attributes
    //
    if (NdisRestartAttributes != NULL)
    {

        ASSERT(NdisRestartAttributes->Oid == OID_GEN_MINIPORT_RESTART_ATTRIBUTES);
    
        NdisGeneralAttributes = (PNDIS_RESTART_GENERAL_ATTRIBUTES)NdisRestartAttributes->Data;
    
        //
        // Check to see if we need to change any attributes, for example, the driver can change the current
        // MAC address here. Or the driver can add media specific info attributes.
        //
    }
    NdisAcquireSpinLock(&Adapter->RcvLock);
    MP_INC_RCV_REF(Adapter);
    Adapter->AdapterState = NicRunning;
    NdisReleaseSpinLock(&Adapter->RcvLock);    
    

    DBGPRINT(MP_WARN, ("<==== MPRestart\n"));
    return NDIS_STATUS_SUCCESS;

}
 
  
BOOLEAN 
MPCheckForHang(
    /*IN*/  NDIS_HANDLE     MiniportAdapterContext
    )
/*++

Routine Description:
    
    MiniportCheckForHang handler
    Ndis call this handler forcing the miniport to check if it needs reset or not,
    If the miniport needs reset, then it should call its reset function
    
Arguments:

    MiniportAdapterContext  Pointer to our adapter

Return Value:

    None.

Note: 
    CheckForHang handler is called in the context of a timer DPC. 
    take advantage of this fact when acquiring/releasing spinlocks

    NOTE: NDIS60 Miniport won't have CheckForHang handler.

--*/
{
    PMP_ADAPTER         Adapter = (PMP_ADAPTER) MiniportAdapterContext;
    NDIS_MEDIA_CONNECT_STATE CurrMediaState;
    NDIS_STATUS         Status;
    BOOLEAN             NeedReset = TRUE;
    NDIS_REQUEST_TYPE   RequestType;
    BOOLEAN             DispatchLevel = (NDIS_CURRENT_IRQL() == DISPATCH_LEVEL);
    
    //
    // Just skip this part if the adapter is doing link detection
    //
    do
    {
        //
        // any nonrecoverable hardware error?
        //
        if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_NON_RECOVER_ERROR))
        {
            DBGPRINT(MP_WARN, ("Non recoverable error - remove\n"));
            break;
        }
            
        //
        // hardware failure?
        //
        if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_HARDWARE_ERROR))
        {
            DBGPRINT(MP_WARN, ("hardware error - reset\n"));
            break;
        }
          
        //
        // Is send stuck?                  
        //
        MP_ACQUIRE_SPIN_LOCK(&Adapter->SendLock, DispatchLevel);
    
        NeedReset = FALSE;

        MP_RELEASE_SPIN_LOCK(&Adapter->SendLock, DispatchLevel);
        MP_ACQUIRE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);

        //
        // Update the RFD shrink count                                          
        //
        if (Adapter->CurrNumRfd > Adapter->NumRfd)
        {
            Adapter->RfdShrinkCount++;          
        }

        MP_RELEASE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);
    }
    while (FALSE);
    //
    // If need reset, ask NDIS to reset the miniport
    //
    return NeedReset;
}

VOID MPHalt(
    /*IN*/  NDIS_HANDLE             MiniportAdapterContext,
    /*IN*/  NDIS_HALT_ACTION        HaltAction
    )

/*++

Routine Description:
    
    MiniportHalt handler
    
Arguments:

    MiniportAdapterContext  Pointer to our adapter
    HaltAction              The reason adapter is being halted

Return Value:

    None
    
--*/
{
    LONG            Count;

    PMP_ADAPTER     Adapter = (PMP_ADAPTER) MiniportAdapterContext;

    if (HaltAction == NdisHaltDeviceSurpriseRemoved)
    {
        //
        // do something here. For example make sure halt will not rely on hardware
        // to generate an interrupt in order to complete a pending operation.
        //
    }

    ASSERT(Adapter->AdapterState == NicPaused);
        
    MP_SET_FLAG(Adapter, fMP_ADAPTER_HALT_IN_PROGRESS);
                                           
    DBGPRINT(MP_WARN, ("====> MPHalt\n"));
    
    if (Adapter->EEPROM_WorkItem != NULL) {
        DBGPRINT(MP_INFO, ("Stopping Work Item\n"));
        WdfWorkItemFlush(
            Adapter->EEPROM_WorkItem);
        WdfObjectDelete(Adapter->EEPROM_WorkItem);
        DBGPRINT(MP_INFO, ("Stopped Work Item\n"));
        Adapter->EEPROM_WorkItem = NULL;
    }

    //
    // Call Shutdown handler to disable interrupts and turn the hardware off 
    // by issuing a full reset. since we are not calling our shutdown handler
    // as a result of a bugcheck, we can use NdisShutdownPowerOff as the reason for
    // shutting down the adapter.
    //
    MPShutdown(MiniportAdapterContext, NdisShutdownPowerOff);
    
    //
    // Deregister interrupt as early as possible
    //
    if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_INTERRUPT_IN_USE))
    {
        NdisMDeregisterInterruptEx(Adapter->NdisInterruptHandle);                           
        MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_INTERRUPT_IN_USE);
    }

    //
    // Decrement the ref count which was incremented in MPInitialize
    //
    Count = MP_DEC_REF(Adapter);

    //
    // Possible non-zero ref counts mean one or more of the following conditions: 
    // 1) Pending async shared memory allocation;
    // 2) DPC's are not finished (e.g. link detection)
    //
    if (Count)
    {
        DBGPRINT(MP_WARN, ("RefCount=%d --- waiting!\n", MP_GET_REF(Adapter)));

        while (TRUE)
        {
            if (NdisWaitEvent(&Adapter->ExitEvent, 2000))
            {
                break;
            }

            DBGPRINT(MP_WARN, ("RefCount=%d --- rewaiting!\n", MP_GET_REF(Adapter)));
        }
    }

    MPUnRegisterDeviceFile(Adapter);

    //Clear board number slot
    BoardNumberSlots[Adapter->BoardNumber] = 0;

    //
    // Free the entire adapter object, including the shared memory structures.
    //
    MpFreeAdapter(Adapter);

    DBGPRINT(MP_WARN, ("<==== MPHalt\n"));
}

NDIS_STATUS 
MPReset(
    /*IN*/  NDIS_HANDLE     MiniportAdapterContext,
    /*OUT*/ PBOOLEAN        AddressingReset
    )
/*++

Routine Description:
    
    MiniportReset handler
    
Arguments:

    AddressingReset         To let NDIS know whether we need help from it with our reset
    MiniportAdapterContext  Pointer to our adapter

Return Value:

    NDIS_STATUS_SUCCESS
    NDIS_STATUS_PENDING
    NDIS_STATUS_RESET_IN_PROGRESS
    NDIS_STATUS_HARD_ERRORS

Note:
    
--*/
{
    NDIS_STATUS         Status = NDIS_STATUS_SUCCESS;
    PNDIS_OID_REQUEST   PendingRequest;
    PMP_ADAPTER         Adapter = (PMP_ADAPTER) MiniportAdapterContext;

    DBGPRINT(MP_WARN, ("====> MPReset\n"));

    *AddressingReset = TRUE;

    NdisAcquireSpinLock(&Adapter->Lock);
    NdisDprAcquireSpinLock(&Adapter->SendLock);
    NdisDprAcquireSpinLock(&Adapter->RcvLock);


    do
    {
        ASSERT(!MP_TEST_FLAG(Adapter, fMP_ADAPTER_HALT_IN_PROGRESS));
  
        //
        // Is this adapter already doing a reset?
        //
        if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS))
        {
            Status = NDIS_STATUS_RESET_IN_PROGRESS;
            goto exit;
        }

        MP_SET_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS);

        //
        // Abort any pending request
        //
        if (Adapter->PendingRequest != NULL)
        {
            PendingRequest = Adapter->PendingRequest;
            Adapter->PendingRequest = NULL;

            DBGPRINT( MP_WARN, ("Clearing pending request\n"));

            NdisDprReleaseSpinLock(&Adapter->RcvLock);
            NdisDprReleaseSpinLock(&Adapter->SendLock);
            NdisReleaseSpinLock(&Adapter->Lock);
            
            NdisMOidRequestComplete(Adapter->AdapterHandle, 
                                    PendingRequest, 
                                    NDIS_STATUS_REQUEST_ABORTED);

            NdisAcquireSpinLock(&Adapter->Lock);
            NdisDprAcquireSpinLock(&Adapter->SendLock);
            NdisDprAcquireSpinLock(&Adapter->RcvLock);
    
        } 

        //
        // Is this adapter going to be removed
        //
        if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_NON_RECOVER_ERROR))
        {
           Status = NDIS_STATUS_HARD_ERRORS;
           if (MP_TEST_FLAG(Adapter, fMP_ADAPTER_REMOVE_IN_PROGRESS))
           {
               MP_EXIT;
           }
           //                       
           // This is an unrecoverable hardware failure. 
           // We need to tell NDIS to remove this miniport
           // 
           MP_SET_FLAG(Adapter, fMP_ADAPTER_REMOVE_IN_PROGRESS);
           MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS);
           
           NdisDprReleaseSpinLock(&Adapter->RcvLock);
           NdisDprReleaseSpinLock(&Adapter->SendLock);
           NdisReleaseSpinLock(&Adapter->Lock);
           
           NdisWriteErrorLogEntry(
               Adapter->AdapterHandle,
               NDIS_ERROR_CODE_HARDWARE_FAILURE,
               1,
               ERRLOG_REMOVE_MINIPORT);
           
           NdisMRemoveMiniport(Adapter->AdapterHandle);
           
           DBGPRINT_S(Status, ("<==== MPReset, Status=%x\n", Status));
            
           return Status;
        }   
                

        //
        // Disable the interrupt and issue a reset to the NIC
        //
        //TODO disable interrupts
        //TODO reset?

        //
        // release all the locks and then acquire back the send lock
        // we are going to clean up the send queues
        // which may involve calling Ndis APIs
        // release all the locks before grabbing the send lock to
        // avoid deadlocks
        //
        NdisDprReleaseSpinLock(&Adapter->RcvLock);
        NdisDprReleaseSpinLock(&Adapter->SendLock);
        NdisReleaseSpinLock(&Adapter->Lock);
        
        NdisAcquireSpinLock(&Adapter->SendLock);


        //
        // This is a deserialized miniport, we need to free all the send packets
        // Free the packets on SendWaitList                                                           
        //
        MpFreeQueuedSendNetBufferLists(Adapter);

        //
        // Free the packets being actively sent & stopped
        //
        MpFreeBusySendNetBufferLists(Adapter);

#if DBG
        if (MP_GET_REF(Adapter) > 1)
        {
            DBGPRINT(MP_WARN, ("RefCount=%d\n", MP_GET_REF(Adapter)));
        }
#endif

        NdisReleaseSpinLock(&Adapter->SendLock);

        //
        // get all the locks again in the right order
        //
        NdisAcquireSpinLock(&Adapter->Lock);
        NdisDprAcquireSpinLock(&Adapter->SendLock);
        NdisDprAcquireSpinLock(&Adapter->RcvLock);

        //
        // Reset the RFD list and re-start RU         
        //
        NICResetRecv(Adapter);

        if (Status != NDIS_STATUS_SUCCESS) 
        {
            //
            // Are we having failures in a few consecutive resets?                  
            // 
            if (Adapter->HwErrCount < NIC_HARDWARE_ERROR_THRESHOLD)
            {
                //
                // It's not over the threshold yet, let it to continue
                // 
                Adapter->HwErrCount++;
            }
            else
            {
                //
                // This is an unrecoverable hardware failure. 
                // We need to tell NDIS to remove this miniport
                // 
                MP_SET_FLAG(Adapter, fMP_ADAPTER_REMOVE_IN_PROGRESS);
                MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS);
                
                NdisDprReleaseSpinLock(&Adapter->RcvLock);
                NdisDprReleaseSpinLock(&Adapter->SendLock);
                NdisReleaseSpinLock(&Adapter->Lock);
                
                NdisWriteErrorLogEntry(
                    Adapter->AdapterHandle,
                    NDIS_ERROR_CODE_HARDWARE_FAILURE,
                    1,
                    ERRLOG_REMOVE_MINIPORT);
                     
                NdisMRemoveMiniport(Adapter->AdapterHandle);
                
                DBGPRINT_S(Status, ("<==== MPReset, Status=%x\n", Status));
                return Status;
            }
            
            break;
        }
        
        Adapter->HwErrCount = 0;
        MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_HARDWARE_ERROR);
    } 
    while (FALSE);

    MP_CLEAR_FLAG(Adapter, fMP_ADAPTER_RESET_IN_PROGRESS);

exit:

    NdisDprReleaseSpinLock(&Adapter->RcvLock);
    NdisDprReleaseSpinLock(&Adapter->SendLock);
    NdisReleaseSpinLock(&Adapter->Lock);
        
    DBGPRINT( MP_WARN, ("<==== MPReset, Status=%x\n", Status));
    return(Status);
}

VOID 
MPReturnNetBufferLists(
    /*IN*/  NDIS_HANDLE         MiniportAdapterContext,
    /*IN*/  PNET_BUFFER_LIST    NetBufferLists,
    /*IN*/  ULONG               ReturnFlags
    )
/*++

Routine Description:
    
    MiniportReturnNetBufferLists handler
    NDIS calls this handler to return the ownership of one or more NetBufferLists and 
    their embedded NetBuffers to the miniport driver.
    
Arguments:

    MiniportAdapterContext  Pointer to our adapter
    NetBufferLists          A linked list of NetBufferLists that miniport driver allocated for one or more 
                            previous receive indications. The list can include NetBuferLists from different
                            previous calls to NdisMIndicateNetBufferLists.
    ReturnFlags             Flags specifying if the caller is at DISPATCH level                            

Return Value:

    None

    
--*/
{
    PMP_ADAPTER         Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    PMP_RFD             pMpRfd;
    ULONG               Count;
    PNET_BUFFER_LIST    NetBufList;
    PNET_BUFFER_LIST    NextNetBufList;
    BOOLEAN             DispatchLevel;

    DBGPRINT(MP_TRACE, ("====> MPReturnNetBufferLists\n"));
    //
    // Later we need to check if the request control size change
    // If yes, return the NetBufferList  to pool, and reallocate 
    // one the RFD
    // 
    DispatchLevel = NDIS_TEST_RETURN_AT_DISPATCH_LEVEL(ReturnFlags);

    MP_ACQUIRE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);
    
    for (NetBufList = NetBufferLists;
            NetBufList != NULL;
            NetBufList = NextNetBufList)
    {
        NextNetBufList = NET_BUFFER_LIST_NEXT_NBL(NetBufList);

        pMpRfd = MP_GET_NET_BUFFER_LIST_RFD(NetBufList);
    
        ASSERT(pMpRfd);

        ASSERT(MP_TEST_FLAG(pMpRfd, fMP_RFD_RECV_PEND));
        MP_CLEAR_FLAG(pMpRfd, fMP_RFD_RECV_PEND);

        RemoveEntryList((PLIST_ENTRY)pMpRfd);

        if (Adapter->RfdShrinkCount < NIC_RFD_SHRINK_THRESHOLD
                || MP_TEST_FLAG(pMpRfd, fMP_RFD_ALLOCED_FROM_POOL))
        {
            NICReturnRFD(Adapter, pMpRfd);
        }
        else
        {
            //
            // Need to shrink the count of RFDs, if the RFD is not allocated during
            // initialization time, free it.
            // 
            ASSERT(Adapter->CurrNumRfd > Adapter->NumRfd);

            Adapter->RfdShrinkCount = 0;
            NICFreeRfd(Adapter, pMpRfd, TRUE);
            Adapter->CurrNumRfd--;

            DBGPRINT(MP_TRACE, ("Shrink... CurrNumRfd = %d\n", Adapter->CurrNumRfd));
        }

        
        //
        // note that we get the ref count here, but check
        // to see if it is zero and signal the event -after-
        // releasing the SpinLock. otherwise, we may let the Halthandler
        // continue while we are holding a lock.
        //
        MP_DEC_RCV_REF(Adapter);
    }
    

    Count =  MP_GET_RCV_REF(Adapter);
    if ((Count == 0) && (Adapter->AdapterState == NicPausing))
    {
        //
        // If all the NetBufferLists are returned and miniport is pausing,
        // complete the pause
        // 
        
        Adapter->AdapterState = NicPaused;
        MP_RELEASE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);
        NdisMPauseComplete(Adapter->AdapterHandle);
        MP_ACQUIRE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);
    }
    
    MP_RELEASE_SPIN_LOCK(&Adapter->RcvLock, DispatchLevel);
    
    //
    // Only halting the miniport will set AllPacketsReturnedEvent
    //
    if (Count == 0)
    {
        NdisSetEvent(&Adapter->AllPacketsReturnedEvent);
    }

    DBGPRINT(MP_TRACE, ("<==== MPReturnNetBufferLists\n"));
}

VOID 
MPSendNetBufferListsCommon(
    /*IN*/  NDIS_HANDLE         MiniportAdapterContext,
    /*IN*/  PNET_BUFFER_LIST    NetBufferList,
    /*IN*/  NDIS_PORT_NUMBER    PortNumber,
    /*IN*/  ULONG               SendFlags
    )
/*++

Routine Description:
    
    MiniportSendNetBufferList handler
    
Arguments:

    MiniportAdapterContext  Pointer to our adapter
    NetBufferList           Pointer to a list of Net Buffer Lists.
    Dispatchlevel           Whether that caller is at DISPATCH_LEVEL or not
    
Return Value:

    None
    
--*/
{
    PMP_ADAPTER         Adapter;
    NDIS_STATUS         Status = NDIS_STATUS_PENDING;
    UINT                NetBufferCount = 0;
    PNET_BUFFER         NetBuffer;
    PNET_BUFFER_LIST    CurrNetBufferList;
    PNET_BUFFER_LIST    NextNetBufferList = NULL;
    KIRQL               OldIrql = 0;
    BOOLEAN             DispatchLevel;
    
    DBGPRINT(MP_TRACE, ("====> MPSendNetBufferLists\n"));
/**

    Go Sushi Truck Go!!!
      _______________
   - |              |__
  -- |  sushi truck | |
      ooo           oo 
		
*/

    Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    DispatchLevel = NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags);

    do
    {
        //
        // If the adapter is in Pausing or paused state, just fail the send.
        //
        if( ( (Adapter->AdapterState == NicPausing) || (Adapter->AdapterState == NicPaused) ) &&
             ( MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) 
               && MP_GET_NET_BUFFER_LIST_TYPE( NetBufferList ) == SEND_BUFFER_ORIGIN_ETHERNET ) )
        {
            Status =  NDIS_STATUS_PAUSED;
            break;
        }

        NICUpdateLinkState( Adapter, DispatchLevel );

        MP_ACQUIRE_SPIN_LOCK(&Adapter->SendLock, DispatchLevel);
        OldIrql = Adapter->SendLock.OldIrql;
    
        //
        // the no-link case
        //
        if ( Adapter->MediaState != MediaConnectStateConnected )
        {
            Status = NDIS_STATUS_NO_CABLE;            
            
            DBGPRINT(MP_TRACE, ("MPSendNetBufferLists: no link, failing send\n"));
            Adapter->SendLock.OldIrql = OldIrql;
            MP_RELEASE_SPIN_LOCK(&Adapter->SendLock, DispatchLevel);
            break;
        }

        for (CurrNetBufferList = NetBufferList;
                 CurrNetBufferList != NULL;
                 CurrNetBufferList = NextNetBufferList)
        {
            //
            // Get how many net buffers inside the net buffer list
            //
            NetBufferCount = 0;
            for (NetBuffer = NET_BUFFER_LIST_FIRST_NB(CurrNetBufferList);
                    NetBuffer != NULL; 
                    NetBuffer = NET_BUFFER_NEXT_NB(NetBuffer))
            {
                NetBufferCount++;
            }
            ASSERT(NetBufferCount > 0);
            MP_GET_NET_BUFFER_LIST_REF_COUNT(CurrNetBufferList) = NetBufferCount;


            if( !Adapter->SendingNetBufferList ) {
                DBGPRINT(MP_TRACE, ("MPSendNetBufferLists: sending top of queue\n"));
                //no net buffers active, send the top of the queue
                Adapter->SendingNetBufferList = CurrNetBufferList;
                NET_BUFFER_LIST_STATUS(CurrNetBufferList) = NDIS_STATUS_SUCCESS;
                MiniportSendNetBufferList(Adapter, CurrNetBufferList, FALSE);

            } else {
                //
                // The first net buffer is the buffer to send
                //
                DBGPRINT(MP_TRACE, ("MPSendNetBufferLists: queue netbuffer\n"));
                MP_GET_NET_BUFFER_LIST_NEXT_SEND(CurrNetBufferList) = NET_BUFFER_LIST_FIRST_NB(CurrNetBufferList);
                NET_BUFFER_LIST_STATUS(CurrNetBufferList) = NDIS_STATUS_SUCCESS;
                InsertTailQueue(&Adapter->SendWaitQueue, 
                            MP_GET_NET_BUFFER_LIST_LINK(CurrNetBufferList));
                Adapter->nWaitSend++;
            }
        }

        Adapter->SendLock.OldIrql = OldIrql;
        MP_RELEASE_SPIN_LOCK(&Adapter->SendLock, DispatchLevel);
    }
    while (FALSE);

    if (Status != NDIS_STATUS_PENDING)
    {
        MPHandleSendNetBufferListsComplete( Adapter, NetBufferList, Status, DispatchLevel );
    }
    DBGPRINT(MP_TRACE, ("<==== MPSendNetBufferLists\n"));
}

VOID
MPSendNetBufferLists(
    /*IN*/  NDIS_HANDLE         MiniportAdapterContext,
    /*IN*/  PNET_BUFFER_LIST    NetBufferList,
    /*IN*/  NDIS_PORT_NUMBER    PortNumber,
    /*IN*/  ULONG               SendFlags
    )
{
    PNET_BUFFER_LIST CurrNetBufferList;
    PNET_BUFFER_LIST NextNetBufferList = NULL;
    
    PMP_ADAPTER Adapter = (PMP_ADAPTER)MiniportAdapterContext;

    for (CurrNetBufferList = NetBufferList;
             CurrNetBufferList != NULL;
             CurrNetBufferList = NextNetBufferList) {

        PMP_NET_BUFFER_LIST_PRIVATE Private = MP_ALLOCMEMTAG( Adapter->AdapterHandle, sizeof( MP_NET_BUFFER_LIST_PRIVATE ) );
        if( Private ) {
            Private->Type = SEND_BUFFER_ORIGIN_ETHERNET;
            Private->RefCount = 0;
            Private->Next = NULL;
            MP_GET_NET_BUFFER_LIST_PRIVATE( NetBufferList ) = (PVOID)Private;
        } else {
            goto end;
        }

        NextNetBufferList = NET_BUFFER_LIST_NEXT_NBL(CurrNetBufferList);
    }

    MPSendNetBufferListsCommon( MiniportAdapterContext, NetBufferList, PortNumber, SendFlags );
end:
    return;
}

VOID MPShutdown(
    /*IN*/  NDIS_HANDLE             MiniportAdapterContext,
    /*IN*/  NDIS_SHUTDOWN_ACTION    ShutdownAction
    )
/*++

Routine Description:
    
    MiniportShutdown handler
    
Arguments:

    MiniportAdapterContext  Pointer to our adapter
    ShutdownAction          The reason for Shutdown

Return Value:

    None
    
--*/
{
    PMP_ADAPTER     Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    NDIS_STATUS     ndis_status = NDIS_STATUS_FAILURE;

    UNREFERENCED_PARAMETER(ShutdownAction);

    ndis_status = NICWriteRegParameters(Adapter);
    if (NT_ERROR(ndis_status)) {
        DBGPRINT( MP_ERROR, ("Failed to NICWriteRegParameters 0x%08x", ndis_status));
    }
    
    DBGPRINT(MP_TRACE, ("====> MPShutdown\n"));

    //
    // Disable interrupt and issue a full reset
    //
    
    MpegDeinitDma( Adapter );

    DBGPRINT(MP_TRACE, ("<==== MPShutdown\n"));
}


BOOLEAN 
MPIsr(
    /*IN*/  NDIS_HANDLE     MiniportInterruptContext,
    /*OUT*/ PBOOLEAN        QueueMiniportInterruptDpcHandler,
    /*OUT*/ PULONG          TargetProcessors)
/*++

Routine Description:
    
    MiniportIsr handler
    
Arguments:

    MiniportInterruptContext: Pointer to the interrupt context.
        In our case, this is a pointer to our adapter structure.
        
    QueueMiniportInterruptDpcHandler: TRUE on return if MiniportHandleInterrupt 
        should be called on default CPU
        
    TargetProcessors: Pointer to a bitmap specifying 
        Target processors which should run the DPC

Return Value:

    TRUE  --- The miniport recognizes the interrupt
    FALSE   --- Otherwise
    
--*/
{
    PMP_ADAPTER  Adapter = (PMP_ADAPTER)MiniportInterruptContext;
    BOOLEAN                         InterruptRecognized = FALSE;
    
    *QueueMiniportInterruptDpcHandler = FALSE;
    
    do 
    {
        
        //
        // If the adapter is in low power state, then it should not 
        // recognize any interrupt
        // 
        if (Adapter->CurrentPowerState > NdisDeviceStateD0)
        {
            DBGPRINT(MP_LOUD, ("CurrentPowerState(%d) > NdisDevieStateD0\n", Adapter->CurrentPowerState));
            InterruptRecognized = FALSE;
            *QueueMiniportInterruptDpcHandler = FALSE;
            break;
        }


        //Check Send (ack)
        if( ctn91xx_read8( Adapter->MsgBase, MSG_BUFFER_MSG_RECV ) ) {

            ctn91xx_write8( 0, Adapter->MsgBase, MSG_BUFFER_INT_ACK_RECV );

            Adapter->MsgBufferSendPending = 1;
            InterruptRecognized = TRUE;
            *QueueMiniportInterruptDpcHandler = TRUE;
        }

        //Check Recv
        if( ctn91xx_read8( Adapter->MsgBase, MSG_BUFFER_MSG_AVAIL ) &&
            ( ctn91xx_read32( Adapter->MsgBase, MSG_BUFFER_TAG ) & 0xff ) == MSG_BUFFER_TAG_ETHERNET ) {

            ctn91xx_write8( 0, Adapter->MsgBase, MSG_BUFFER_INT_ACK_AVAIL );
            Adapter->MsgBufferRecvPending = 1;
            InterruptRecognized = TRUE;
            *QueueMiniportInterruptDpcHandler = TRUE;    
        }

        if( ctn91xx_read8( Adapter->MsgBase, MSG_BUFFER_MSG_AVAIL ) &&
            ( ctn91xx_read32( Adapter->MsgBase, MSG_BUFFER_TAG ) & 0xff ) == MSG_BUFFER_TAG_CONTROL ) {

            ctn91xx_write8( 0, Adapter->MsgBase, MSG_BUFFER_INT_ACK_AVAIL );
            Adapter->MsgBufferControlPending = 1;
            InterruptRecognized = TRUE;
            *QueueMiniportInterruptDpcHandler = TRUE;
        }

        {
            BOOLEAN ShouldQueueDpc = FALSE;
            //Check Mpeg
            if( MpegIsr( Adapter, &ShouldQueueDpc ) ) {
                InterruptRecognized = TRUE;
            }

            if( ShouldQueueDpc ) {
                *QueueMiniportInterruptDpcHandler = TRUE;
            }
        }

    } while (FALSE);    

    return InterruptRecognized;
}


VOID
MPHandleInterrupt(
    /*IN*/  NDIS_HANDLE  MiniportInterruptContext,
    /*IN*/  PVOID        MiniportDpcContext,
    /*IN*/  PULONG       NdisReserved1,
    /*IN*/  PULONG       NdisReserved2
    )
/*++

Routine Description:
    
    MiniportHandleInterrupt handler
    
Arguments:

    MiniportInterruptContext:  Pointer to the interrupt context.
        In our case, this is a pointer to our adapter structure.

Return Value:

    None
    
--*/
{
    PMP_ADAPTER  Adapter = (PMP_ADAPTER)MiniportInterruptContext;
    USHORT       IntStatus;
    int i;

    UNREFERENCED_PARAMETER(MiniportDpcContext);
    UNREFERENCED_PARAMETER(NdisReserved1);
    UNREFERENCED_PARAMETER(NdisReserved2);

    NICUpdateLinkState( Adapter, TRUE );
    //    
    // Handle receive interrupt    
    //
    NdisDprAcquireSpinLock(&Adapter->RcvLock);

    MpHandleRecvInterrupt(Adapter);

    NdisDprReleaseSpinLock(&Adapter->RcvLock);
    
    //
    // Handle send interrupt (also ctrl recv ack)
    //
    NdisDprAcquireSpinLock( &Adapter->ControlLock );
    NdisDprAcquireSpinLock( &Adapter->SendLock );

    MpHandleSendInterrupt(Adapter);

    NdisDprReleaseSpinLock( &Adapter->SendLock );
    NdisDprReleaseSpinLock( &Adapter->ControlLock );

    // 
    // Handle the control msg buffer interrupt
    //
    NdisDprAcquireSpinLock( &Adapter->ControlLock );

    MpHandleControlInterrupt( Adapter );

    NdisDprReleaseSpinLock( &Adapter->ControlLock );


    // 
    // Handle the mpeg interrupts
    //
    MP_ACQUIRE_SPIN_LOCK( &Adapter->MpegLock, TRUE );

    MpegDpc( Adapter );

    MP_RELEASE_SPIN_LOCK( &Adapter->MpegLock, TRUE );
}

VOID 
MPCancelSendNetBufferLists(
    /*IN*/  NDIS_HANDLE     MiniportAdapterContext,
    /*IN*/  PVOID           CancelId
    )
/*++

Routine Description:
    
    MiniportCancelNetBufferLists handler - This function walks through all 
    of the queued send NetBufferLists and cancels all the NetBufferLists that
    have the correct CancelId
    
Arguments:

    MiniportAdapterContext      Pointer to our adapter
    CancelId                    All the Net Buffer Lists with this Id should be cancelled

Return Value:

    None
    
--*/
{
    PMP_ADAPTER         Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    PQUEUE_ENTRY        pEntry, pPrevEntry, pNextEntry;
    PNET_BUFFER_LIST    NetBufferList;
    PNET_BUFFER_LIST    CancelHeadNetBufferList = NULL;
    PNET_BUFFER_LIST    CancelTailNetBufferList = NULL;
    PVOID               NetBufferListId;
    
    DBGPRINT(MP_TRACE, ("====> MPCancelSendNetBufferLists\n"));

    pPrevEntry = NULL;

    NdisAcquireSpinLock(&Adapter->SendLock);

    //
    // Walk through the send wait queue and complete the sends with matching Id
    //
    do
    {

        if (IsQueueEmpty(&Adapter->SendWaitQueue))
        {
            break;
        }
        
        pEntry = GetHeadQueue(&Adapter->SendWaitQueue); 

        while (pEntry != NULL)
        {
            NetBufferList = MP_GET_NET_BUFFER_LIST_FROM_QUEUE_LINK(pEntry);
    
            NetBufferListId = NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(NetBufferList);

            if ((NetBufferListId == CancelId)
                    && (NetBufferList != Adapter->SendingNetBufferList))
            {
                NET_BUFFER_LIST_STATUS(NetBufferList) = NDIS_STATUS_REQUEST_ABORTED;
                Adapter->nWaitSend--;
                //
                // This packet has the right CancelId
                //
                pNextEntry = pEntry->Next;

                if (pPrevEntry == NULL)
                {
                    Adapter->SendWaitQueue.Head = pNextEntry;
                    if (pNextEntry == NULL)
                    {
                        Adapter->SendWaitQueue.Tail = NULL;
                    }
                }
                else
                {
                    pPrevEntry->Next = pNextEntry;
                    if (pNextEntry == NULL)
                    {
                        Adapter->SendWaitQueue.Tail = pPrevEntry;
                    }
                }

                pEntry = pEntry->Next;

                //
                // Queue this NetBufferList for cancellation
                //
                if (CancelHeadNetBufferList == NULL)
                {
                    CancelHeadNetBufferList = NetBufferList;
                    CancelTailNetBufferList = NetBufferList;
                }
                else
                {
                    NET_BUFFER_LIST_NEXT_NBL(CancelTailNetBufferList) = NetBufferList;
                    CancelTailNetBufferList = NetBufferList;
                }
            }
            else
            {
                // This packet doesn't have the right CancelId
                pPrevEntry = pEntry;
                pEntry = pEntry->Next;
            }
        }
    } while (FALSE);
    
    NdisReleaseSpinLock(&Adapter->SendLock);
    
    //
    // Get the packets from SendCancelQueue and complete them if any
    //
    if (CancelHeadNetBufferList != NULL)
    {
        NET_BUFFER_LIST_NEXT_NBL(CancelTailNetBufferList) = NULL;
        MPHandleSendNetBufferListsComplete( Adapter, CancelTailNetBufferList, NDIS_STATUS_REQUEST_ABORTED, FALSE );
    } 

    DBGPRINT(MP_TRACE, ("<==== MPCancelSendNetBufferLists\n"));
    return;

}

VOID 
MPPnPEventNotify(
    /*IN*/  NDIS_HANDLE             MiniportAdapterContext,
    /*IN*/  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
    )
/*++

Routine Description:
    
    MiniportPnPEventNotify handler - NDIS51 and later
    
Arguments:

    MiniportAdapterContext      Pointer to our adapter
    PnPEvent                    Self-explanatory 
    InformationBuffer           Self-explanatory 
    InformationBufferLength     Self-explanatory 

Return Value:

    None
    
--*/
{
    PMP_ADAPTER     Adapter = (PMP_ADAPTER)MiniportAdapterContext;
    NDIS_DEVICE_PNP_EVENT   PnPEvent = NetDevicePnPEvent->DevicePnPEvent;
    PVOID                   InformationBuffer = NetDevicePnPEvent->InformationBuffer;
    ULONG                   InformationBufferLength = NetDevicePnPEvent->InformationBufferLength;

    //
    // Turn off the warings.
    //
    UNREFERENCED_PARAMETER(InformationBuffer);
    UNREFERENCED_PARAMETER(InformationBufferLength);
    UNREFERENCED_PARAMETER(Adapter);
    
    DBGPRINT(MP_TRACE, ("====> MPPnPEventNotify\n"));

    switch (PnPEvent)
    {
        case NdisDevicePnPEventQueryRemoved:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventQueryRemoved\n"));
            break;

        case NdisDevicePnPEventRemoved:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventRemoved\n"));
            break;       

        case NdisDevicePnPEventSurpriseRemoved:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventSurpriseRemoved\n"));
            break;

        case NdisDevicePnPEventQueryStopped:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventQueryStopped\n"));
            break;

        case NdisDevicePnPEventStopped:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventStopped\n"));
            break;      
            
        case NdisDevicePnPEventPowerProfileChanged:
            DBGPRINT(MP_WARN, ("MPPnPEventNotify: NdisDevicePnPEventPowerProfileChanged\n"));
            break;      
            
        default:
            DBGPRINT(MP_ERROR, ("MPPnPEventNotify: unknown PnP event %x \n", PnPEvent));
            break;         
    }

    DBGPRINT(MP_TRACE, ("<==== MPPnPEventNotify\n"));

}


VOID 
MPUnload(
    /*IN*/  PDRIVER_OBJECT  DriverObject
    )
/*++

Routine Description:
    
    The Unload handler
    This handler is registered through NdisMRegisterUnloadHandler
    
Arguments:

    DriverObject        Not used

Return Value:

    None
    
--*/
{
    WdfDriverMiniportUnload(WdfGetDriver());
    //
    // Deregister Miniport driver
    //
    NdisMDeregisterMiniportDriver(NdisMiniportDriverHandle);
}



void ctn91xx_mod_sdump_buffer( const char * module_name, const char * function, int line, const PUCHAR buf, ULONG length, const char* name)
{
	ULONG i, j, i_valid;
	ULONG remainder = (length % 16);
	ULONG top_len = length + (remainder ? 16 - remainder : 0); 
	ULONG char_index;
	UCHAR c;
	
	if (name) {
		DBGPRINT( MP_LOUD, ("%s:%i DUMP: \"%s\" (%p) length %d\n", function, line, name, buf, length));
	} else {
		DBGPRINT( MP_LOUD, ("%s:%i DUMP: (%p) length %d\n", function, line, buf, length));
	}
	
	DBGPRINT( MP_LOUD, ("%08d  ", 0));
    
	for(i=0; i<top_len; i++) {
		
		i_valid = (i < length);
		
		if (i_valid) {
			DBGPRINT( MP_LOUD, ("%02x ", buf[i]));
		} else {
			DBGPRINT( MP_LOUD, ("   "));
		}
		
		if(i % 16 == 15 && i != 0) {
			DBGPRINT( MP_LOUD, ("  |"));
			j = 0;
			for(j=0; j<16; j++) {
				char_index = i - 15 + j;
				if (char_index < length) {
					c = buf[char_index];
					DBGPRINT( MP_LOUD, ("%c", (c >= '0' && c <= 'z') ? c : '.'));
				} else {
					DBGPRINT( MP_LOUD, ("%c", ' '));
				}
			}
            if (i == top_len - 1) {
                DBGPRINT( MP_LOUD, ("|"));
            } else {
                DBGPRINT( MP_LOUD, ("|\n%08d  ", i+1));
            }
            
		} else if (i % 8 == 7) {
			DBGPRINT( MP_LOUD, (" "));
		}
		
		
	}
	DBGPRINT( MP_LOUD, ("\n"));
}


VOID
NICUpdateLinkState( PMP_ADAPTER Adapter, BOOLEAN DispatchLevel )
{
    ULONG Value = 0;
    NDIS_MEDIA_CONNECT_STATE NewState = MediaConnectStateDisconnected;

    Value = ctn91xx_read32( Adapter->MsgBase, MSG_BUFFER_TAG );
    if( Value & MSG_BUFFER_TAG_READY ) {
        NewState = MediaConnectStateConnected;
    }

    if( Adapter->ForceDisconnectCable && 
        ( Adapter->MediaState != MediaConnectStateDisconnected || 
          NewState != MediaConnectStateDisconnected ) ) {
        NewState = MediaConnectStateDisconnected;
    }

    if( NewState != Adapter->MediaState ) {
        MP_ACQUIRE_SPIN_LOCK( &Adapter->Lock, DispatchLevel );
        Adapter->MediaState = NewState;
        MPIndicateLinkState( Adapter, DispatchLevel );
        MP_RELEASE_SPIN_LOCK( &Adapter->Lock, DispatchLevel );
    }
}

