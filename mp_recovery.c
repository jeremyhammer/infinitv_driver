#include "precomp.h"


NTSTATUS MPHandleTranslationInit(
        PMP_ADAPTER Adapter,
        PIRP Irp)
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG mask, base;

    Adapter->TranslationMask = 0x0000ffff; //64k window
    Adapter->TranslationBase = 0x70000000;

    if (!Adapter->TranslationMap) {
        DBGPRINT(MP_WARN, ("No Translation IO Map"));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    ctn91xx_write32(Adapter->TranslationMask, Adapter->SystemControlBase, SYSTEM_CONTROL_EXT_TRANSLATION_MASK);
    ctn91xx_write32(Adapter->TranslationBase, Adapter->SystemControlBase, SYSTEM_CONTROL_EXT_TRANSLATION_VALUE);

    mask = ctn91xx_read32(Adapter->SystemControlBase, SYSTEM_CONTROL_EXT_TRANSLATION_MASK);
    base = ctn91xx_read32(Adapter->SystemControlBase, SYSTEM_CONTROL_EXT_TRANSLATION_VALUE);

    if (mask != Adapter->TranslationMask || base != Adapter->TranslationBase) {
        DBGPRINT( MP_WARN, ("%08x != %08x || %08x != %08x", mask, Adapter->TranslationMask, base, Adapter->TranslationBase));
        status = STATUS_ADAPTER_HARDWARE_ERROR;
        goto end;
    }

end:
    return status;
}

static void
SetTranslation(PMP_ADAPTER Adapter, ULONG addr)
{
    Adapter->TranslationBase = addr & ~Adapter->TranslationMask;
    ctn91xx_write32(Adapter->TranslationBase, Adapter->SystemControlBase, SYSTEM_CONTROL_EXT_TRANSLATION_VALUE);
}

static void
TranslatedWrite32(PMP_ADAPTER Adapter, UINT32 word, ULONG offset)
{
    *(UINT32*)(Adapter->TranslationMap + (offset - Adapter->TranslationBase)) = word;
}

static void
TranslatedMemcpyTo(PMP_ADAPTER Adapter,
        ULONG dst,
        UINT32* src,
        ULONG len)
{
    ULONG i;

    //TODO: make faster. implement without set translation everytime and use
    //memcpy. watch out for the endians!

    for (i=0; i<len/4; i++) {
        ULONG new_addr = (dst + (i*4));
        SetTranslation(Adapter, new_addr);
        TranslatedWrite32(Adapter, src[i], new_addr);
    }
}

NTSTATUS MPHandleLoadRecovery(
        PMP_ADAPTER Adapter,
        PIRP Irp)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    UINT8 * InputFirmware = (UINT8*)Irp->AssociatedIrp.SystemBuffer;
    ULONG inlen = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG dst = 0x40000000;

    if (!InputFirmware) {
        DBGPRINT( MP_WARN, ("Invalid IOCTL input\n"));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    if (inlen < 1024) {
        DBGPRINT( MP_WARN, ("Invalid IOCTL length %d\n", inlen));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    TranslatedMemcpyTo(Adapter, dst, (UINT32*)InputFirmware, inlen);

    //patch our 'trampoline' to start executing out of ddr
    ctn91xx_write32(dst, Adapter->SystemControlBase, SYSTEM_CONTROL_PROC_DMA_RESET_ADDR_OFFSET);

    //pull proc out of reset
    ctn91xx_write8(0, Adapter->SystemControlBase, SYSTEM_CONTROL_PROC_RST_HOLD);


end:
    return status;
}
