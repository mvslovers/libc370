/* @@ENVLN.C - one line of a SYSENV-style data set, for loadenv() and the
** start-up's SYSENV reader (@@envdd.c).
*/
#include <string.h>
#include <stdlib.h>
#include "mvs/env.h"
#include "src/internal/envld.h"

static int numbered(const char *buf, int *len);

void
__envln(char *buf)
{
    int     len;

    if (buf[0]=='*') return;        /* skip MVS style "* comments"          */
    if (buf[0]=='#') return;        /* skip Unix style "# comments"         */
    if (numbered(buf, &len)) {
        /* remove sequence numbers from end of buf */
        buf[len-8] = 0;
        len -= 8;
    }
    /* remove any trailing spaces */
    for(len--; len > 0 && buf[len]==' '; len--) buf[len] = 0;

    if (strchr(buf, '=')) {         /* do we have "name = value"?           */
        putenv(buf);                /* yes, set environment variable        */
    }
}

__asm__("\n&FUNC    SETC 'numbered'");
static int
numbered(const char *buf, int *len)
{
    int         rc  = 0;
    const char  *p;

    *len = strlen(buf);
    if (*len <= 8) goto quit;

    /* examine last 8 character of buf for sequnence number */
    for(p = &buf[(*len)-8]; *p; p++) {
        if (*p >= '0' && *p <= '9') continue;   /* sequence number  */
        goto quit;                              /* no sequence      */
    }

    rc = 1; /* buf looks like it has sequence numbers in last 8 characters */

quit:
    return rc;
}
