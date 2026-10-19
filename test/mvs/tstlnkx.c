/*
 * tstlnkx.c - libc370 #553 on MVS: __linkx(), __linkdsx() and __loadx()
 * pass on the completion code (and for LOAD the reason code) of the
 * error exit that __link() and __load() discard.
 *
 * A module that does not exist gives S806 reason 4 in LINK's and LOAD's
 * error exit; IEFBR14 is the control that exists.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstlnkx.c \
 *         -o $S/TSTLNKXG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DRED test/mvs/tstlnkx.c \
 *         -o $S/TSTLNKXR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTLNKXG=TSTLNKXG.iebcopy \
 *         TSTLNKXR=TSTLNKXR.iebcopy -o lx -xmit --dsn IBMUSER.LIBC370.LKXSCR
 *
 * then jcl/recvlnkx.jcl and jcl/tstlnkx.jcl.  RED is built with -DRED
 * against a library without the new entries: it calls __link() and
 * __load() and fails because neither tells why it failed.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <mvs/wto.h>
#include <mvs/link.h>

#define MISSING "NOSUCHMD"
#define PRESENT "IEFBR14"

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTLNKX %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#ifdef RED
int main(void)
{
    int         rc, prc = 0;
    void        *ep;

    rc = __link(MISSING, 0, 0, &prc);
    wtof("TSTLNKX   __link(%s) = %d, prc %d", MISSING, rc, prc);
    ep = __load(0, MISSING, 0, 0);
    wtof("TSTLNKX   __load(%s) = %p", MISSING, ep);
    want(rc == -1 && !ep, "control: both fail");
    want(0, "__link()/__load() report the completion code 806-4");
    return bad ? 8 : 0;
}
#else
static void loadx(void)
{
    unsigned    ab = 0xDEADBEEF, rs = 0xDEADBEEF, size = 0;
    char        ac = 0x55;
    void        *ep;

    ep = __loadx(0, MISSING, &size, &ac, &ab, &rs);
    wtof("TSTLNKX   __loadx(%s) = %p, abcode %X, reason %X",
         MISSING, ep, ab, rs);
    want(!ep && ab == 0x806 && rs == 4, "__loadx() missing: 0, 806-4");

    ab = rs = 0xDEADBEEF;
    ep = __loadx(0, PRESENT, &size, &ac, &ab, &rs);
    wtof("TSTLNKX   __loadx(%s) = %p, size %u, ac %d, abcode %X, reason %X",
         PRESENT, ep, size, ac, ab, rs);
    want(ep && size && ab == 0 && rs == 0, "__loadx() present: EP, 0, 0");
    if (ep) want(__delete(PRESENT) == 0, "__delete() after __loadx()");

    ep = __loadx(0, MISSING, 0, 0, 0, 0);
    want(!ep, "__loadx() with NULL out-parameters");

    ab = rs = 0xDEADBEEF;
    ep = __loadx(0, "", 0, 0, &ab, &rs);
    want(!ep && ab == 0 && rs == 0, "__loadx() empty name: 0, 0, 0");
}

static void linkx(void)
{
    unsigned    ab = 0xDEADBEEF;
    int         rc, prc = 99;

    rc = __linkx(MISSING, 0, 0, &prc, &ab);
    wtof("TSTLNKX   __linkx(%s) = %d, prc %d, abcode %X", MISSING, rc, prc, ab);
    want(rc == -1 && prc == -1 && ab == 0x806, "__linkx() missing: -1, 806");

    ab = 0xDEADBEEF;
    prc = 99;
    rc = __linkx(PRESENT, 0, 0, &prc, &ab);
    wtof("TSTLNKX   __linkx(%s) = %d, prc %d, abcode %X", PRESENT, rc, prc, ab);
    want(rc == 0 && prc == 0 && ab == 0, "__linkx() present: 0, abcode 0");

    rc = __linkx(MISSING, 0, 0, 0, 0);
    want(rc == -1, "__linkx() with NULL out-parameters");

    ab = 0xDEADBEEF;
    rc = __linkx(0, 0, 0, 0, &ab);
    want(rc == -1 && ab == 0, "__linkx() NULL name: -1, abcode 0");

    ab = 0xDEADBEEF;
    prc = 99;
    rc = __linkdsx(MISSING, 0, 0, &prc, &ab);
    wtof("TSTLNKX   __linkdsx(%s) = %d, prc %d, abcode %X",
         MISSING, rc, prc, ab);
    want(rc == 0 && prc == -1 && ab == 0x806, "__linkdsx() missing: 0, -1, 806");

    ab = 0xDEADBEEF;
    prc = 99;
    rc = __linkdsx(PRESENT, 0, 0, &prc, &ab);
    wtof("TSTLNKX   __linkdsx(%s) = %d, prc %d, abcode %X",
         PRESENT, rc, prc, ab);
    want(rc == 0 && prc == 0 && ab == 0, "__linkdsx() present: 0, 0, 0");
}

int main(void)
{
    loadx();
    linkx();
    return bad ? 8 : 0;
}
#endif
