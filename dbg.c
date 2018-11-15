#include "NdisWdm.h"

// Bytes to appear on each line of dump output.
//
#define DUMP_BytesPerLine 16




VOID
DumpLine(
    __in_bcount(cb) __in PUCHAR    p,
    __in ULONG                    cb,  
    __in ULONG                    ulGroup 
    )
{
    CHAR* pszDigits = "0123456789ABCDEF";
    CHAR szHex[ ((2 + 1) * DUMP_BytesPerLine) + 1 ];
    CHAR* pszHex = szHex;
    CHAR szAscii[ DUMP_BytesPerLine + 1 ];
    CHAR* pszAscii = szAscii;
    ULONG ulGrouped = 0;
    PUCHAR start = p;

    while (cb)
    {

#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW, "pszHex accessed is always within bounds");    

        *pszHex++ = pszDigits[ ((UCHAR )*p) / 16 ];
        *pszHex++ = pszDigits[ ((UCHAR )*p) % 16 ];

        if (++ulGrouped >= ulGroup)
        {
            *pszHex++ = ' ';
            ulGrouped = 0;
        }

#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW, "pszAscii is bounded by cb bytes");

		*pszAscii++ = (*p >= 32 && *p < 128) ? *p : '.';

        ++p;
        --cb;
    }

    *pszHex = '\0';
    *pszAscii = '\0';

    DEBUGP(MP_INFO,
        ("%p: %-*s|%-*s|\n",
        start,
        (2 * DUMP_BytesPerLine) + (DUMP_BytesPerLine / ulGroup), szHex,
        DUMP_BytesPerLine, szAscii ));
}

// Hex dump 'cb' bytes starting at 'p' grouping 'ulGroup' bytes together.
// For example, with 'ulGroup' of 1, 2, and 4:
//
// 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 |................|
// 0000 0000 0000 0000 0000 0000 0000 0000 |................|
// 00000000 00000000 00000000 00000000 |................|
//
VOID
Dump(
    __in_bcount(cb) IN PUCHAR    p,
    IN ULONG                    cb,
    IN ULONG                    ulGroup 
    )
{
    INT cbLine;

    while (cb)
    {
        cbLine = (cb < DUMP_BytesPerLine) ? cb : DUMP_BytesPerLine;
#pragma prefast(suppress: __WARNING_POTENTIAL_BUFFER_OVERFLOW, "p is bounded by cb bytes");        
        DumpLine( p, cbLine, ulGroup );
        cb -= cbLine;
        p += cbLine;
    }
}


