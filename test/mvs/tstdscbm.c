/*
 * tstdscbm.c - libc370 #409 on MVS: __dscbav() reads the DSCB at a binary
 * CCHHR.
 *
 * __dscbav() copied its 5-byte CCHHR with strcpyp(), which stops at the
 * first X'00' and pads with blanks.  The cylinder and head numbers of a
 * real address start with X'00' bytes, so OBTAIN got X'4040404040'.
 *
 *   1 __locate() of SYS1.LINKLIB gives the volume; __dscbv() reads its
 *     format-4 DSCB (OBTAIN SEARCH, the 96-byte data part)
 *   2 the VTOC starts at the lower CCHH of DS4VTOCE (data offset 61); its
 *     first record, R=1, is the format-4 DSCB itself
 *   3 __dscbav() of that CCHHR returns rc 0, a 44-byte key of X'04' and
 *     the same 96 data bytes as __dscbv() (OBTAIN SEEK: key and data)
 *
 * Offsets are IECSDSL1's, not struct dscb4's (#173).
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstdscbm.c \
 *         -o $S/TSTDSCBG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstdscbm.c \
 *         -o $S/TSTDSCBR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTDSCBG=TSTDSCBG.iebcopy \
 *         TSTDSCBR=TSTDSCBR.iebcopy -o db -xmit --dsn IBMUSER.LIBC370.DBSCR
 *
 * then jcl/recvdscbm.jcl and jcl/tstdscbm.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dscb.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTDSCBM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    static const char   dsn[44] = "SYS1.LINKLIB                                ";
    static LOCWORK      loc;
    static DSCB         f4, seek;
    unsigned char       *v  = (unsigned char *)f4.work;
    unsigned char       *a  = (unsigned char *)seek.work;
    char                cchhr[5];
    int                 rc, i, keyok;

    rc = __locate(dsn, &loc);
    want(rc == 0, "__locate(SYS1.LINKLIB)");
    if (rc) return 8;
    wtof("TSTDSCBM   volume %-6.6s", loc.volser);

    rc = __dscbv(loc.volser, &f4);
    want(rc == 0 && v[0] == 0xF4, "__dscbv(): format-4 DSCB");
    if (rc) return 8;

    memcpy(cchhr, v + 61 + 2, 4);   /* DS4VTOCE: type, seq, lower CCHH */
    cchhr[4] = 1;                   /* R1: the format-4 DSCB */
    wtof("TSTDSCBM   VTOC CCHHR %02X%02X%02X%02X%02X",
         cchhr[0], cchhr[1], cchhr[2], cchhr[3], cchhr[4]);

    memset(&seek, 0xEE, sizeof(seek));
    rc = __dscbav(cchhr, loc.volser, &seek);
    wtof("TSTDSCBM   __dscbav rc %d, key %02X, byte 44 %02X", rc, a[0], a[44]);
    want(rc == 0, "__dscbav() of the VTOC's first record, rc 0");

    for (keyok = 1, i = 0; i < 44; i++) if (a[i] != 0x04) keyok = 0;
    want(rc == 0 && keyok, "its key is 44 bytes of X'04'");
    want(rc == 0 && memcmp(a + 44, v, 96) == 0,
         "its data is the format-4 DSCB __dscbv() read");
    return bad ? 8 : 0;
}
