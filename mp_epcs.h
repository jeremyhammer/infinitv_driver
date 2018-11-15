#ifndef _MP_EPCS_H
#define _MP_EPCS_H

#include "mp.h"

NTSTATUS
MPHandleFirmwareUpdate(
    IN PMP_ADAPTER Adapter,
    IN PIRP Irp);

NTSTATUS
MPHandleFirmwareUpdateStatus(
    IN PMP_ADAPTER Adapter,
    IN PIRP Irp,
    IN ULONG outlen);


#endif
