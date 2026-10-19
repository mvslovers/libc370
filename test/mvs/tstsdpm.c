/*
 * tstsdpm.c - libc370 #413 on MVS: __abrpt() with DUMP_SDUMP behaves as
 * DUMP_DEFAULT and writes no "system_dump requested".
 *
 * A function run under try() establishes the report routine with
 * abendrpt(ESTAE_CREATE, DUMP_SDUMP) and abends S0C1.  The report routine,
 * the newer recovery routine, writes its register and traceback report and
 * percolates; try() retries and returns the abend code.
 *
 * The verdict is the job log: the library before the fix writes
 * "system_dump requested" after the report, the fixed one does not.  The
 * return code says only that the recovery path ran: 0 when try() returned
 * the S0C1, 8 otherwise.
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * MAIN a build of origin/main for the red control):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstsdpm.c \
 *         -o $S/TSTSDPG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         test/mvs/tstsdpm.c -o $S/TSTSDPR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSDPG=TSTSDPG.iebcopy \
 *         TSTSDPR=TSTSDPR.iebcopy -o sdp -xmit --dsn IBMUSER.LIBC370.SDPSCR
 *
 * then jcl/recvsdpm.jcl and jcl/tstsdpm.jcl.
 */
#include <mvs/wto.h>
#include <mvs/recovery.h>

static int boom(void *unused)
{
    (void)unused;
    abendrpt(ESTAE_CREATE, DUMP_SDUMP);
    __asm__("DC\tH'0'");
    return 0;
}

int main(void)
{
    int     rc;

    wtof("TSTSDPM begin: abend S0C1 with DUMP_SDUMP");
    rc = try(boom, 0);
    wtof("TSTSDPM end: try() returned %08X", rc);
    wtof("TSTSDPM VERDICT %s", rc == 0x000C1000 ? "recovered" : "UNEXPECTED");
    return rc == 0x000C1000 ? 0 : 8;
}
