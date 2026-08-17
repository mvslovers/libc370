/* JESPRINT.C - Print JES Job by DSID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hasphct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "haspjct.h"    /* JES Job Control Table                            */
#include "hasppddb.h"   /* JES PDDB Print Datasets                          */
#include "haspiot.h"    /* JES IOT                                          */
#include "ieftxtft.h"   /* text string types                                */
#include "iefvkeys.h"   /* text key values                                  */
#include "clibjes2.h"   /* JES prototypes */
#include "clibary.h"    /* dynamic array */
#include "jesprb.h"     /* the record walk, asm-free so it can be host-tested */

/* what esc_print() needs to reach the caller's print callback, since the
   record walk hands it a single void *arg                                  */
typedef struct prctx {
    int     (*prt)(const char *line, unsigned linelen, void *arg);
    void    *arg;
} PRCTX;

static int esc_print(char *line, unsigned linelen, void *arg);

int jesprint(JES *jes, JESJOB *job, unsigned dsid,
             int(*prt)(const char *line, unsigned linelen, void *arg),
             void *arg, JESPRST *st)
{
    int         rc      = 503;      /* service unavailable */
    JESPRST     stat;               /* used when the caller passes st = NULL */
    JESDD       *jesdd  = NULL;
    HASPCP      *cp     = NULL;
    HASPJS      *js     = NULL;
    __HCT       *hct    = NULL;
    char        *buf    = NULL;
    PRBLOCK     *block;
    PRCTX       ctx;
    JESPRB      pb;                 /* record-walk state, spans the blocks   */
    unsigned    bufsize;
    unsigned    mttr;
    unsigned    count;
    unsigned    n;

    if (!st) st = &stat;            /* one code path, no NULL tests below    */
    memset(st, 0, sizeof(JESPRST));
    memset(&pb, 0, sizeof(JESPRB));
    st->reason = JESPR_EMPTY;       /* nothing walked yet                    */

    ctx.prt = prt;
    ctx.arg = arg;

    if (!jes) goto quit;
    if (!job) goto quit;
    if (!dsid) goto quit;

    cp     = jes->cp;
    if (!cp) goto quit;

    hct    = &cp->hct;
    bufsize = hct->_BUFSIZE;

    if (!jes->js) goto quit;    /* an empty array is not indexable (#108)  */

    js     = jes->js[0];
    if (!js) goto quit;

    buf = calloc(1, bufsize);
    if (!buf) {
        wtof("Unable to allocate storage for %u byte buffer", bufsize);
        st->reason = JESPR_NOMEM;
        goto quit;
    }

    count = arraycount(&job->jesdd);
    for(n=0; n < count; n++) {
        JESDD *dd = job->jesdd[n];

        if (!dd) continue;
        /* wtof("dd->dsid=%u, dsid=%u", dd->dsid, dsid); */
        if (dd->dsid == dsid) {
            jesdd = dd;
            break;
        }
    }

    rc = 404;   /* not found */
    if (!jesdd) goto quit;

    /* at this point we know which DSID in the job we want to start with */
    rc = 0;
    if (!jesdd->mttr) goto quit;    /* no sysout for this dsid, nothing to do */

    /* process the sysout dataset */
    st->reason = JESPR_END;         /* until something stops the walk early  */

    for(block = (PRBLOCK*)buf, mttr = jesdd->mttr; mttr; mttr = block->next) {
        /* runaway guard: the next address comes out of the block we read */
        if (st->blocks >= JESPR_MAXBLK) {
            st->reason = JESPR_CAP;
            st->mttr   = mttr;
            break;
        }

        /* read block from spool dataset */
        if (spool_read(js, mttr, buf, hct->_BUFSIZE)) {
            st->reason = JESPR_IOERR;
            st->mttr   = mttr;
            break;
        }

        /* Make sure this block is for our job.  Where the mismatch happens
           decides what it means, and the two must not be confused:

           on the FIRST block  nothing of this data set was read.  The
                               checkpointed PDDB points at tracks that now
                               belong to somebody else - JES2 purged the data
                               set and reallocated them.  The data is gone.

           after N blocks      the data set is still open.  Its last written
                               block chains to a track that is allocated but
                               not yet written, so it carries a foreign key.
                               Everything written so far WAS read; this is the
                               normal end of an open data set, not a loss.  */
        if (job->jobkey != block->jobkey) {
            st->reason = st->blocks ? JESPR_OPENEND : JESPR_FOREIGN;
            st->mttr   = mttr;
            break;
        }
        if (jesdd->dsid != block->dsid) {
            st->reason = st->blocks ? JESPR_OPENEND : JESPR_DSID;
            st->mttr   = mttr;
            break;
        }

        st->blocks++;
#if 0
        wtodumpf(buf, sizeof(PRBLOCK), "PRBLOCK");
#endif
        /* unwind the print lines in the block */
        if (__jesprb(buf, bufsize, &pb, esc_print, &ctx) < 0) {
            if (pb.reason == JESPRB_NOMEM) {
                wtof("Unable to allocate storage for %u byte buffer", pb.blksize + 4);
                st->reason = JESPR_NOMEM;
            } else {
                st->reason = JESPR_STOPPED;
                st->prtrc  = pb.prtrc;
            }
            st->mttr = mttr;
            goto quit;
        }

        /* the record walk gave up on this block but the chain is intact, so
           the remaining blocks are still read; the reason survives to the end
           of the walk.  Whatever the walk had already assembled of a spanned
           line went out as a truncated line before it gave up (#24).       */
        if (pb.reason == JESPRB_NOBUF || pb.reason == JESPRB_TRUNC) {
            st->reason = (pb.reason == JESPRB_TRUNC) ? JESPR_TRUNC : JESPR_NOBUF;
            st->mttr   = mttr;
        }

        /* a block that chains to itself would spin forever */
        if (block->next == mttr) {
            st->reason = JESPR_LOOP;
            st->mttr   = mttr;
            break;
        }
    }

quit:
    st->lines = pb.lines;
    if (pb.prbuf) free(pb.prbuf);
    if (buf) free(buf);
    return rc;
}

__asm__("\n&FUNC    SETC 'esc_print'");
static int
esc_print(char *line, unsigned linelen, void *arg)
{
    int         rc = 0;
    PRCTX       *ctx = (PRCTX*)arg;

    if (!linelen) goto quit;

    /* wtodumpf(line, linelen, "esc_print()"); */

    /* here we attempt to remove any trailing spaces */
    while(linelen > 1 && line[linelen-1]==' ') linelen--;

    /* here we limit linelen to 255 (mostly for the inline translate of unprintable characters) */
    if (linelen > 255) linelen = 255;

    /* translate any unprintable characters to spaces */
    __asm("LR\t1,%0          => our print line\n\t"
          "LR\t14,%1         => length of line\n\t"
          "BCTR\t14,0        decrement for execute\n\t"
          "EX\t14,TRLINE     translate unsafe characters"
          : :"r"(line), "r"(linelen) : "14", "1");

    /* print the translated buffer */
    rc = ctx->prt(line, linelen, ctx->arg);
quit:
    return rc;
}
__asm("\nTRLINE   TR    0(*-*,1),PRTXLATE   REMOVE UNPRINTABLES");
__asm("\nPRTXLATE DC    64C' ',191AL1(*-PRTXLATE),C' '");
