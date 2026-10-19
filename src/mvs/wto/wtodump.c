/* WTODUMP.C */
#define CLIB_C
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "mvs/crt.h"
#include "mvs/wto.h"
#include "stddef.h"
#include "ibm/mvs/ihacde.h"

void
wtodump(const char *title, void *varea, int size, int chunk)
{
    int             indent  = 0;
    int             i, j;
    int             iHex    = 0;
    int             ie      = 0;
    int             x;
    unsigned char   *area   = (unsigned char*)varea;
    unsigned char   e;
    char            sHex[80];
    char            eChar[80];

    /* 1 to 32 bytes a line: sHex holds 2 characters a byte and a blank
       every 4, eChar one a byte.  A chunk above 35 ran past sHex, and one
       of 0 or less never ended a line and ran past both (#404) */
    if (chunk <= 0) chunk = 16;
    if (chunk > 32) chunk = 32;
    /* the hex part: 2 characters a byte and a blank between groups of 4.
       (chunk / 4) - 1 blanks cut the last digit off a chunk that is not a
       multiple of 4 (#404) */
    x = (chunk * 2) + ((chunk - 1) / 4);

    wtof("%*.*sDump of %08X \"%s\" (%d bytes)\n",
        indent, indent, "",
        area, title, size);

    for (i=0, j=0; size > 0;i++ ) {
        if ( i==chunk ) {
            wtof("%*.*s+%05X %-*.*s :%-*.*s:\n",
                indent, indent, "", j, x, x, sHex, chunk, chunk, eChar);
            j += i;
            ie = iHex = i = 0;
        }
        iHex += sprintf(&sHex[iHex],"%02X",*area);
        if ((i & 3) == 3) iHex += sprintf(&sHex[iHex]," ");

        e = *area;

        ie += sprintf(&eChar[ie],"%c", isgraph(e)?e:e==' '?e:'.');
        area++;
        size--;
    }

    if ( iHex ) {
        wtof("%*.*s+%05X %-*.*s :%-*.*s:\n",
            indent, indent, "", j, x, x, sHex, chunk, chunk, eChar);
    }

    return;
}
