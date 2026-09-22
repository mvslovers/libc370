/* @@DDBUSY.C */
#include <stdio.h>
#include <string.h>
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
 * HOSOPEN dispatches by data set TYPE -- HO000 internal reader, HO100
 * 'SI', HO200 'SO', HO300 'PS' -- and an instream SYSIN reaches HO100,
 * whose non-XBM path falls through HO107 into the process-SYSOUT open
 * code, which is unconditional about it:
 *
 *     HO300    DS    0H
 *              L     R0,SDBDEB        GET SDB'S DEB POINTER.
 *              LTR   R0,R0            IF NO DEB, DATA SET IS
 *              BZ    HO110            CLOSED.  GO OPEN IT.
 *              TM    SJBFLG1,SJB1XBM  IF OPEN ALREADY AND XBM,
 *              BO    HORET            IGNORE OPEN.
 *              B     HOERR            NOT XBM BUT OPEN - ERROR.
 *
 * Plain SYSOUT never comes here: HO200 is its own block, and it keeps an
 * open count.
 *
 * Already open and not an execution batch monitor means HOERR, and the
 * S013 follows.  There is nothing to negotiate: the caller has to be told
 * instead of killed, which is what this makes possible.
 *
 * Three things bound the test, each measured on mvsdev 2026-09-22 rather
 * than reasoned about:
 *
 *  - AN INPUT DD, WHICH IS NOT THE SAME AS AN INPUT OPEN.  HOCSETUP
 *    dispatches on DSNDSTYP -- the data set's TYPE, 'SI'/'SO'/'PS' --
 *    not on the DCB's mode, so the mode of the incoming open is only a
 *    proxy for it.  The two partitions do not coincide, and the gap is
 *    not theoretical: fopen("dd:SYSPRINT","r") while stdout holds
 *    SYSPRINT SUCCEEDS on the pre-fix library (JOB00429), because JES2
 *    routes an 'SO' data set to HO200, which keeps an open count and is
 *    re-entrant.  Refusing on the incoming mode alone would have broken
 *    that.  So the test is on the direction of the stream ALREADY
 *    holding the DD: an output DD is held for write, an input DD for
 *    read.  That is a PROXY for DSNDSTYP, not JES2's discriminator: it
 *    coincides with it for every DD shape measured, and the residual
 *    cell is an 'SO' data set opened for read while ANOTHER READ stream
 *    already holds it, which HO200's open count would allow and this
 *    still refuses.  That needs a first read open of a SYSOUT DD to have
 *    happened, so it is rarer than the cell above, but it is the same
 *    class and it is not measured.  The exact discriminator is the
 *    JFCB's SYSOUT class, and it is out of reach here -- __rdjfcb() runs
 *    at @@fpopen.c AFTER __aopen(), so reading it first would mean an
 *    RDJFCB with its own DCB and exit list.  Not worth it for #184.
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
 * Returns 1 when the OPEN must be refused, 0 otherwise.
 *
 * KNOWN BLIND SPOT.  It can only see DCBs that fopen() registered in
 * grt->grtfile.  A DD opened by ropen() (src/clib/ropen.c calls __aopen()
 * directly), by hand-written assembler, by another library, or by a
 * subtask with a different GRT is invisible here, and such a case still
 * abends exactly as before.  The DSAB carries dsabopct, an open-DCB count
 * that would see all of them -- but it counts without regard to
 * direction, so on its own it would refuse the very cell JOB00429 shows
 * working.  Closing the blind spot means combining the two, which is
 * more machinery than #184 needs.
 */
int __ddbusy(FILE *fp)
{
    CLIBGRT *grt;
    DSAB    *dsab;
    TIOTDD  *tiotdd;
    unsigned count;
    unsigned i;
    int      busy = 0;
    int      owned;

    if (!fp) return 0;
    if (!fp->ddname[0]) return 0;

    /* an input open.  __fpmode() makes READ and WRITE mutually exclusive
       and rejects '+', so the second test cannot fire once the first has
       passed; it is belt and braces against that changing. */
    if (!(fp->flags & _FILE_FLAG_READ)) return 0;
    if (fp->flags & _FILE_FLAG_WRITE) return 0;

    dsab = get_dsab(0, fp->ddname);
    if (!dsab) return 0;

    tiotdd = dsab->dsabtiot;
    if (!tiotdd) return 0;

    /* The pair, as IBM's own OPEN tests it: IFG0RR0B.asm:500 is
       TM TIOESYIN-24(TIOTPTR),B'00000110' -- X'04' | X'02'.  On 3.8j
       only X'02' is ever set (measured across 9 DDs), so this is
       behaviour-identical today; testing the pair stops the check
       resting on that observation. */
    if (!(tiotdd->TIOELINK & (TIOESYIN | TIOESSDS))) return 0;

    grt = __grtget();
    if (!grt) return 0;
    if (!grt->grtfile) return 0;

    /* fopen() adds under this lock and __fpterm() removes under it, so
       reading the array without it can see a FILE mid-removal.  No
       nesting risk: fopen() takes it only after __fpopen() has returned. */
    /* lock() answers 8 when this task already holds it, and unlocking
       then would release someone else's claim.  No caller reaching here
       holds it today -- fopen() takes it only after __fpopen() returns --
       but this is the one site that would break silently if that changed,
       so it uses the house idiom (fclose.c:26, @@fpterm.c). */
    owned = (lock(&grt->grtfile, 0) == 0);
    count = arraycount(&grt->grtfile);
    for (i = 0; i < count; i++) {
        FILE *other = grt->grtfile[i];

        if (!other) continue;
        /* unreachable today: fopen() registers fp only after __fpopen()
           returns, and __reopen() passes a freshly created FILE.  Kept as
           defence, not as a live case. */
        if (other == fp) continue;
        /* a FILE with no DCB is registered but not open, so it holds no
           DEB and cannot be what JES2 is objecting to */
        if (!other->dcb) continue;
        /* the holder's direction is the DD's direction, and that is what
           JES2 keys on.  A DD held for WRITE is an 'SO' data set, whose
           second open HO200 allows -- measured, JOB00429. */
        if (!(other->flags & _FILE_FLAG_READ)) continue;
        if (other->flags & _FILE_FLAG_WRITE) continue;
        if (memcmp(other->ddname, fp->ddname, sizeof(fp->ddname)) == 0) {
            busy = 1;
            break;
        }
    }
    if (owned) unlock(&grt->grtfile, 0);

    return busy;
}
