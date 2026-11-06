/* JESPRB.C - walk the print records of one JES2 spool block.
 *
 * Extracted from jesprint.c (#25) so the record walk can be linked and
 * executed by a host test (test/host/tstjesprb.c).  Buffer in, lines out: no
 * I/O, no EX/TR translate, no libc370 header - see jesprb.h.
 *
 * Nothing in a block is trusted.  The block is read from a spool track that
 * the checkpointed PDDB says belongs to this data set, which is not the same
 * as it being intact: JES2 may have purged and reallocated the track, the
 * chain may lead into a foreign block, and a block written while a data set
 * is still open can be cut short.  So every record is measured against the
 * end of the block before anything in it is read (#23), and every part of a
 * spanned line against what its FIRST part announced (#24).
 */
#include <stdlib.h>
#include <string.h>

#include "src/mvs/jes2/jesprb.h"

static int giveup(JESPRB *pb, int reason,
                  int (*emit)(char *line, unsigned linelen, void *arg),
                  void *arg);

/* JESCC_* of a record: its flags say what the control byte is, see jesprb.h */
#define ccval(flags, byte) \
    ((((unsigned)(flags) & (FLAG_HASCC | FLAG_ASA | FLAG_MACH)) << 8) \
     | (unsigned char)(byte))

int
__jesprb(char *blk, unsigned blklen, JESPRB *pb,
         int (*emit)(char *line, unsigned linelen, void *arg), void *arg)
{
    char        *p;
    char        *end;
    char        *d;
    PRLINE      *line;
    int         rc;

    pb->reason = JESPRB_OK;

    end = blk + blklen;

    for(p=&blk[10]; ; ) {
        /* The record header has to be inside the block BEFORE anything in it
           is read.  This used to be the second operand of an && whose first
           operand had already dereferenced the header (#23), so a header at
           the very end of the block was read either way.

           A tail too short to hold another header is the ordinary end of a
           block - a full block has no room for an EOB byte, and a block padded
           with zeroes walks 3 bytes at a time into exactly this.  Only a
           header that IS readable and then points past the end means the block
           is malformed; that is what JESPRB_TRUNC reports. */
        if (p + sizeof(PRLINE) > end) break;

        line = (PRLINE*)p;
        if (line->len == EOB) break;

        if (line->flags & FLAG_SPAN) {
            SPLINE *sp = (SPLINE*)line;

            if (p + sizeof(SPLINE) > end) return giveup(pb, JESPRB_TRUNC, emit, arg);

            d = (char*)sp->data;
            if (sp->flags & FLAG_FIRST) {
                unsigned newblock;

                if (d + 2 > end) return giveup(pb, JESPRB_TRUNC, emit, arg);
                newblock = *(unsigned short *)d;
                d += 2;

                if (newblock > pb->blksize) {
                    if (pb->prbuf) free(pb->prbuf);
                    pb->blksize = newblock;
                    pb->prbuf = calloc(1, pb->blksize + 4);
                    if (!pb->prbuf) {
                        pb->linelen    = 0;
                        pb->assembling = 0;
                        pb->reason     = JESPRB_NOMEM;
                        return -1;
                    }
                }

                pb->total      = newblock;  /* THIS line's announcement */
                pb->linelen    = 0;
                pb->assembling = 1;         /* a line is open from here */
                pb->spancc     = 0;
                if (sp->flags & FLAG_HASCC) {
                    /* not read when it would lie behind the block; the bounds
                       check below then ends the block as it always did */
                    if (d < end) pb->spancc = ccval(sp->flags, *d);
                    d++;
                }
            }

            /* A MIDDLE/LAST part with no FIRST opening the line.  prbuf may
               well be non-NULL - a shorter line completed earlier and left it
               behind - but it is sized for THAT line, not for what is arriving
               now, and there is no announced total to check against (#24). */
            if (!pb->prbuf || !pb->assembling)
                return giveup(pb, JESPRB_NOBUF, emit, arg);

            if (d + sp->len2 > end) return giveup(pb, JESPRB_TRUNC, emit, arg);

            /* The parts must fit what THIS line's FIRST part announced.  Not
               prbuf's size: that is the largest total seen so far, because the
               buffer only ever grows, so a short line arriving after a long
               one would be measured against the long one.  This used to be
               tested as `linelen > blksize` AFTER the copy had already run
               past the end of prbuf (#24). */
            if (pb->linelen + sp->len2 > pb->total)
                return giveup(pb, JESPRB_NOBUF, emit, arg);

            memcpy(&pb->prbuf[pb->linelen], d, sp->len2);
            pb->linelen += sp->len2;

            /* FLAG_LAST is now the only way a spanned line is handed out: the
               old `|| linelen > blksize` was the overflow escaping, and with
               the clamp above it can no longer happen. */
            if (sp->flags & FLAG_LAST) {
                if (pb->linelen) pb->lines++;
                pb->cc = pb->spancc;
                rc = emit(pb->prbuf, pb->linelen, arg);
                if (rc < 0) {
                    pb->reason = JESPRB_STOPPED;
                    pb->prtrc  = rc;
                    return -1;
                }
                pb->linelen    = 0;
                pb->assembling = 0;
            }

            p = d + sp->len2;
            continue;
        }

        d = (char*)line->data;
        if (line->flags & FLAG_HASCC) d++;  /* skip over carriage control character */

        if (d + line->len > end) return giveup(pb, JESPRB_TRUNC, emit, arg);

        /* d is at most end here, so the byte in front of it is in the block */
        pb->cc = (line->flags & FLAG_HASCC) ? ccval(line->flags, d[-1]) : 0;

        if (line->len) pb->lines++;
        rc = emit(d, line->len, arg);
        if (rc < 0) {
            pb->reason = JESPRB_STOPPED;
            pb->prtrc  = rc;
            return -1;
        }

        p = d + line->len;
    }

    return 0;
}

/* Give up on this block.
 *
 * Whatever of a spanned line was already assembled is handed out first: a
 * fragment the reader can see beats a line that silently disappears, and the
 * caller reports the reason alongside it.  Then the reassembly state is
 * dropped - without that, the next block would keep appending to a line whose
 * middle this block could not read, and hand out something that never existed
 * on the spool (#24).
 *
 * Returns 0: the block chain itself is intact, only this block ends here.
 * Only the print callback refusing the fragment stops the whole walk.      */
static int
giveup(JESPRB *pb, int reason,
       int (*emit)(char *line, unsigned linelen, void *arg), void *arg)
{
    int rc = 0;

    if (pb->assembling && pb->linelen) {
        pb->lines++;
        pb->cc = pb->spancc;
        rc = emit(pb->prbuf, pb->linelen, arg);
    }

    pb->linelen    = 0;
    pb->assembling = 0;

    if (rc < 0) {
        pb->reason = JESPRB_STOPPED;
        pb->prtrc  = rc;
        return -1;
    }

    pb->reason = reason;
    return 0;
}

/* The paper movement of one record, see mvs/jes2.h.
 *
 * The ASA codes are compared as characters, so this is right in EBCDIC on the
 * target and in ASCII under the host test.  A machine code is a bit pattern in
 * either: the low three bits say write (001) or immediate (011), the high five
 * what the paper does: 00000 nothing, 00001 to 00011 space 1 to 3, 10001 to
 * 11100 skip to channel 1 to 12.                                             */
void
jesccsp(unsigned cc, int *before, int *after)
{
    int     c   = (int)JESCC_BYTE(cc);
    int     n   = 1;

    if ((cc & JESCC_HAS) && (cc & JESCC_ASA)) {
        *after = 0;
        if (c == ' ')                   n = 1;
        else if (c == '0')              n = 2;
        else if (c == '-')              n = 3;
        else if (c == '+')              n = 0;
        else if (c >= '1' && c <= '9')  n = -(c - '0');
        else if (c == 'A')              n = -10;
        else if (c == 'B')              n = -11;
        else if (c == 'C')              n = -12;
        *before = n;
        return;
    }

    *before = 0;
    *after  = 1;

    if ((cc & JESCC_HAS) && (cc & JESCC_MACH)
        && ((c & 7) == 1 || (c & 7) == 3)) {
        int op = c >> 3;

        if (op <= 3)                        n = op;
        else if (op >= 0x11 && op <= 0x1C)  n = -(op - 0x10);
        else return;                        /* no carriage code */

        if ((c & 7) == 1) {
            *after  = n;
        } else {
            *before = n;
            *after  = 0;
        }
    }
}
