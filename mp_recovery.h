#ifndef _MP_RECOVERY_H
#define _MP_RECOVERY_H

#include "mp.h"

NTSTATUS MPHandleLoadRecovery(
        PMP_ADAPTER Adapter,
        PIRP Irp);

NTSTATUS MPHandleTranslationInit(
        PMP_ADAPTER Adapter,
        PIRP Irp);

#endif
