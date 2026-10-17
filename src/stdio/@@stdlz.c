/* @@STDLZ.C - the standard streams, opened on first use (#564)
**
** The start-up used to open stdout, stderr and stdin before main(), each
** with an OPEN and a CLOSE at the end, and for stderr without a SYSTERM
** DD and stdin without a SYSIN DD a dynamic allocation as well: measured
** on MVS as 7-15 ms (stderr) and 4-10 ms (stdin) of CPU for a program
** that may never touch them.  Now __rtinit() only sets GRTFLAG1_STDLAZY,
** and __gtin()/__gtout()/__gterr() call here the first time a stream is
** still NULL.
**
** - Each stream is tried once: its TRY bit is set before the open, so a
**   stream used inside its own open (the open reporting through stderr)
**   gets NULL rather than recursing, and a stream the program closed is
**   not opened again.
** - The GRT is locked around the attempt, so two threads' first use opens
**   it once; the thread that comes second waits and finds it open.  (In
**   2.15.0 the TRY bit was tested before the lock, and the second thread
**   could get NULL - fixed in 2.16.0.)
** - Once __exit() closes the files (GRTFLAG1_STDEND) nothing is opened.
** - What is opened is what __rtinit() opened: "*SYSPRINT" and "*SYSTERM"
**   (the DD, else a SYSOUT data set), "dd:SYSIN" else 'NULLFILE'.  It is
**   opened through __fpopn(), fopen() without the handlers for new and
**   temporary data sets and the TSO streams (size analysis F5).  An
**   output stream that cannot be opened is reported with a WTO and opened
**   on 'NULLFILE', so the program runs and its output is lost audibly.
*/
#include "src/internal/fileio.h"
#include <stdio.h>
#include "mvs/crt.h"
#include "mvs/lock.h"
#include "mvs/wto.h"

static void lost(const char *dd)
{
    char    msg[80];
    int     i = 0;
    const char *p;

    for (p = "libc370: "; *p; p++) msg[i++] = *p;
    for (p = dd; *p; p++) msg[i++] = *p;
    for (p = " could not be opened, output discarded"; *p; p++) msg[i++] = *p;
    msg[i] = 0;
    wto(msg);
}

void
__stdlz(int which)
{
    CLIBGRT     *grt    = __grtget();
    void        **slot;
    char        bit;
    int         owned;

    if (!grt || (grt->grtflag1 & GRTFLAG1_STDEND)) return;

    switch (which) {
    case 0:  slot = &grt->grtin;  bit = GRTFLAG1_INTRY;  break;
    case 1:  slot = &grt->grtout; bit = GRTFLAG1_OUTTRY; break;
    default: slot = &grt->grterr; bit = GRTFLAG1_ERRTRY; break;
    }
    if (*slot) return;

    /* The TRY bit is tested under the lock only.  Tested before it, a
       second task arriving while the first is still in the open saw the
       bit, skipped the lock and got NULL (fixed in 2.16.0); now it waits
       for the lock and finds the stream open.  The same task, re-entering
       from inside its own open, gets rc 8 here, sees the bit and
       returns NULL without recursing. */
    owned = (lock(grt, 0) == 0);    /* 8: this task holds it already */
    if (!*slot && !(grt->grtflag1 & bit)) {
        grt->grtflag1 |= bit;
        switch (which) {
        case 0:
            *slot = __fpopn("dd:SYSIN", "r", 0, 0);
            if (!*slot) *slot = __fpopn("'NULLFILE'", "r", 0, 0);
            break;
        case 1:
            *slot = __fpopn("*SYSPRINT", "w", 0, 0);
            if (!*slot) {
                lost("stdout (SYSPRINT)");
                *slot = __fpopn("'NULLFILE'", "w", 0, 0);
            }
            break;
        default:
            *slot = __fpopn("*SYSTERM", "w", 0, 0);
            if (!*slot) {
                lost("stderr (SYSTERM)");
                *slot = __fpopn("'NULLFILE'", "w", 0, 0);
            }
            break;
        }
    }
    if (owned) unlock(grt, 0);
}
