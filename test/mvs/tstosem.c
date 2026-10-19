/*
 * tstosem.c - libc370 #412 on MVS: a read-only open for EXCP and BDAM,
 * and the default EODAD and SYNAD of osbdcb()/osddcb().
 *
 * osxopen() and osdopen() always opened for UPDAT.  osbdcb() built a DCB
 * without EODAD and SYNAD, so CHECK abended at end of data (S337) and on
 * an I/O error (S001), and oscheck() returned the abend.
 *
 *   1 osxopenm(dcb, 0, "r") of DD RO (DISP=SHR) opens for INPUT: the
 *     DEB's open option (DEBOPATB low 4 bits) is 0.  The red has only
 *     osxopen(), which opens for UPDAT (4).
 *   2 control: osxopen() still opens for UPDAT.
 *   3 osdopenm(dcb, 0, "r") the same for BDAM, and a block can be read.
 *   4 BSAM read to the end of DD SEQ: oscheck() returns 4 through the
 *     library's EODAD after the two blocks.  The red returned the S337.
 *   5 BSAM read of DD WL, whose DD says BLKSIZE=400 for blocks of 800:
 *     a wrong-length record, an I/O error.  oscheck() returns 8 through
 *     the library's SYNAD.  The red returned the S001.
 *   6 the same with a SYNAD of the caller's, stored after osbdcb(): it is
 *     called, and oscheck() returns 8 when it has returned.  The red
 *     called it too, and oscheck() returned 0.
 *   7 control: a CHECK of the caller's own on a DCB with the library's
 *     EODAD, at end of data, ends in S337 as without an EODAD.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstosem.c \
 *         -o $S/TSTOSEG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DRED test/mvs/tstosem.c \
 *         -o $S/TSTOSER -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTOSEG=TSTOSEG.iebcopy \
 *         TSTOSER=TSTOSER.iebcopy -o oe -xmit --dsn IBMUSER.LIBC370.OESCR
 *
 * then jcl/recvosem.jcl and jcl/tstosem.jcl.  -DRED builds against a
 * library without osxopenm()/osdopenm(): it calls osxopen()/osdopen().
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/osio.h>
#include <mvs/recovery.h>

#ifdef RED
#define osxopenm(dcb, typej, mode)  osxopen((dcb), (typej))
#define osdopenm(dcb, typej, mode)  osdopen((dcb), (typej))
#endif

static int  bad;
static char buf[800];

static void want(int cond, const char *what)
{
    wtof("TSTOSEM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* the open option OPEN recorded in the DEB: 0 INPUT, 4 UPDAT */
static int openopt(DCB *dcb)
{
    unsigned char *deb = (unsigned char *)((unsigned)dcb->dcbdebad & 0x00FFFFFF);

    return deb ? deb[0x0C] & 0x0F : -1;
}

/* a SYNAD of the caller's: counts its calls and returns */
static void *mysynad(int **count) __attribute__((noinline));
static void *mysynad(int **count)
{
    void    *synad;
    void    *cnt;

    __asm__ __volatile__(
"         B     MYSYNE\n"
"MYSYN    BALR  15,0\n"
"         USING *,15\n"
"         L     1,MYSYNN\n"
"         LA    1,1(,1)\n"
"         ST    1,MYSYNN\n"
"         BR    14\n"
"         DS    0F\n"
"MYSYNN   DC    F'0'\n"
"         DROP  15\n"
"MYSYNE   LA    %0,MYSYN\n"
"         LA    %1,MYSYNN"
        : "=r"(synad), "=r"(cnt) : : "1", "15", "memory");
    *count = (int *)cnt;
    return synad;
}

/* reads DD dd with BSAM until oscheck() is not 0; the blocks read */
static int readall(DCB *dcb, int *rc)
{
    DECB    decb;
    int     n;

    for (n = 0; n < 10; n++) {
        memset(&decb, 0, sizeof(decb));
        osread(&decb, dcb, buf, sizeof(buf));
        *rc = oscheck(&decb);
        if (*rc) break;
    }
    return n;
}

/* a CHECK of the caller's own, not oscheck() */
static void owncheck(DECB *decb)
{
    __asm__("LR    1,%0\n\tCHECK (1)"
        : : "r"(decb) : "0", "1", "14", "15", "memory");
}

static void exopen(void)
{
    DCB     *dcb;
    int     rc;

    dcb = osxdcb("RO", NULL);
    rc = osxopenm(dcb, 0, "r");
    wtof("TSTOSEM   osxopenm(RO, \"r\") %d, open option %d", rc,
         rc ? -1 : openopt(dcb));
    want(rc == 0 && openopt(dcb) == 0, "EXCP read-only: OPEN INPUT");
    if (rc == 0) osxclose(dcb, 1);

    dcb = osxdcb("RO", NULL);
    rc = osxopen(dcb, 0);
    wtof("TSTOSEM   osxopen(RO) %d, open option %d", rc,
         rc ? -1 : openopt(dcb));
    want(rc == 0 && openopt(dcb) == 4, "control: osxopen() OPEN UPDAT");
    if (rc == 0) osxclose(dcb, 1);
}

/* BDAM: open DD RO with mode (NULL: osdopen()), read block 0 */
static void bdread(const char *mode, int wantopt)
{
    DCB     *dcb;
    DECB    decb;
    int     rc;

    dcb = osddcb("RO");
    rc = mode ? osdopenm(dcb, 0, mode) : osdopen(dcb, 0);
    wtof("TSTOSEM   osdopen%s(RO) %d, open option %d", mode ? "m" : "",
         rc, rc ? -1 : openopt(dcb));
    want(rc == 0 && openopt(dcb) == wantopt, mode
         ? "BDAM read-only: OPEN INPUT" : "control: osdopen() OPEN UPDAT");
    if (rc) return;
    memset(&decb, 0, sizeof(decb));
    memset(buf, 0, sizeof(buf));
    osdread(&decb, dcb, buf, 800, 0);
    rc = oscheck(&decb);
    wtof("TSTOSEM   BDAM read of block 0: %d, ECB %08X, \"%.8s\"", rc,
         decb.decsdecb, buf);
    want(rc == 0 && memcmp(buf, "RECORD 1", 8) == 0, "BDAM: block 0 read");
    osdclose(dcb, 1);
}

/* BDAM: block 0 read with a length of 400, a wrong-length record; with
   the library's SYNAD, then with one of the caller's */
static void bdwrong(int own)
{
    DCB     *dcb;
    DECB    decb;
    int     rc, *count = NULL;

    dcb = osddcb("RO");
    if (own) dcb->dcbsynad = mysynad(&count);
    if (count) *count = 0;
    rc = osdopenm(dcb, 0, "r");
    if (rc) {
        want(0, "BDAM open for the wrong-length read");
        return;
    }
    memset(&decb, 0, sizeof(decb));
    osdread(&decb, dcb, buf, 400, 0);
    rc = oscheck(&decb);
    wtof("TSTOSEM   BDAM wrong length%s: oscheck() %08X, ECB %08X, own "
         "SYNAD called %d times", own ? ", own SYNAD" : "", rc,
         decb.decsdecb, count ? *count : -1);
    want(rc == 8, own ? "BDAM I/O error, own SYNAD: oscheck() 8"
                      : "BDAM I/O error: oscheck() 8 through the SYNAD");
    if (own) want(*count == 1, "BDAM: the caller's own SYNAD is called");
    osdclose(dcb, 1);
}

static void bdopen(void)
{
    bdread("r", 0);
    bdread(NULL, 4);
    bdwrong(0);
    bdwrong(1);
}

static void eodsyn(void)
{
    DCB     *dcb;
    DECB    decb;
    int     rc, n, *count;

    dcb = osbdcb("SEQ", NULL);
    osbopen(dcb, 0, "r");
    n = readall(dcb, &rc);
    wtof("TSTOSEM   SEQ: %d blocks, then oscheck() %08X", n, rc);
    want(n == 2 && rc == 4, "end of data: oscheck() 4 through the EODAD");
    osbclose(dcb, NULL, 1, 0);

    dcb = osbdcb("WL", NULL);
    osbopen(dcb, 0, "r");
    n = readall(dcb, &rc);
    wtof("TSTOSEM   WL: %d blocks, then oscheck() %08X", n, rc);
    want(n == 0 && rc == 8, "I/O error: oscheck() 8 through the SYNAD");
    osbclose(dcb, NULL, 1, 0);

    dcb = osbdcb("WL", NULL);
    dcb->dcbsynad = mysynad(&count);
    *count = 0;
    osbopen(dcb, 0, "r");
    n = readall(dcb, &rc);
    wtof("TSTOSEM   WL, own SYNAD: %d blocks, oscheck() %08X, SYNAD "
         "called %d times", n, rc, *count);
    want(*count == 1, "the caller's own SYNAD is called");
    want(rc == 8, "after it, oscheck() 8");
    osbclose(dcb, NULL, 1, 0);

    dcb = osbdcb("SEQ", NULL);
    osbopen(dcb, 0, "r");
    for (n = 0; n < 3; n++) {
        memset(&decb, 0, sizeof(decb));
        osread(&decb, dcb, buf, sizeof(buf));
        rc = try(owncheck, &decb);
        if (rc) break;
    }
    wtof("TSTOSEM   SEQ, own CHECK: %d blocks, then %08X", n, rc);
    want(n == 2 && (rc & 0x00FFF000) == 0x00337000,
         "control: an own CHECK at end of data ends in S337");
    osbclose(dcb, NULL, 1, 0);
}

int main(void)
{
    exopen();
    bdopen();
    eodsyn();
    return bad ? 8 : 0;
}
