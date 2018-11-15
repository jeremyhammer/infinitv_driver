/*++

Copyright (c) 1999  Microsoft Corporation

Module Name:
    mp_def.h

Abstract:
    NIC specific definitions

Revision History:

Notes:

--*/

#ifndef _MP_DEF_H
#define _MP_DEF_H

// memory tag for this driver   
#define NIC_TAG                         ((ULONG)'DRAC')
#define NIC_DBG_STRING                  ("**CTN91XX**") 

// packet and header sizes
#define NIC_MAX_PACKET_SIZE             1514
#define NIC_MIN_PACKET_SIZE             60
#define NIC_HEADER_SIZE                 14

// multicast list size                          
#define NIC_MAX_MCAST_LIST              32

// update the driver version number every time you release a new driver
// The high word is the major version. The low word is the minor version.
// let's start with 6.0 for NDIS 6.0 driver
#define NIC_MAJOR_DRIVER_VERSION        0x01
#define NIC_MINOR_DRIVER_VERISON        0x01

// update the driver version number every time you release a new driver
// The high word is the major version. The low word is the minor version.
// this should be the same as the version reported in miniport driver characteristics

#define NIC_VENDOR_DRIVER_VERSION       ((NIC_MAJOR_DRIVER_VERSION << 16) | NIC_MINOR_DRIVER_VERISON)

// NDIS version in use by the NIC driver. 
// The high byte is the major version. The low byte is the minor version. 
#define NIC_DRIVER_VERSION              0x0601


// media type, we use ethernet, change if necessary
#define NIC_MEDIA_TYPE                  NdisMedium802_3


//
// maximum link speed for send dna recv in bps
//
#define NIC_MEDIA_MAX_SPEED             100000000

// interface type, we use PCI
#define NIC_INTERFACE_TYPE              NdisInterfacePci
#define NIC_INTERRUPT_MODE              NdisInterruptLevelSensitive 

// NIC PCI Device and vendor IDs 
#define NIC_PCI_DEVICE_ID               CTN91XX_DEVICE_ID
#define NIC_PCI_VENDOR_ID               CETON_VENDOR_ID

// buffer size passed in NdisMQueryAdapterResources                            
// We should only need three adapter resources (IO, interrupt and memory),
// Some devices get extra resources, so have room for 10 resources 
#define NIC_RESOURCE_BUF_SIZE           (sizeof(NDIS_RESOURCE_LIST) + \
                                        (10*sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))

// PCS config space including the Device Specific part of it/
#define NIC_PCI_HDR_LENGTH         0xe2

#define NIC_VENDOR_DESC                 "Ceton"

// number of TCBs per processor - min, default and max
#define NIC_MIN_TCBS                    1
#define NIC_DEF_TCBS                    32
#define NIC_MAX_TCBS                    64

// max number of physical fragments supported per TCB
#define NIC_MAX_PHYS_BUF_COUNT          8     

// number of RFDs - min, default and max
#define NIC_MIN_RFDS                    4
#define NIC_DEF_RFDS                    512
#define NIC_MAX_RFDS                    1024

// only grow the RFDs up to this number
#define NIC_MAX_GROW_RFDS               1024

// How many intervals before the RFD list is shrinked?
#define NIC_RFD_SHRINK_THRESHOLD        10

// local data buffer size (to copy send packet data into a local buffer)
#define NIC_BUFFER_SIZE(Adapter)        max( (RTP_SINK_BUFFER_SIZE(Adapter) + 14 + 20 + 8), 1520 )

// max number of send packets the MiniportSendPackets function can accept                            
#define NIC_MAX_SEND_PACKETS            10

// supported filters
#define NIC_SUPPORTED_FILTERS (     \
    NDIS_PACKET_TYPE_DIRECTED       | \
    NDIS_PACKET_TYPE_MULTICAST      | \
    NDIS_PACKET_TYPE_BROADCAST      | \
    NDIS_PACKET_TYPE_PROMISCUOUS    | \
    NDIS_PACKET_TYPE_ALL_MULTICAST)

// Threshold for a remove 
#define NIC_HARDWARE_ERROR_THRESHOLD    5

// The CheckForHang intervals before we decide the send is stuck
#define NIC_SEND_HANG_THRESHOLD         5        

// NDIS_ERROR_CODE_ADAPTER_NOT_FOUND                                                     
#define ERRLOG_READ_PCI_SLOT_FAILED     0x00000101L
#define ERRLOG_WRITE_PCI_SLOT_FAILED    0x00000102L
#define ERRLOG_VENDOR_DEVICE_NOMATCH    0x00000103L

// NDIS_ERROR_CODE_ADAPTER_DISABLED
#define ERRLOG_BUS_MASTER_DISABLED      0x00000201L

// NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION
#define ERRLOG_INVALID_SPEED_DUPLEX     0x00000301L
#define ERRLOG_SET_SECONDARY_FAILED     0x00000302L

// NDIS_ERROR_CODE_OUT_OF_RESOURCES
#define ERRLOG_OUT_OF_MEMORY            0x00000401L
#define ERRLOG_OUT_OF_SHARED_MEMORY     0x00000402L
#define ERRLOG_OUT_OF_BUFFER_POOL       0x00000404L
#define ERRLOG_OUT_OF_NDIS_BUFFER       0x00000405L
#define ERRLOG_OUT_OF_PACKET_POOL       0x00000406L
#define ERRLOG_OUT_OF_NDIS_PACKET       0x00000407L
#define ERRLOG_OUT_OF_LOOKASIDE_MEMORY  0x00000408L
#define ERRLOG_OUT_OF_SG_RESOURCES      0x00000409L

// NDIS_ERROR_CODE_HARDWARE_FAILURE
#define ERRLOG_SELFTEST_FAILED          0x00000501L
#define ERRLOG_INITIALIZE_ADAPTER       0x00000502L
#define ERRLOG_REMOVE_MINIPORT          0x00000503L

// NDIS_ERROR_CODE_RESOURCE_CONFLICT
#define ERRLOG_MAP_IO_SPACE             0x00000601L
#define ERRLOG_QUERY_ADAPTER_RESOURCES  0x00000602L
#define ERRLOG_NO_IO_RESOURCE           0x00000603L
#define ERRLOG_NO_INTERRUPT_RESOURCE    0x00000604L
#define ERRLOG_NO_MEMORY_RESOURCE       0x00000605L

// Constants for various purposes of NdisStallExecution

#define NIC_DELAY_POST_RESET            20
// Wait 5 milliseconds for the self-test to complete
#define NIC_DELAY_POST_SELF_TEST_MS     5
                                      
// delay used for link detection to minimize the init time
// change this value to match your hardware 

//10000 system time units (100-nanosecond intervals) = 1 milliseconds
#define NIC_LINK_DETECTION_DELAY        -1000000 //-1*(100*10000) 

#define DEFAULT_TX_FIFO_LIMIT   8
#define DEFAULT_RX_FIFO_LIMIT   8
#define DEFAULT_UNDERRUN_RETRY  1
#define MIN_NUM_RFD 4

#endif  // _MP_DEF_H