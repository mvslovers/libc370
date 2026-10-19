/*
 * tstdynit.c - libc370 #52: <dynit.h>, z/OS's dynalloc()/dynfree()/dyninit().
 *
 * It links the REAL src/dynit sources and replaces only __svc99(), which
 * here reads the request instead of issuing it: the verb, the first flag
 * byte and every text unit, byte for byte as SVC 99 would see them.  The
 * request block is the first member of the request (src/dynit/dynreq.h),
 * so the stand-in finds the text unit list and its count from it, and the
 * last pointer from rq->last: a 64-bit host cannot follow a pointer with
 * X'80000000' ORed into it.
 *
 *   (1)  a new sequential data set: every field becomes its text unit,
 *        numbers big-endian in their SVC 99 widths (DSORG 2, RECFM 1,
 *        LRECL 2, BLKSIZE 2, quantities 3), no DD name -> DALRTDDN, which
 *        comes back trimmed in __retddn; S99NOCNV|S99NOMNT for a unit
 *   (2)  names in lower case go upper case; a 50-character name is cut
 *        to 44, as z/OS cuts it
 *   (3)  __ddname "????????" gets the generated name written into it
 *   (4)  SYSOUT: __DEF_CLASS is DALSYSOU without a value, a class is one
 *        byte; no DALRTDSN for SYSOUT
 *   (5)  a temporary data set (no name) asks for DALRTDSN, into __retdsn
 *   (6)  __CONTIG is DALSPFRM X'08', __CLOSE is DALCLOSE without a value
 *   (7)  refused before SVC 99: a quantity over 3 bytes, a data class, a
 *        path, NULL; ignored: a storage class
 *   (8)  SVC 99 fails: its return code, __errcode/__infocode, no __retddn
 *   (9)  __miscitems are passed on, the caller's high bit taken off
 *   (10) dynfree() by DD name, and by data set, member and overriding
 *        disposition; nothing named is refused
 *   (11) dyninit() clears the structure and is 0
 *
 * RED: against a tree without include/dynit.h and src/dynit/ the test does
 * not compile.  What only MVS can show - that SVC 99 accepts the units - is
 * test/mvs/tstdynit.c.
 *
 * ====================================================================
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -fsanitize=address -Wno-int-to-pointer-cast \
 *        -Wno-pointer-to-int-cast -D'__asm__(...)=' -D'asm(x)=' \
 *        -D__32BIT__ -I ../../include -I ../.. -o tstdynit tstdynit.c
 *     ./tstdynit                                      # rc 0 when green
 * ==================================================================== */
/* suppress string.h - its inline MVCL cannot build on the host
   (tstfprls.c note (a)) */
#define STRING_H
#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
typedef unsigned long size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
void   *memcpy(void *, const void *, size_t);
void   *memset(void *, int, size_t);
int     memcmp(const void *, const void *, size_t);
char   *strcpy(char *, const char *);
size_t  strlen(const char *);
int     strcmp(const char *, const char *);

#include <stdio.h>
#include <dynit.h>
#include "mvs/dynalloc.h"
#include "src/dynit/dynreq.h"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
        fflush(NULL);                                                     \
    } while (0)

/* ---- host shims ------------------------------------------------------ */
static int errno_;
int *__errno(void) { return &errno_; }

static short toup_[256];
short *__toup = toup_;

static void ctype_init(void)
{
    int c;

    for (c = 0; c < 256; c++)
        toup_[c] = (short)((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
}

#include "../../src/dynit/@@dynrq.c"
#include "../../src/dynit/dynalloc.c"
#include "../../src/dynit/dynfree.c"

/* ---- the SVC 99 stand-in --------------------------------------------- */
#define MAXSEEN 48

static struct { unsigned key, count, len; unsigned char v[64]; } seen[MAXSEEN];
static int      nseen, calls, verb, flag1;
static int      svc_rc;                 /* what the next "SVC" returns  */
static unsigned svc_err, svc_info;
static int      marked;                 /* last pointer had the bit     */

int __svc99(void *rb)
{
    DYNRQ           *rq = (DYNRQ *)rb;
    unsigned        i;
    unsigned char   *tu;

    calls++;
    verb  = rq->rb.request;
    flag1 = rq->rb.flag1;
    marked = ((unsigned long)rq->tu[rq->count - 1] & 0x80000000UL)
             && ((unsigned long)rq->tu[rq->count - 1] & ~(unsigned long)0x80000000UL)
                == ((unsigned long)rq->last & ~(unsigned long)0x80000000UL);
    for (nseen = 0, i = 0; i < rq->count && nseen < MAXSEEN; i++, nseen++) {
        tu = (i == rq->count - 1) ? rq->last : rq->tu[i];
        seen[nseen].key   = (tu[0] << 8) | tu[1];
        seen[nseen].count = (tu[2] << 8) | tu[3];
        seen[nseen].len   = seen[nseen].count ? ((tu[4] << 8) | tu[5]) : 0;
        memcpy(seen[nseen].v, tu + 6, seen[nseen].len);
        if (svc_rc) continue;
        if (seen[nseen].key == DALRTDDN && rq->rb.request == S99VRBAL)
            memcpy(tu + 6, "SYS00042", 8);
        if (seen[nseen].key == DALRTDSN)
            memcpy(tu + 6, "SYS26293.T101010.RA000.TSTDYNIT.R0000001    ", 44);
    }
    rq->rb.error = (short)svc_err;
    rq->rb.info  = (short)svc_info;
    return svc_rc;
}

static void reset(void)
{
    nseen = calls = verb = flag1 = marked = 0;
    svc_rc = 0;
    svc_err = svc_info = 0;
    errno_ = 0;
}

/* the text unit with this key, or NULL */
static int find(unsigned key)
{
    int i;

    for (i = 0; i < nseen; i++)
        if (seen[i].key == key) return i;
    return -1;
}

/* key present with this value (len bytes) */
static int has(unsigned key, const void *v, unsigned len)
{
    int i = find(key);

    return i >= 0 && seen[i].count == (len ? 1 : 0) && seen[i].len == len
        && (len == 0 || memcmp(seen[i].v, v, len) == 0);
}

int main(void)
{
    __dyn_t d;
    char    dd[9];
    int     rc;

    ctype_init();

    printf("(1) a new sequential data set\n");
    reset();
    dyninit(&d);
    d.__dsname    = "IBMUSER.T52.SEQ";
    d.__status    = __DISP_NEW;
    d.__normdisp  = __DISP_CATLG;
    d.__conddisp  = __DISP_DELETE;
    d.__unit      = "SYSDA";
    d.__dsorg     = __DSORG_PS;
    d.__recfm     = _FB_;
    d.__lrecl     = 80;
    d.__blksize   = 32720;
    d.__alcunit   = __TRK;
    d.__primary   = 70000;
    d.__secondary = 2;
    rc = dynalloc(&d);
    CHECK(rc == 0 && calls == 1, "dynalloc() 0, one SVC 99");
    CHECK(verb == S99VRBAL, "verb: allocation");
    CHECK(flag1 == (S99NOCNV | S99NOMNT), "S99NOCNV|S99NOMNT for a unit");
    CHECK(marked, "the last pointer has its high-order bit");
    CHECK(seen[0].key == DALRTDDN && seen[0].len == 8
          && memcmp(seen[0].v, "        ", 8) == 0,
          "no DD name: DALRTDDN first, 8 blanks");
    CHECK(strcmp(d.__retddn, "SYS00042") == 0, "__retddn is the generated DD");
    CHECK(has(DALDSNAM, "IBMUSER.T52.SEQ", 15), "DALDSNAM");
    CHECK(has(DALSTATS, "\x04", 1), "DALSTATS NEW X'04'");
    CHECK(has(DALNDISP, "\x02", 1), "DALNDISP CATLG X'02'");
    CHECK(has(DALCDISP, "\x04", 1), "DALCDISP DELETE X'04'");
    CHECK(has(DALUNIT, "SYSDA", 5), "DALUNIT");
    CHECK(has(DALDSORG, "\x40\x00", 2), "DALDSORG PS X'4000'");
    CHECK(has(DALRECFM, "\x90", 1), "DALRECFM FB X'90'");
    CHECK(has(DALLRECL, "\x00\x50", 2), "DALLRECL 80");
    CHECK(has(DALBLKSZ, "\x7F\xD0", 2), "DALBLKSZ 32720");
    CHECK(has(DALTRK, "", 0), "DALTRK without a value");
    CHECK(has(DALPRIME, "\x01\x11\x70", 3), "DALPRIME 70000 in 3 bytes");
    CHECK(has(DALSECND, "\x00\x00\x02", 3), "DALSECND 2");
    CHECK(find(DALRTDSN) < 0, "a named data set: no DALRTDSN");
    CHECK(find(DALDIR) < 0 && find(DALCYL) < 0, "nothing that was not set");

    printf("(2) upper case, and a name cut to 44\n");
    reset();
    dyninit(&d);
    d.__ddname = "mydd";
    d.__dsname = "ibmuser.a234567.b234567.c234567.d234567.e234567";
    d.__member = "mem1";
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    CHECK(rc == 0, "dynalloc() 0");
    CHECK(has(DALDDNAM, "MYDD", 4), "DALDDNAM in upper case");
    CHECK(has(DALDSNAM, "IBMUSER.A234567.B234567.C234567.D234567.E234", 44),
          "DALDSNAM upper case, cut to 44");
    CHECK(has(DALMEMBR, "MEM1", 4), "DALMEMBR");
    CHECK(flag1 == S99NOCNV, "no unit or volume: S99NOCNV alone");
    CHECK(find(DALRTDDN) < 0, "a DD name: no DALRTDDN");

    printf("(3) __ddname \"????????\"\n");
    reset();
    dyninit(&d);
    strcpy(dd, "????????");
    d.__ddname = dd;
    d.__dsname = "SYS1.MACLIB";
    d.__status = __DISP_SHR;
    rc = dynalloc(&d);
    CHECK(rc == 0 && find(DALRTDDN) == 0 && find(DALDDNAM) < 0,
          "\"????????\" asks for DALRTDDN");
    CHECK(strcmp(dd, "SYS00042") == 0, "the generated name in __ddname");

    printf("(4) SYSOUT\n");
    reset();
    dyninit(&d);
    d.__sysout = __DEF_CLASS;
    rc = dynalloc(&d);
    CHECK(rc == 0 && has(DALSYSOU, "", 0), "__DEF_CLASS: DALSYSOU, no value");
    CHECK(find(DALRTDSN) < 0, "SYSOUT: no DALRTDSN");
    reset();
    dyninit(&d);
    d.__sysout = 'A';
    d.__sysoutname = "intrdr";
    rc = dynalloc(&d);
    CHECK(rc == 0 && has(DALSYSOU, "A", 1), "class A: one byte");
    CHECK(has(DALSPGNM, "INTRDR", 6), "DALSPGNM");

    printf("(5) a temporary data set\n");
    reset();
    dyninit(&d);
    d.__status  = __DISP_NEW;
    d.__alcunit = __CYL;
    d.__primary = 1;
    d.__dirblk  = 5;
    d.__dsorg   = __DSORG_PO;
    rc = dynalloc(&d);
    CHECK(rc == 0 && find(DALRTDSN) >= 0, "no name: DALRTDSN");
    CHECK(strcmp(d.__retdsn, "SYS26293.T101010.RA000.TSTDYNIT.R0000001") == 0,
          "__retdsn, trailing blanks removed");
    CHECK(has(DALCYL, "", 0) && has(DALDIR, "\x00\x00\x05", 3),
          "DALCYL, DALDIR 5");

    printf("(6) __misc_flags\n");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__misc_flags = __CONTIG | __CLOSE | __RELEASE | __PERM;
    rc = dynalloc(&d);
    CHECK(rc == 0 && has(DALSPFRM, "\x08", 1), "__CONTIG: DALSPFRM X'08'");
    CHECK(has(DALCLOSE, "", 0) && has(DALRLSE, "", 0) && has(DALPERMA, "", 0),
          "DALCLOSE, DALRLSE, DALPERMA without a value");
    reset();
    dyninit(&d);
    d.__misc_flags = __DUMMY_DSN;
    rc = dynalloc(&d);
    CHECK(rc == 0 && has(DALDUMMY, "", 0) && find(DALRTDSN) < 0,
          "DUMMY: DALDUMMY, no DALRTDSN");

    printf("(7) refused before SVC 99, and ignored\n");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__primary = 0x1000000;
    rc = dynalloc(&d);
    CHECK(rc == -2 && calls == 0 && errno_ == EINVAL,
          "a quantity over 3 bytes: -2, EINVAL, no SVC 99");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__dataclass = "DCLAS";
    CHECK(dynalloc(&d) == -2 && calls == 0, "__dataclass: -2");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__pathname = "/u/x";
    CHECK(dynalloc(&d) == -2 && calls == 0, "__pathname: -2");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__alcunit = 3;
    CHECK(dynalloc(&d) == -2 && calls == 0, "__alcunit neither CYL nor TRK: -2");
    reset();
    CHECK(dynalloc(NULL) == -2 && dynfree(NULL) == -2 && calls == 0,
          "NULL: -2");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__storclass = "SCLAS";
    d.__mgntclass = "MCLAS";
    d.__dsntype = __DSNT_PDS;
    rc = dynalloc(&d);
    CHECK(rc == 0 && calls == 1 && find(DALSTCL) < 0 && find(DALMGCL) < 0,
          "storage and management class, DSNTYPE=PDS: ignored");

    printf("(8) SVC 99 fails\n");
    reset();
    dyninit(&d);
    d.__dsname = "NOT.THERE";
    d.__status = __DISP_SHR;
    svc_rc = 4;
    svc_err = 0x1708;
    svc_info = 2;
    rc = dynalloc(&d);
    CHECK(rc == 4, "the SVC 99 return code");
    CHECK((unsigned short)d.__errcode == 0x1708 && d.__infocode == 2,
          "__errcode X'1708', __infocode");
    CHECK(d.__retddn[0] == 0, "no __retddn after a failure");

    printf("(9) __miscitems\n");
    {
        static unsigned char t1[] = { 0x00, 0x1D, 0x00, 0x01, 0x00, 0x01, 3 };
        static unsigned char t2[] = { 0x00, 0x1B, 0x00, 0x01, 0x00, 0x03,
                                      0x00, 0x00, 0x10 };
        char *items[2];

        if (((unsigned long)t2 & 0x80000000UL) == 0) {
            items[0] = (char *)t1;
            items[1] = (char *)((unsigned long)t2 | 0x80000000UL);
            reset();
            dyninit(&d);
            d.__dsname = "A.B";
            d.__miscitems = items;
            rc = dynalloc(&d);
            CHECK(rc == 0 && has(0x001D, "\x03", 1), "first item passed on");
            CHECK(has(0x001B, "\x00\x00\x10", 3), "last item passed on");
            CHECK(seen[nseen - 1].key == 0x001B, "and it is the last unit");
        }
        else printf("  (skipped: the item's address has bit 31 set)\n");
    }

    printf("(10) dynfree()\n");
    reset();
    dyninit(&d);
    d.__ddname = "sys00042";
    rc = dynfree(&d);
    CHECK(rc == 0 && verb == S99VRBUN && flag1 == 0, "unallocation");
    CHECK(has(DUNDDNAM, "SYS00042", 8), "DUNDDNAM");
    CHECK(seen[nseen - 1].key == DUNUNALC && seen[nseen - 1].count == 0,
          "DUNUNALC last, without a value");
    reset();
    dyninit(&d);
    d.__dsname = "A.B";
    d.__member = "M";
    d.__normdisp = __DISP_DELETE;
    d.__sysout = 'A';
    rc = dynfree(&d);
    CHECK(rc == 0 && has(DUNDSNAM, "A.B", 3) && has(DUNMEMBR, "M", 1),
          "DUNDSNAM, DUNMEMBR");
    CHECK(has(DUNOVDSP, "\x04", 1), "DUNOVDSP DELETE");
    CHECK(find(DALSYSOU) < 0, "__sysout ignored");
    reset();
    dyninit(&d);
    CHECK(dynfree(&d) == -2 && calls == 0, "nothing named: -2");

    printf("(11) dyninit()\n");
    memset(&d, 0x55, sizeof(d));
    rc = dyninit(&d);
    CHECK(rc == 0 && d.__dsname == NULL && d.__primary == 0
          && d.__misc_flags == 0 && d.__retddn[0] == 0, "cleared, 0");

    printf("\n=== tstdynit: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed ? 1 : 0;
}
