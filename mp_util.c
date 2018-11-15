#include "precomp.h"

LARGE_INTEGER
GetTime(PMP_ADAPTER Adapter)
{
#if USE_CTN91XX_HW_FOR_TIME
	LARGE_INTEGER us;
	ULONGLONG cur_tick = ctn91xx_read64(Adapter->SystemControlBase, SYSTEM_CONTROL_UP_TIME_OFFSET);
	us.QuadPart = (cur_tick / (ULONGLONG)Adapter->TicksPerUS);
	return us;
#else
    LARGE_INTEGER li;
    KeQueryTickCount( &li );
    li.QuadPart *= KeQueryTimeIncrement();
    li.QuadPart /= 10;

    return li;
#endif
}

