/*
 * tsttrym.c - libc370 #476 on MVS: try() returns a negative value when
 * the library cannot set up its recovery routine.
 *
 * __estae() returns -1 when ten recovery routines are already in use;
 * try() negated that to +1, which reads as user abend 1.
 *
 * The probe establishes estae() levels until __estae() refuses, calls
 * try(), and deletes its levels again.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tsttrym.c \
 *         -o $S/TSTTRYG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tsttrym.c \
 *         -o $S/TSTTRYR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTTRYG=TSTTRYG.iebcopy \
 *         TSTTRYR=TSTTRYR.iebcopy -o ty -xmit --dsn IBMUSER.LIBC370.TYSCR
 *
 * then jcl/recvtrym.jcl and jcl/tsttrym.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <mvs/wto.h>
#include <mvs/recovery.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTTRYM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static int exitfn(void *sdwa, void *data)
{
    (void)sdwa;
    (void)data;
    return 0;
}

static int fine(void)
{
    return 0;
}

int main(void)
{
    int     n = 0, rc;

    rc = try(fine, 0);
    want(rc == 0, "control: try() of a function that returns");

    while (n < 20 && estae(ESTAE_CREATE, exitfn, 0) == 0) n++;
    rc = try(fine, 0);
    wtof("TSTTRYM   %d levels added, try() with none left: %d", n, rc);
    want(rc < 0, "try() without a free level is negative, not +1");
    while (n-- > 0) estae(ESTAE_DELETE, 0, 0);

    rc = try(fine, 0);
    want(rc == 0, "try() works again after the levels are deleted");
    return bad ? 8 : 0;
}
