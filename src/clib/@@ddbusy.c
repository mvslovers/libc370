/* @@DDBUSY.C */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <clib.h>
#include <clibary.h>
#include "cliblock.h"
#include <clibdsab.h>
#include <clibcrt.h>
#include <ieftiot.h>

/*
 * __ddbusy() - would this OPEN be the second concurrent DCB on a spool
 * data set, i.e. the one JES2 refuses with ABEND S013-C0 (#184)?
 *
 * HASPSSSM's SSI open path is unconditional about it:
 *
 *     HO300    L     R0,SDBDEB      GET SDB'S DEB POINTER.
 *              LTR   R0,R0          IF NO DEB, DATA SET IS
 *              BZ    HO110          CLOSED.  GO OPEN IT.
 *              TM    SJBFLG1,SJB1XBM  IF OPEN ALREADY AND XBM,
 *              BO    HORET            IGNORE OPEN.
 *              B     HOERR            NOT XBM BUT OPEN - ERROR.
 *
 * Already open and not an execution batch monitor means HOERR, and the
 * S013 follows.  There is nothing to negotiate: the caller has to be told
 * instead of killed, which is what this makes possible.
 *
 * Three things bound the test, each measured on mvsdev 2026-09-22 rather
 * than reasoned about:
 *
 *  - INPUT ONLY.  SYSOUT goes to HASPSSSM HO200, which keeps an open
 *    count and returns happily on the second open.  A doubled open of
 *    SYSPRINT works (JOB00425), so refusing it would break working code.
 *
 *  - SPOOL ONLY.  A real data set tolerates two concurrent DCBs
 *    (JOB00424 step SI3).  The discriminator is the TIOT entry's flag
 *    byte -- and it is TIOESSDS (X'02', "subsystem data set", the VS2
 *    meaning), NOT the TIOESYIN (X'04') that ieftiot.h's comment points
 *    at: X'04' is never set on 3.8j (JOB00426).  A check written from
 *    that comment compiles, runs and never fires.
 *
 *  - ALREADY OPEN HERE.  The refusal is about a live DEB, so a spool DD
 *    that nothing holds open is fine: fclose(stdin) then fopen("dd:SYSIN")
 *    succeeds and re-reads from the top (JOB00424 step SI2).
 *
 * Returns 1 when the OPEN must be refused, 0 otherwise.  Conservative in
 * both directions: anything it cannot establish returns 0, so an OPEN
 * that would have worked is never refused on a guess.
 */
int __ddbusy(FILE *fp)
{
    CLIBGRT *grt;
    DSAB    *dsab;
    TIOTDD  *tiotdd;
    unsigned count;
    unsigned i;
    int      busy = 0;

    if (!fp) return 0;
    if (!fp->ddname[0]) return 0;

    /* input opens only - see SYSOUT above */
    if (!(fp->flags & _FILE_FLAG_READ)) return 0;
    if (fp->flags & _FILE_FLAG_WRITE) return 0;

    dsab = get_dsab(0, fp->ddname);
    if (!dsab) return 0;

    tiotdd = dsab->dsabtiot;
    if (!tiotdd) return 0;

    /* TIOESSDS, not TIOESYIN - see above */
    if (!(tiotdd->TIOELINK & TIOESSDS)) return 0;

    grt = __grtget();
    if (!grt) return 0;
    if (!grt->grtfile) return 0;

    /* fopen() adds under this lock and __fpterm() removes under it, so
       reading the array without it can see a FILE mid-removal.  No
       nesting risk: fopen() takes it only after __fpopen() has returned. */
    lock(&grt->grtfile, 0);
    count = arraycount(&grt->grtfile);
    for (i = 0; i < count; i++) {
        FILE *other = grt->grtfile[i];

        if (!other) continue;
        if (other == fp) continue;
        /* a FILE with no DCB is registered but not open, so it holds no
           DEB and cannot be what JES2 is objecting to */
        if (!other->dcb) continue;
        if (memcmp(other->ddname, fp->ddname, sizeof(fp->ddname)) == 0) {
            busy = 1;
            break;
        }
    }
    unlock(&grt->grtfile, 0);

    return busy;
}
