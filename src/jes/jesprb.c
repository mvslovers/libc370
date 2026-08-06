/* JESPRB.C - walk the print records of one JES2 spool block.
 *
 * Extracted from jesprint.c (#25) so the record walk can be linked and
 * executed by a host test (test/host/tstjesprb.c).  Buffer in, lines out: no
 * I/O, no EX/TR translate, no libc370 header - see jesprb.h.
 *
 * This is a move, not a rewrite: the walk below is jesprint.c's inner loop
 * with the state that outlives one block (prbuf/blksize/linelen and the line
 * count) lifted into JESPRB.  The two memory-safety defects it contains are
 * known and deliberately left in place here - #23 (the bounds test is
 * evaluated after the dereference) and #24 (the memcpy into prbuf is
 * unchecked); they get their red->green tests and their fix in their own
 * change, against this now-testable code.
 */
#include <stdlib.h>
#include <string.h>

#include "jesprb.h"

int
__jesprb(char *blk, unsigned blklen, JESPRB *pb,
         int (*emit)(char *line, unsigned linelen, void *arg), void *arg)
{
    char        *p;
    char        *eob;
    PRLINE      *line;
    int         rc;

    pb->reason = JESPRB_OK;

    eob = &blk[blklen - sizeof(PRLINE)];

    for(p=&blk[10], line=(PRLINE*)p; line->len != EOB && p < eob; line=(PRLINE*)p) {
        if (line->flags & FLAG_SPAN) {
            SPLINE *sp = (SPLINE*)line;

            p = (char*)sp->data;
            if (sp->flags & FLAG_FIRST) {
                unsigned newblock = *(unsigned short *)p;
                p += 2;

                if (newblock > pb->blksize) {
                    if (pb->prbuf) free(pb->prbuf);
                    pb->blksize = newblock;
                    pb->prbuf = calloc(1, pb->blksize + 4);
                    if (!pb->prbuf) {
                        pb->reason = JESPRB_NOMEM;
                        return -1;
                    }
                }

                pb->linelen = 0;
                if (sp->flags & FLAG_HASCC) p++;
            }

            /* a MIDDLE/LAST part with no FIRST part before it: there is
               no buffer to reassemble into, so the rest of this block is
               skipped.  Report it - silently dropping records is what
               #21 is about.  The first block after a foreign read is
               exactly how this happens (see #24).                       */
            if (!pb->prbuf) {
                pb->reason = JESPRB_NOBUF;
                return 0;
            }

            memcpy(&pb->prbuf[pb->linelen], p, sp->len2);
            pb->linelen += sp->len2;

            if (sp->flags & FLAG_LAST || pb->linelen > pb->blksize) {
                if (pb->linelen) pb->lines++;
                rc = emit(pb->prbuf, pb->linelen, arg);
                if (rc < 0) {
                    pb->reason = JESPRB_STOPPED;
                    pb->prtrc  = rc;
                    return -1;
                }
                pb->linelen = 0;
            }

            p+=sp->len2;
            continue;
        }

        p = (char*)line->data;
        if (line->flags & FLAG_HASCC) p++;  /* skip over carriage control character */

        if (line->len) pb->lines++;
        rc = emit(p, line->len, arg);
        if (rc < 0) {
            pb->reason = JESPRB_STOPPED;
            pb->prtrc  = rc;
            return -1;
        }
        p+=line->len;
    }

    return 0;
}
