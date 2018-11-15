#include "precomp.h"

#define EPCS_WRITE_ENABLE_CMD   0x06
#define EPCS_WRITE_DISABLE_CMD  0x04
#define EPCS_READ_STATUS_CMD    0x05
#define EPCS_READ_BYTES_CMD     0x03
#define EPCS_READ_SIL_ID_CMD    0xAB
#define EPCS_WRITE_STATUS_CMD   0x01
#define EPCS_WRITE_BYTES_CMD    0x02
#define EPCS_ERASE_BULK_CMD     0xC7
#define EPCS_ERASE_SECTOR_CMD   0xD8

#define EPCS_DCLK   4
#define EPCS_OE     5
#define EPCS_NCS    6
#define EPCS_ASDO   7
#define EPCS_DATA0  8

#define EPCS_ENABLE 12
#define EPCS_QUARTER_PERIOD 14
#define EPCS_BYTES_TO_READ 16
#define EPCS_FLOW_CTRL 19
#define     EPCS_FLOW_CTRL_DA 0x1
#define     EPCS_FLOW_CTRL_WD 0x2

#define EPCS_TX_DATA 20
#define EPCS_RX_DATA 24

#define EPCS_STATUS_WIP 0x01
#define EPCS_STATUS_WEL 0x02


typedef struct {
    ULONG32 Status;
    ULONG32 Percent;
} FirmwareUpdateStatus;

typedef struct _CETON_EEPROM_UPDATE_CONTEXT {
    PMP_ADAPTER Adapter;
    ULONG FirmwareLength;
    BYTE Firmware[1];
} CETON_EEPROM_UPDATE_CONTEXT, *PCETON_EEPROM_UPDATE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE(CETON_EEPROM_UPDATE_CONTEXT);

static VOID epcs_transfer_end(PMP_ADAPTER Adapter);
static VOID epcs_transfer_start(PMP_ADAPTER Adapter);
static BOOLEAN epcs_write_enable(PMP_ADAPTER Adapter);
static VOID epcs_write_start(PMP_ADAPTER Adapter, UINT32 addr);
static VOID epcs_write_end(PMP_ADAPTER Adapter);
static BOOLEAN epcs_read_start(PMP_ADAPTER Adapter, UINT32 addr);
static VOID epcs_read_end(PMP_ADAPTER Adapter);
static UINT8 epcs_status(PMP_ADAPTER Adapter);

#if 0
static VOID dump_epcs(PMP_ADAPTER Adapter)
{
    DBGPRINT( MP_WARN, ("CTRL   %02X", ctn91xx_read8(Adapter->EPCSBase, 0)));
    DBGPRINT( MP_WARN, ("DCLK   %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_DCLK)));
    DBGPRINT( MP_WARN, ("OE     %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_OE)));
    DBGPRINT( MP_WARN, ("NCS    %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_NCS)));
    DBGPRINT( MP_WARN, ("ASDO   %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_ASDO)));
    DBGPRINT( MP_WARN, ("DATA0  %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_DATA0)));
    DBGPRINT( MP_WARN, ("ENABLE %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_DATA0)));
    DBGPRINT( MP_WARN, ("Q PER  %d", ctn91xx_read16(Adapter->EPCSBase, EPCS_QUARTER_PERIOD)));
    DBGPRINT( MP_WARN, ("R CNT  %d", ctn91xx_read16(Adapter->EPCSBase, EPCS_BYTES_TO_READ)));
    DBGPRINT( MP_WARN, ("FLOW   %02X", ctn91xx_read8(Adapter->EPCSBase, EPCS_FLOW_CTRL)));
    DBGPRINT( MP_WARN, (""));
}
#endif

/////////////////////////////////////////////////////////////////////////////
// EPCS Bit Bang Transfers
/////////////////////////////////////////////////////////////////////////////

static VOID epcs_transfer_end(PMP_ADAPTER Adapter)
{
    ctn91xx_write8(1, Adapter->EPCSBase, EPCS_NCS);
}

static VOID epcs_transfer_start(PMP_ADAPTER Adapter)
{
    ctn91xx_write8(0, Adapter->EPCSBase, 0);

    ctn91xx_write8(0, Adapter->EPCSBase, EPCS_DCLK);
    ctn91xx_write16(0, Adapter->EPCSBase, EPCS_BYTES_TO_READ);
    ctn91xx_write16(20, Adapter->EPCSBase, EPCS_QUARTER_PERIOD);
    ctn91xx_write8(3, Adapter->EPCSBase, EPCS_ENABLE);

    epcs_transfer_end(Adapter);

    ctn91xx_write8(0, Adapter->EPCSBase, EPCS_NCS);
}

static void ctn91xx_wait(PMP_ADAPTER Adapter, ULONG usec)
{
    LARGE_INTEGER delay;
    delay.QuadPart = RELATIVE(MICROSECONDS(usec));
    KeDelayExecutionThread( KernelMode, FALSE, &delay );
}

static BOOLEAN wait_for_write_complete(PMP_ADAPTER Adapter)
{
    int wait_cnt = 0;
    while(!(ctn91xx_read8(Adapter->EPCSBase, EPCS_FLOW_CTRL) & EPCS_FLOW_CTRL_WD)) {
        wait_cnt++;
        ctn91xx_wait( Adapter, 200 );
        if( wait_cnt > 50 ) {
            DBGPRINT( MP_ERROR, ("timed out waiting for write complete"));
            return FALSE;
        }
    }
    return TRUE;
}

static BOOLEAN wait_for_data_avail(PMP_ADAPTER Adapter)
{
    int wait_cnt = 0;
    while(!(ctn91xx_read8(Adapter->EPCSBase, EPCS_FLOW_CTRL) & EPCS_FLOW_CTRL_DA)) {
        wait_cnt++;
        ctn91xx_wait( Adapter, 200 );
        if( wait_cnt > 50 ) {
            DBGPRINT( MP_ERROR, ("timed out waiting for data avail"));
            return FALSE;
        }
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// EPCS Write Enable
/////////////////////////////////////////////////////////////////////////////

static BOOLEAN epcs_write_enable(PMP_ADAPTER Adapter)
{
    epcs_transfer_start(Adapter);

    ctn91xx_write8(EPCS_WRITE_ENABLE_CMD, Adapter->EPCSBase, EPCS_TX_DATA);
    if( !wait_for_write_complete(Adapter) ) {
        DBGPRINT( MP_ERROR, ("enable cmd failed"));
        return FALSE;
    }

    epcs_transfer_end(Adapter);

    if(!(epcs_status(Adapter) & EPCS_STATUS_WEL)) {
        DBGPRINT( MP_ERROR, ("WRITE ENABLE FAILED!"));
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// EPCS Status and Erase
/////////////////////////////////////////////////////////////////////////////

UINT8 epcs_status(PMP_ADAPTER Adapter)
{
    UINT8 status = 0;

    epcs_transfer_start(Adapter);

    ctn91xx_write8(EPCS_READ_STATUS_CMD, Adapter->EPCSBase, EPCS_TX_DATA);
    if( !wait_for_write_complete(Adapter) ) {
        return status;
    }
    ctn91xx_write16(1, Adapter->EPCSBase, EPCS_BYTES_TO_READ);
    if( !wait_for_data_avail(Adapter) ) {
        return status;
    }
    status = ctn91xx_read8(Adapter->EPCSBase, EPCS_RX_DATA);

    epcs_transfer_end(Adapter);

    return status;
}

BOOLEAN epcs_bulk_erase(PMP_ADAPTER Adapter)
{
    int i = 0;
    static const int erase_timeout = 180;
    
    Adapter->EEPROM_SubprocessPercent = 0;

    if( !epcs_write_enable(Adapter) ) {
        DBGPRINT( MP_ERROR, ("write enable failed"));
        return FALSE;
    }

    epcs_transfer_start(Adapter);

    ctn91xx_write8(EPCS_ERASE_BULK_CMD, Adapter->EPCSBase, EPCS_TX_DATA);
    if( !wait_for_write_complete(Adapter) ) {
        DBGPRINT( MP_ERROR, ("erase bulk cmd failed"));
        return FALSE;
    }

    epcs_transfer_end(Adapter);

    while(i < erase_timeout) {

        ctn91xx_wait( Adapter, 1000 * 1000 );

        if(!(epcs_status(Adapter) & EPCS_STATUS_WIP)) {
            DBGPRINT( MP_WARN, ("EEPROM Erased"));
            break;
        }
        
        Adapter->EEPROM_SubprocessPercent = (i * 100) / erase_timeout;

        i++;
    }
    
    Adapter->EEPROM_SubprocessPercent = 100;

    if(i == 180) {
        DBGPRINT( MP_ERROR, ("Timed out erasing EEPROM"));
        return FALSE;
    }

    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// EPCS Write
/////////////////////////////////////////////////////////////////////////////

static VOID epcs_write_start(PMP_ADAPTER Adapter, UINT32 addr)
{
    epcs_transfer_start(Adapter);

    ctn91xx_write8(EPCS_WRITE_BYTES_CMD, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8((addr >> 16) & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8((addr >> 8) & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8(addr & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
}

static VOID epcs_write_end(PMP_ADAPTER Adapter)
{
    epcs_transfer_end(Adapter);
}

BOOLEAN epcs_write_bytes(PMP_ADAPTER Adapter, UINT32 addr, UINT8* bytes, UINT32 length)
{
    int i = 0;
    int bytes_left = length;
    int bytes_to_transfer = 0;
    int wait_cnt = 0;
    int offset = 0;
    int chunk = 0;
    
    Adapter->EEPROM_SubprocessPercent = 0;

    while(bytes_left > 0) {
        bytes_to_transfer = MIN(256, bytes_left);

        if( !epcs_write_enable(Adapter) ) {
            return FALSE;
        }

        epcs_write_start(Adapter, addr);
        
        {
            for(i=0;i<bytes_to_transfer;i++) {
                ctn91xx_write8(bytes[offset+i], Adapter->EPCSBase, EPCS_TX_DATA);

                if(length > 10) {
                    if((offset+i) % (length/10) == 0) {
                        //DBGPRINT( MP_WARN, ("%d%% complete", (int)((((double)((offset+i)+1))/((double)length))*100.0)));
                        DBGPRINT( MP_WARN, ("%d%% complete", 
                            ((offset+i)+1)*100/length
                            ));
                    }
                }
            }
        }

        if( !wait_for_write_complete(Adapter) ) {
            return FALSE;
        }

        epcs_write_end(Adapter);

        wait_cnt = 0;
        while(epcs_status(Adapter) & EPCS_STATUS_WIP) {
            ctn91xx_wait(Adapter, 500);
            wait_cnt++;
            if(wait_cnt >= 12) {
                DBGPRINT( MP_ERROR, ("Timed out waiting for write to complete"));
                return FALSE;
            }
        }
        
        Adapter->EEPROM_SubprocessPercent = (offset * 100) / length;
        
        bytes_left -= bytes_to_transfer;
        addr += bytes_to_transfer;
        offset += bytes_to_transfer;
        chunk++;
    }
    
    Adapter->EEPROM_SubprocessPercent = 100;
    
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// EPCS Read
/////////////////////////////////////////////////////////////////////////////

static BOOLEAN epcs_read_start(PMP_ADAPTER Adapter, UINT32 addr)
{
    epcs_transfer_start(Adapter);

    ctn91xx_write8(EPCS_READ_BYTES_CMD, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8((addr >> 16) & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8((addr >> 8) & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
    ctn91xx_write8(addr & 0xFF, Adapter->EPCSBase, EPCS_TX_DATA);
    if( !wait_for_write_complete(Adapter) ) {
        return FALSE;
    }
    return TRUE;
}

static VOID epcs_read_end(PMP_ADAPTER Adapter)
{
    epcs_transfer_end(Adapter);
}

VOID epcs_check_dclk(PMP_ADAPTER Adapter)
{
    ctn91xx_write8(0, Adapter->EPCSBase, EPCS_DCLK);
    while(!ctn91xx_read8(Adapter->EPCSBase, EPCS_DCLK)) {
        ctn91xx_write8(1, Adapter->EPCSBase, EPCS_DCLK);
        ctn91xx_write8(0, Adapter->EPCSBase, EPCS_DCLK);
    }

    DBGPRINT( MP_ERROR, ("wtf"));
}

BOOLEAN epcs_compare_bytes(PMP_ADAPTER Adapter, UINT32 addr, UINT8* bytes, UINT32 length)
{
    UINT32 i = 0;
    UINT8 b = 0;

    if(!bytes || length <= 0) {
        DBGPRINT( MP_ERROR, ("invalid buffer"));
        return FALSE;
    }

    if( !epcs_read_start(Adapter, addr) ) {
        return FALSE;
    }
    
    Adapter->EEPROM_SubprocessPercent = 0;

    for(i=0;i<length;i++) {

        if(i % 0x8000 == 0) {
            ctn91xx_write16((USHORT)MIN(0x8000, length-i), Adapter->EPCSBase, EPCS_BYTES_TO_READ);
        }

        if(i % (length/10) == 0) {
            DBGPRINT( MP_WARN, ("%d%% complete", 
                (i+1)*100/length
                ));
        }

        if( !wait_for_data_avail(Adapter) ) {
            return FALSE;
        }
        b = ctn91xx_read8(Adapter->EPCSBase, EPCS_RX_DATA);

        if(bytes[i] != b) {

            DBGPRINT( MP_ERROR, ("Contents do not match at index %d (%02X != %02X)", i, bytes[i], b));
            break;
        }
        
        Adapter->EEPROM_SubprocessPercent = (i * 100) / length;
    }

    if(i == length) {
        DBGPRINT( MP_WARN, ("Contents match"));
    }

    epcs_read_end(Adapter);
    
    Adapter->EEPROM_SubprocessPercent = 100;

    return (i == length);
}

BOOLEAN EPCSUpdate(PMP_ADAPTER Adapter, UINT8* Firmware, UINT32 FirmwareLength)
{
    UINT32 i;
    int num_retries_left = 2;
    
    Adapter->EEPROM_UpdatePercent = 0;
    Adapter->EEPROM_SubprocessTotal = 10;
    
retry:
    Adapter->EEPROM_UpdateProgress = FIRMWARE_UPDATE_ERASING;
    Adapter->EEPROM_UpdatePercent = 10;
    Adapter->EEPROM_SubprocessTotal = 20;
    
    DBGPRINT( MP_WARN, ("Erasing EEPROM"));
    if( !epcs_bulk_erase(Adapter) ) {
        if( num_retries_left > 0 ) {
            num_retries_left--;
            goto retry;
        } else {
            return FALSE;
        }
    }
    
    Adapter->EEPROM_UpdateProgress = FIRMWARE_UPDATE_WRITING;
    Adapter->EEPROM_UpdatePercent = 30;
    Adapter->EEPROM_SubprocessTotal = 50;

    DBGPRINT( MP_WARN, ("Writing contents to EEPROM"));
    if( !epcs_write_bytes(Adapter, 0, Firmware, FirmwareLength) ) {
        if( num_retries_left > 0 ) {
            num_retries_left--;
            goto retry;
        } else {
            return FALSE;
        }
    }
    
    Adapter->EEPROM_UpdateProgress = FIRMWARE_UPDATE_NOT_RUNNING;
    Adapter->EEPROM_UpdatePercent = 100;

    return TRUE;
}



static void
MPDoFirmwareUpdateCallback(
    IN WDFWORKITEM WorkItem)
{
    PCETON_EEPROM_UPDATE_CONTEXT ceuc = WdfObjectGet_CETON_EEPROM_UPDATE_CONTEXT(WorkItem);
    PMP_ADAPTER Adapter = ceuc->Adapter;
    ULONG FirmwareLength = ceuc->FirmwareLength;
    BYTE * Firmware = ceuc->Firmware;
    
    DBGPRINT( MP_ERROR, ("Work item started - Adapter: %p FirmwareLength %d Firmware %p\n",
        Adapter,
        FirmwareLength,
        Firmware));
    
    if(!EPCSUpdate(Adapter, Firmware, FirmwareLength)) {
        DBGPRINT( MP_ERROR, ("Failed to update EPCS firmware"));
    }
    
    Adapter->EEPROM_UpdateInProgress = FALSE;
}

NTSTATUS
MPHandleFirmwareUpdate(
    IN PMP_ADAPTER Adapter,
    IN PIRP Irp)
{
    NTSTATUS  status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    UINT8 * InputFirmware = (UINT8*)Irp->AssociatedIrp.SystemBuffer;
    ULONG inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    PCETON_EEPROM_UPDATE_CONTEXT ceuc = NULL;
    WDF_OBJECT_ATTRIBUTES  attributes;
    WDF_WORKITEM_CONFIG  workitemConfig;

    //TODO: put under lock
    if (Adapter->EEPROM_UpdateInProgress) {
        DBGPRINT( MP_WARN, ("Update already in progress\n"));
        status = STATUS_INVALID_PARAMETER;
        return status;
    }
    Adapter->EEPROM_UpdateInProgress = TRUE;
    Adapter->EEPROM_UpdateProgress = FIRMWARE_UPDATE_INIT;
    Adapter->EEPROM_UpdatePercent = 0;
    
    if (!InputFirmware) {
        DBGPRINT( MP_WARN, ("Invalid IOCTL input\n"));
        status = STATUS_INVALID_PARAMETER;
        goto error;
    }
    
    if (inlen < 1024) {
        DBGPRINT( MP_WARN, ("Invalid IOCTL length %d\n", inlen));
        status = STATUS_INVALID_PARAMETER;
        goto error;
    }

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &attributes,
            CETON_EEPROM_UPDATE_CONTEXT
            );
    attributes.ContextSizeOverride =
        sizeof(CETON_EEPROM_UPDATE_CONTEXT) + inlen - 1;
    attributes.ParentObject = Adapter->WdfDevice;

    WDF_WORKITEM_CONFIG_INIT(
            &workitemConfig,
            MPDoFirmwareUpdateCallback
            );

    status = WdfWorkItemCreate(
            &workitemConfig,
            &attributes,
            &Adapter->EEPROM_WorkItem
            );

    if (!NT_SUCCESS(status)) {
        DBGPRINT( MP_ERROR, ("Failed to create work item %.08x\n", status));
        goto error;
    }

    ceuc = WdfObjectGet_CETON_EEPROM_UPDATE_CONTEXT(Adapter->EEPROM_WorkItem);
    if (!ceuc) {
        DBGPRINT( MP_ERROR, ("Failed to get work item context\n"));
        status = STATUS_INVALID_PARAMETER;
        goto error;
    }

    ceuc->Adapter = Adapter;
    ceuc->FirmwareLength = inlen;
    NdisMoveMemory(ceuc->Firmware, InputFirmware, inlen);

    WdfWorkItemEnqueue(
            Adapter->EEPROM_WorkItem);
    DBGPRINT( MP_INFO, ("Work item queued\n"));
    return STATUS_SUCCESS;

error:
    Adapter->EEPROM_UpdateInProgress = FALSE;
    return status;
}


NTSTATUS
MPHandleFirmwareUpdateStatus(
    IN PMP_ADAPTER Adapter,
    IN PIRP Irp,
    ULONG outlen)
{
    NTSTATUS  status = STATUS_SUCCESS;
    FirmwareUpdateStatus* output = Irp->AssociatedIrp.SystemBuffer;

    DBGPRINT( MP_TRACE, ("IOCTL_CETON_FIRMWARE_UPDATE_STATUS\n"));

    if (outlen < sizeof(FirmwareUpdateStatus))
    {
        DBGPRINT( MP_WARN, ("Invalid IOCTL length (%d/%d)\n",  outlen, sizeof(FirmwareUpdateStatus)));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    if (!Adapter->EEPROM_UpdateInProgress) {
        output->Status = FIRMWARE_UPDATE_NOT_RUNNING;
        output->Percent = 0;
        goto end;
    }

    output->Status = Adapter->EEPROM_UpdateProgress;
    output->Percent = Adapter->EEPROM_UpdatePercent
        + ((Adapter->EEPROM_SubprocessTotal * Adapter->EEPROM_SubprocessPercent) / 100);
    DBGPRINT( MP_WARN, ("Progress %d %% (step %d/percent %d %%/sub total %d %%/sub percent %d %%)\n", 
                output->Percent,
                Adapter->EEPROM_UpdateProgress,
                Adapter->EEPROM_UpdatePercent,
                Adapter->EEPROM_SubprocessTotal,
                Adapter->EEPROM_SubprocessPercent));

end:
    return status;
}
                        
