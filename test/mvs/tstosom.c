/*
 * tstosom.c - libc370 #412 on MVS: osxopen() and osdopen() refuse a NULL
 * DCB.
 *
 * Both handed DCB address 0 to OPEN under try(), discarded the abend,
 * read DCBOFLGS from low storage and returned an rc they never set.
 *
 *   1 osxopen(NULL, 0) returns 8
 *   2 osdopen(NULL, 0) returns 8
 *   3 control: osxopen() of a DCB for DD IN opens it
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstosom.c \
 *         -o $S/TSTOSOG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstosom.c \
 *         -o $S/TSTOSOR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTOSOG=TSTOSOG.iebcopy \
 *         TSTOSOR=TSTOSOR.iebcopy -o oo -xmit --dsn IBMUSER.LIBC370.OOSCR
 *
 * then jcl/recvosom.jcl and jcl/tstosom.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <mvs/wto.h>
#include <mvs/osio.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTOSOM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    DCB     *dcb;
    int     rc;

    rc = osxopen(NULL, 0);
    wtof("TSTOSOM   osxopen(NULL) %d", rc);
    want(rc == 8, "osxopen(NULL) returns 8");
    rc = osdopen(NULL, 0);
    wtof("TSTOSOM   osdopen(NULL) %d", rc);
    want(rc == 8, "osdopen(NULL) returns 8");

    dcb = osxdcb("DD:IN", NULL);
    rc = dcb ? osxopen(dcb, 0) : -1;
    wtof("TSTOSOM   osxopen(DD:IN) %d", rc);
    want(rc == 0, "control: osxopen() of DD IN");
    if (rc == 0) osxclose(dcb, 1);
    return bad ? 8 : 0;
}
