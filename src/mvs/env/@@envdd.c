/* @@ENVDD.C - the SYSENV environment, read without the stream functions
**
** __envlz() loads DD SYSENV (or ENVIRON) the first time a program uses the
** environment.  It did so through loadenv() - fopen() and fgets() - which
** put the stream layer and printf() into every program that reads one
** variable, every program using a time function included (size analysis
** F3).  This reads the DD record by record through the BSAM layer and
** hands each line to __envln(), as loadenv() does.
**
** What fopen("dd:...", "r") did for this open is done here too:
** - a DD that is not allocated, or that OPEN refuses, answers nonzero, so
**   the caller goes on to ENVIRON;
** - a spool SYSIN another stream holds open is not opened a second time,
**   which JES2 answers with ABEND S013-C0 (#184);
** - a DUMMY is opened with 80-byte records, not with 32760-byte buffers
**   (#277);
** - trailing X'00' bytes of a record are dropped, as on a text stream.
** A record longer than 1023 bytes is cut there; fgets() handed the rest
** on as a line of its own.  A DD that is a TSO terminal is not read:
** there is no prompting for environment lines at the first getenv().
*/
#include <string.h>
#include "src/internal/fileio.h"
#include "src/internal/bsam.h"
#include "src/internal/envld.h"
#include "ibm/mvs/dcbd.h"

int
__envdd(const char *ddname)
{
    FILE            probe;
    DCB             *dcb;
    unsigned char   *dptr;
    size_t          lenread;
    size_t          len;
    int             mode    = 0;    /* input */
    int             recfm   = 0;
    int             lrecl   = 0;
    int             blksize = 0;
    void            *asmbuf = 0;
    int             i;
    char            buf[1024];

    /* #184: __ddbusy() looks at the DD name and the direction only, and
       compares the name as fopen() keeps it: no blanks, zero filled */
    memset(&probe, 0, sizeof(probe));
    for (i = 0; i < 8 && ddname[i] > ' '; i++) probe.ddname[i] = ddname[i];
    probe.flags = _FILE_FLAG_READ;
    if (__ddbusy(&probe)) return 1;

    if (__dddum(ddname)) {          /* #277 */
        lrecl   = 80;
        blksize = 80;
    }

    dcb = __aopen(ddname, &mode, &recfm, &lrecl, &blksize, &asmbuf, 0);
    if ((int)dcb <= 0) return 1;

    if (dcb->dcbdevt == DCBDVTRM) goto done;

    while (__aread(dcb, &dptr, &lenread) == 0) {
        switch (dcb->dcbrecfm & _FILE_RECFM_TYPE) {
        case _FILE_RECFM_F:
            len = dcb->dcblrecl;
            break;
        case _FILE_RECFM_V:
            len = (dptr[0] << 8) | dptr[1];
            len = len > 4 ? len - 4 : 0;
            dptr += 4;              /* skip over the RDW */
            break;
        default:
            len = lenread;
            break;
        }
        while (len > 0 && dptr[len - 1] == '\0') len--;
        if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
        memcpy(buf, dptr, len);
        buf[len] = 0;
        __envln(buf);
    }

done:
    __aclose(dcb);
    return 0;
}
