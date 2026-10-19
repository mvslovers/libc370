/*
 * tsttsom.c - libc370 #413 on MVS: tsocmd() leaves the caller's ECT as
 * it was.  (Runs as a TSO command under IKJEFT01.)
 *
 * tsocmd() wrote the command name it called into the ECT of the caller's
 * TSO session and set or cleared its no-operands bit, and did not put
 * them back: later commands in the session saw the wrong name.
 *
 *   1 the program runs as a TSO command (it has a CPPL)
 *   2 after tsocmd("IEFBR14", "X") the ECT's command name and switches
 *     are those from before the call
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tsttsom.c \
 *         -o $S/TSTTSOG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tsttsom.c \
 *         -o $S/TSTTSOR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTTSOG=TSTTSOG.iebcopy \
 *         TSTTSOR=TSTTSOR.iebcopy -o tt -xmit --dsn IBMUSER.LIBC370.TTSCR
 *
 * then jcl/recvtsom.jcl and jcl/tsttsom.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/crt.h>
#include <mvs/tso.h>
#include <ibm/mvs/ikjcppl.h>
#include <ibm/mvs/ikject.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTTSOM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    CLIBPPA *ppa = __ppaget();
    CPPL    *cppl = ppa ? (CPPL *)ppa->ppacppl : NULL;
    ECT     *ect;
    char    cmd[9];
    unsigned char sws;

    want(cppl != NULL, "runs as a TSO command");
    if (!cppl) return 8;
    ect = cppl->cpplect;
    memcpy(cmd, ect->ectpcmd, 8);
    cmd[8] = 0;
    sws = ect->ectsws;

    tsocmd("IEFBR14", "X");

    wtof("TSTTSOM   ECT before \"%s\" %02X, after \"%.8s\" %02X",
         cmd, sws, ect->ectpcmd, ect->ectsws);
    want(memcmp(ect->ectpcmd, cmd, 8) == 0 && ect->ectsws == sws,
         "tsocmd() leaves the ECT as it was");
    return bad ? 8 : 0;
}
