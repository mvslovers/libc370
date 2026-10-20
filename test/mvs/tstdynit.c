/*
 * tstdynit.c - libc370 #52 on MVS: <dynit.h>, z/OS's dynalloc(), dynfree()
 * and dyninit(), against SVC 99 of MVS 3.8j.
 *
 *   1 a new sequential data set IBMUSER.LIBC370.T52S: DISP=(NEW,CATLG,
 *     DELETE), UNIT=SYSDA, TRK(1,1), DSORG=PS, RECFM=FB, LRECL=80,
 *     BLKSIZE=800, no DD name -> MVS generates one, in __retddn; a record
 *     written through fopen("DD:...") and the DD freed by dynfree()
 *   2 the format-1 DSCB of T52S says PS, FB, 80, 800; LOCATE finds it
 *   3 T52S again with DISP=SHR under a DD name of the caller's: the
 *     record written in 1 reads back
 *   4 a new PDS T52P with a member MEMA (__member, __dirblk), __ddname
 *     "????????": the generated name comes back in __ddname itself; then
 *     the member allocated SHR reads back its record
 *   5 SYSOUT: class A with a DD name, and __DEF_CLASS; a line written to
 *     each (the job output shows them)
 *   6 a temporary data set (no name): __retdsn holds the name MVS gave it
 *   7 __CLOSE: the DD is gone after fclose()
 *   8 dynfree() by data set name: the DD that held it is gone
 *   9 errors: a data set that is not cataloged (SVC 99 rc 4 and its
 *     __errcode), dynfree() of a DD that does not exist, a data class
 *     (refused with -2 before SVC 99)
 *
 * T52S and T52P stay cataloged; the job lists them with IDCAMS LISTCAT
 * and prints them with IEBGENER as the second opinion, after deleting the
 * old ones in its first step.
 *
 * RED: there is none to run - against a library without <dynit.h> the
 * probe does not compile.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstdynit.c \
 *         -o $S/TSTDYNG -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTDYNG=TSTDYNG.iebcopy -o dy -xmit \
 *         --dsn IBMUSER.LIBC370.DYSCR
 *
 * then jcl/recvdynit.jcl and jcl/tstdynit.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdio.h>
#include <string.h>
#include <dynit.h>
#include <mvs/wto.h>
#include <mvs/dscb.h>
#include <mvs/dd.h>

#define SEQ     "IBMUSER.LIBC370.T52S"
#define PDS     "IBMUSER.LIBC370.T52P"

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTDYNIT %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void said(const char *what, int rc, const __dyn_t *d)
{
    wtof("TSTDYNIT   %s: rc %d, error %04X, info %04X", what, rc,
         (unsigned short)d->__errcode, (unsigned short)d->__infocode);
}

/* write one line to DD dd */
static int put(const char *dd, const char *line)
{
    char    name[12];
    FILE    *fp;
    int     ok;

    snprintf(name, sizeof(name), "DD:%s", dd);
    fp = fopen(name, "w");
    if (!fp) return 0;
    ok = fputs(line, fp) >= 0 && fputc('\n', fp) != EOF;
    return fclose(fp) == 0 && ok;
}

/* the first line of DD dd, its first 10 characters */
static int first(const char *dd, char *line)
{
    char    name[12];
    FILE    *fp;
    int     ok = 0;

    line[0] = 0;
    snprintf(name, sizeof(name), "DD:%s", dd);
    fp = fopen(name, "r");
    if (!fp) return 0;
    if (fgets(line, 11, fp)) ok = 1;
    fclose(fp);
    return ok;
}

static int freedd(const char *dd)
{
    __dyn_t d;
    char    name[9];
    int     rc;

    dyninit(&d);
    strcpy(name, dd);
    d.__ddname = name;
    rc = dynfree(&d);
    said("dynfree()", rc, &d);
    return rc;
}

static void blank44(char *dst, const char *dsn)
{
    size_t n = strlen(dsn);

    memset(dst, ' ', 44);
    memcpy(dst, dsn, n);
}

static void attributes(void)
{
    LOCWORK lw;
    DSCB    dscb;
    char    dsn[44];
    int     rc;

    blank44(dsn, SEQ);
    memset(&lw, 0, sizeof(lw));
    rc = __locate(dsn, &lw);
    wtof("TSTDYNIT   LOCATE %s: rc %d, volume %.6s", SEQ, rc, lw.volser);
    want(rc == 0, "the new data set is cataloged");
    if (rc) return;
    memset(&dscb, 0, sizeof(dscb));
    rc = __dscbdv(dsn, lw.volser, &dscb);
    wtof("TSTDYNIT   DSCB: rc %d, id %02X, DSORG %02X%02X, RECFM %02X, "
         "LRECL %u, BLKSIZE %u", rc, (unsigned char)dscb.dscb1.fmtid,
         dscb.dscb1.dsorg1, dscb.dscb1.dsorg2, dscb.dscb1.recfm,
         dscb.dscb1.lrecl, dscb.dscb1.blksz);
    want(rc == 0 && (unsigned char)dscb.dscb1.fmtid == 0xF1
         && dscb.dscb1.dsorg1 == 0x40 && dscb.dscb1.recfm == 0x90
         && dscb.dscb1.lrecl == 80 && dscb.dscb1.blksz == 800,
         "the DSCB: PS, FB, LRECL 80, BLKSIZE 800");
}

static void sequential(void)
{
    __dyn_t d;
    char    dd[9];
    char    line[12];
    int     rc;

    dyninit(&d);
    d.__dsname    = SEQ;
    d.__status    = __DISP_NEW;
    d.__normdisp  = __DISP_CATLG;
    d.__conddisp  = __DISP_DELETE;
    d.__unit      = "SYSDA";
    d.__dsorg     = __DSORG_PS;
    d.__recfm     = _FB_;
    d.__lrecl     = 80;
    d.__blksize   = 800;
    d.__alcunit   = __TRK;
    d.__primary   = 1;
    d.__secondary = 1;
    rc = dynalloc(&d);
    said("dynalloc() new PS", rc, &d);
    wtof("TSTDYNIT   __retddn %s", d.__retddn);
    want(rc == 0 && d.__retddn[0], "1 a new data set, the DD name generated");
    if (rc) return;
    want(put(d.__retddn, "SEQ LINE 1"), "1 a record written to it");
    want(freedd(d.__retddn) == 0, "1 dynfree() by DD name");

    attributes();

    dyninit(&d);
    d.__ddname = "T52DD";
    d.__dsname = SEQ;
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    said("dynalloc() SHR as T52DD", rc, &d);
    want(rc == 0 && get_dsab(NULL, "T52DD") != NULL,
         "3 the existing data set under the caller's DD name");
    if (rc) return;
    rc = first("T52DD", line);
    wtof("TSTDYNIT   first record: %s", rc ? line : "(none)");
    want(rc && strncmp(line, "SEQ LINE 1", 10) == 0, "3 the record reads back");
    strcpy(dd, "T52DD");
    want(freedd(dd) == 0, "3 dynfree()");
}

static void partitioned(void)
{
    __dyn_t d;
    char    dd[9];
    char    line[12];
    int     rc;

    dyninit(&d);
    strcpy(dd, "????????");
    d.__ddname    = dd;
    d.__dsname    = PDS;
    d.__member    = "MEMA";
    d.__status    = __DISP_NEW;
    d.__normdisp  = __DISP_CATLG;
    d.__conddisp  = __DISP_DELETE;
    d.__unit      = "SYSDA";
    d.__dsorg     = __DSORG_PO;
    d.__recfm     = _FB_;
    d.__lrecl     = 80;
    d.__blksize   = 800;
    d.__alcunit   = __TRK;
    d.__primary   = 1;
    d.__secondary = 1;
    d.__dirblk    = 2;
    rc = dynalloc(&d);
    said("dynalloc() new PDS(MEMA)", rc, &d);
    wtof("TSTDYNIT   __ddname now %s", dd);
    want(rc == 0 && strcmp(dd, "????????") != 0 && dd[0],
         "4 a new PDS, \"????????\" replaced by the generated name");
    if (rc) return;
    want(put(dd, "MEMA LINE 1"), "4 the member written");
    want(freedd(dd) == 0, "4 dynfree()");

    dyninit(&d);
    d.__dsname = PDS;
    d.__member = "MEMA";
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    said("dynalloc() PDS(MEMA) SHR", rc, &d);
    if (rc) {
        want(0, "4 the member allocated SHR");
        return;
    }
    rc = first(d.__retddn, line);
    wtof("TSTDYNIT   first record of the member: %s", rc ? line : "(none)");
    want(rc && strncmp(line, "MEMA LINE ", 10) == 0, "4 the member reads back");
    freedd(d.__retddn);
}

static void sysout(char cls, const char *dd, const char *what)
{
    __dyn_t d;
    char    name[9];
    int     rc;

    dyninit(&d);
    strcpy(name, dd);
    d.__ddname = name;
    d.__sysout = cls;
    rc = dynalloc(&d);
    said(what, rc, &d);
    want(rc == 0 && put(dd, "TSTDYNIT SYSOUT LINE"), what);
    if (rc == 0) freedd(dd);
}

static void temporary(void)
{
    __dyn_t d;
    int     rc;

    dyninit(&d);
    d.__status  = __DISP_NEW;
    d.__unit    = "SYSDA";
    d.__alcunit = __TRK;
    d.__primary = 1;
    rc = dynalloc(&d);
    said("dynalloc() temporary", rc, &d);
    wtof("TSTDYNIT   __retdsn %s", d.__retdsn);
    want(rc == 0 && d.__retdsn[0], "6 a temporary data set, its name returned");
    if (rc == 0) freedd(d.__retddn);
}

static void closing(void)
{
    __dyn_t d;
    char    line[12];
    int     rc;

    dyninit(&d);
    d.__dsname = SEQ;
    d.__status = __DISP_SHR;
    d.__misc_flags = __CLOSE;
    rc = dynalloc(&d);
    said("dynalloc() __CLOSE", rc, &d);
    if (rc) {
        want(0, "7 allocated with __CLOSE");
        return;
    }
    want(get_dsab(NULL, d.__retddn) != NULL, "7 the DD is there before the close");
    first(d.__retddn, line);
    want(get_dsab(NULL, d.__retddn) == NULL, "7 __CLOSE: the DD is gone after fclose()");
}

static void bydsname(void)
{
    __dyn_t d;
    int     rc;

    dyninit(&d);
    d.__ddname = "T52X";
    d.__dsname = SEQ;
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    if (rc) {
        said("dynalloc() SHR as T52X", rc, &d);
        want(0, "8 allocated");
        return;
    }
    dyninit(&d);
    d.__dsname = SEQ;
    rc = dynfree(&d);
    said("dynfree() by data set name", rc, &d);
    want(rc == 0 && get_dsab(NULL, "T52X") == NULL,
         "8 dynfree() by data set name: the DD is gone");
}

static void errors(void)
{
    __dyn_t d;
    int     rc;

    dyninit(&d);
    d.__dsname = "IBMUSER.LIBC370.T52NONE";
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    said("dynalloc() of a data set not cataloged", rc, &d);
    want(rc == 4 && (unsigned short)d.__errcode == 0x1708,
         "9 not cataloged: rc 4, __errcode 1708");

    rc = freedd("T52NODD");
    want(rc == 4, "9 dynfree() of a DD that is not there: rc 4");

    dyninit(&d);
    d.__ddname = "T52DC";
    d.__dsname = SEQ;
    d.__status = __DISP_SHR;
    d.__dataclass = "DCLAS";
    rc = dynalloc(&d);
    said("dynalloc() with a data class", rc, &d);
    want(rc == -2 && get_dsab(NULL, "T52DC") == NULL,
         "9 a data class: -2, nothing allocated");
}

int main(void)
{
    sequential();
    partitioned();
    sysout('A', "T52OUTA", "5 SYSOUT class A");
    sysout(__DEF_CLASS, "T52OUTD", "5 SYSOUT __DEF_CLASS");
    temporary();
    closing();
    bydsname();
    errors();
    wtof("TSTDYNIT %s", bad ? "FAILED" : "ALL PASSED");
    return bad ? 8 : 0;
}
