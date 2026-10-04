/*
 * tstdsnmt.c - libc370 #181: __dsalc() must not wait for the operator when
 * the caller names a unit or a volume.
 *
 * ISSUE #181 (duplicate #305): __dsalc() (src/mvs/dynalloc/@@dsalc.c) issued
 * its SVC 99 with rb99.flag1 = S99NOCNV and nothing else.  A request naming
 * a volume that is not mounted then does not fail: SVC 99 goes into
 * allocation recovery, IEF238D REPLY DEVICE NAME OR 'CANCEL', and the task
 * waits until an operator answers.  Measured through ftpd (mvslovers/ftpd#133,
 * SITE VOLUME=NOVOL9 then STOR: the worker thread stuck in __dsalcf()), and
 * through fopen() before #172 gave __fpnew() the same flag (JOB01082 waited,
 * JOB01084 with S99NOMNT returned at once).
 *
 * THE FIX: the shape #172 settled for __fpnew().  A request whose opts name
 * UNIT= or VOLSER= carries S99NOMNT ("do not mount volumes or consider
 * offline units"); one that names neither keeps the flag byte it always had.
 * The keyword MOUNT drops it again: a caller who wants a tape or a volume
 * mounted gets the operator's mount request, as before the fix.
 *
 * WHAT THIS TEST PINS
 * --------------------------------------------------------------------
 * It links and executes the REAL @@dsalc.c with the real text unit builders
 * behind it, and reads the request block's first flag byte at the point SVC
 * 99 would be issued.
 *
 *   (1)  DSN + DISP=SHR, nothing else      -> S99NOCNV alone, as before.
 *   (2)  a full DISP=NEW request, no unit or volser -> S99NOCNV alone.
 *   (3)  UNIT=SYSDA                         -> S99NOCNV | S99NOMNT.
 *   (4)  VOLSER=PUB001                      -> S99NOCNV | S99NOMNT.
 *   (5)  both, in lower case, the way ftpd's STOR writes them ("unit=%s;
 *        volser=%s" through __dsalcf())     -> S99NOCNV | S99NOMNT, and the
 *        call succeeds and returns the DDNAME.
 *   (6)  MOUNT with UNIT= and VOLSER=       -> S99NOCNV alone: the caller
 *        asked for the operator's mount (a tape, a volume to be mounted).
 *   (7)  "mount" in lower case, as the first keyword -> the same.
 *   (8)  a token that merely contains MOUNT ("NOMOUNT") is not the
 *        keyword: S99NOMNT stays.
 *
 * WHAT IT DOES NOT PIN - that needs the MVS probe (test/mvs/tstdsnmt.c):
 * that SVC 99 then really returns instead of raising IEF238D.  The probe
 * cannot see a wait from inside either; the job log is the evidence.
 *
 * ====================================================================
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
 *        -D'__asm__(...)=' -D'asm(x)=' -D__32BIT__ \
 *        -I ../../include -I ../.. -o tstdsnmt tstdsnmt.c
 *     ./tstdsnmt                                      # rc 0 when green
 *
 * The flags, the suppressed string.h and the recorder are tstfpunit.c's (and
 * through it tstfprls.c's, notes a-d).
 * ==================================================================== */

/* suppress string.h - see tstfprls.c note (a) */
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
char   *strchr(const char *, int);
char   *strstr(const char *, const char *);
char   *strtok(char *, const char *);
size_t  strspn(const char *, const char *);   /* src/internal/tok.h */
size_t  strcspn(const char *, const char *);
size_t  strlen(const char *);
int     strcmp(const char *, const char *);

#include <stdio.h>
#include <stdlib.h>
#include "mvs/dynalloc.h"
#include "mvs/crt.h"
#include "ext/array.h"

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same inline copy as tstfpunit.c.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
        fflush(NULL);                                                     \
    } while (0)

#define CHECK_EQ(got, want, msg)                                          \
    do {                                                                  \
        long g_ = (long)(got), w_ = (long)(want);                         \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }    \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
        fflush(NULL);                                                     \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * Host shims for the MVS services these TUs reach.
 * ------------------------------------------------------------------------ */
int *__errno(void)
{
    static int e;
    return &e;
}

static unsigned short isbuf_[256];
static short          tolow_[256];
static short          toup_[256];

unsigned short *__isbuf = isbuf_;
short          *__tolow = tolow_;
short          *__toup  = toup_;

static void ctype_init(void)
{
    int c;

    for (c = 0; c < 256; c++) {
        tolow_[c] = (short)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
        toup_[c]  = (short)((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c);
        isbuf_[c] = 0;
        if (c >= '0' && c <= '9') isbuf_[c] |= 0x0008U;   /* isdigit */
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            isbuf_[c] |= 0x0002U;                         /* isalpha */
        if (isbuf_[c] & 0x000AU) isbuf_[c] |= 0x0001U;    /* isalnum */
    }
}

/* __dsalc() saves and restores the CRT's strtok() pointer; any CRT will do */
static CLIBCRT crt_;

CLIBCRT *__crtget(void)
{
    return &crt_;
}

/* see tstfprls.c note (b): the real __txrecf() is EBCDIC-table driven */
int __txrecf(TXT99 ***txt99, const char *recfm)
{
    unsigned char type = 0x80;
    TXT99         *tu;

    if (!recfm) return 1;
    tu = NewTXT99(DALRECFM, 1, 1, (const char *)&type);
    if (!tu) return 1;
    return arrayadd(txt99, tu);
}

/* --------------------------------------------------------------------------
 * The recorder - tstfprls.c note (c): __dsalc() marks the last array slot by
 * ORing the high bit into a pointer cast to a 32-bit unsigned, which on a
 * 64-bit host truncates it.  So the slots are never dereferenced or freed
 * here; every text unit is remembered at creation and freed from that list.
 * ------------------------------------------------------------------------ */
#define MAX_TU 32

static TXT99 *tu_made[MAX_TU];
static int    tu_count;

static void record(TXT99 *tu)
{
    if (tu && tu_count < MAX_TU) tu_made[tu_count++] = tu;
}

static TXT99 *real_nwtx99(int dal, int count, int size, const char *text);
static TXT99 *real_nwtx9a(int dal, int count, char **array);

#define __nwtx99 real_nwtx99
#include "../../src/mvs/dynalloc/@@nwtx99.c"
#undef __nwtx99
#define __nwtx9a real_nwtx9a
#include "../../src/mvs/dynalloc/@@nwtx9a.c"
#undef __nwtx9a

TXT99 *__nwtx99(int dal, int count, int size, const char *text)
{
    TXT99 *tu = real_nwtx99(dal, count, size, text);

    record(tu);
    return tu;
}

TXT99 *__nwtx9a(int dal, int count, char **array)
{
    TXT99 *tu = real_nwtx9a(dal, count, array);

    record(tu);
    return tu;
}

void __frtx9a(TXT99 ***txt99)
{
    int n;

    for (n = 0; n < tu_count; n++) {
        free(tu_made[n]);
        tu_made[n] = NULL;
    }
    tu_count = 0;
    if (txt99 && *txt99) arrayfree(txt99);
}

/* --------------------------------------------------------------------------
 * The SVC 99 shim: keeps the flag byte, hands back a DDNAME.
 * ------------------------------------------------------------------------ */
static int svc99_calls;
static int svc99_flag1;

int __svc99(void *rb)
{
    RB99 *rb99 = (RB99 *)rb;

    svc99_calls++;
    svc99_flag1 = rb99->flag1;

    if (tu_count > 0 && tu_made[0]->dal == DALRTDDN) {
        memcpy(tu_made[0]->text, "SYS00001", 8);
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * The code under test.
 * ------------------------------------------------------------------------ */
#include "../../src/mvs/dynalloc/@@dsalc.c"

#include "../../src/mvs/dynalloc/@@txddn.c"
#include "../../src/mvs/dynalloc/@@txrddn.c"
#include "../../src/mvs/dynalloc/@@txdsn.c"
#include "../../src/mvs/dynalloc/@@txnew.c"
#include "../../src/mvs/dynalloc/@@txold.c"
#include "../../src/mvs/dynalloc/@@txmod.c"
#include "../../src/mvs/dynalloc/@@txshr.c"
#include "../../src/mvs/dynalloc/@@txdel.c"
#include "../../src/mvs/dynalloc/@@txcat.c"
#include "../../src/mvs/dynalloc/@@txkeep.c"
#include "../../src/mvs/dynalloc/@@txucat.c"
#include "../../src/mvs/dynalloc/@@txadel.c"
#include "../../src/mvs/dynalloc/@@txacat.c"
#include "../../src/mvs/dynalloc/@@txakee.c"
#include "../../src/mvs/dynalloc/@@txauca.c"
#include "../../src/mvs/dynalloc/@@txdcbd.c"
#include "../../src/mvs/dynalloc/@@txorg.c"
#include "../../src/mvs/dynalloc/@@txlrec.c"
#include "../../src/mvs/dynalloc/@@txbksz.c"
#include "../../src/mvs/dynalloc/@@txcyl.c"
#include "../../src/mvs/dynalloc/@@txtrk.c"
#include "../../src/mvs/dynalloc/@@txblk.c"
#include "../../src/mvs/dynalloc/@@txspac.c"
#include "../../src/mvs/dynalloc/@@txunit.c"
#include "../../src/mvs/dynalloc/@@txvols.c"

#include "../../src/ext/array/@@aradd.c"
#include "../../src/ext/array/@@arnew.c"
#include "../../src/ext/array/@@arcou.c"
#include "../../src/ext/array/@@arget.c"
#include "../../src/ext/array/@@arfre.c"

/* ------------------------------------------------------------------------ */
static int run(const char *opts, char *dd)
{
    tu_count    = 0;
    svc99_calls = 0;
    svc99_flag1 = -1;
    if (dd) dd[0] = 0;
    return __dsalc(dd, opts);
}

int main(void)
{
    char dd[9];
    int  rc;

    ctype_init();

    printf("=== TSTDSNMT - libc370 #181: __dsalc() and S99NOMNT ===\n");

    printf("\n(1) DSN + DISP=SHR -> S99NOCNV alone\n");
    rc = run("DSN=SYS1.MACLIB;DISP=SHR", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_calls, 1, "SVC 99 issued once");
    CHECK_EQ(svc99_flag1, S99NOCNV, "flag1 is S99NOCNV alone, as before");

    printf("\n(2) DISP=NEW, DCB and space, no unit or volser -> S99NOCNV alone\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG,DELETE);DSORG=PS;"
             "RECFM=FB;LRECL=80;BLKSIZE=800;SPACE=TRK(1,1)", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV, "flag1 is S99NOCNV alone, as before");

    printf("\n(3) UNIT=SYSDA -> S99NOMNT\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=TRK(1,1);"
             "UNIT=SYSDA", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "unit alone: S99NOMNT set");

    printf("\n(4) VOLSER=PUB001 -> S99NOMNT\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=TRK(1,1);"
             "VOLSER=PUB001", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "volser alone: S99NOMNT set");

    printf("\n(5) ftpd's STOR shape, lower case -> S99NOMNT, DDNAME back\n");
    rc = run("dsn=mvslove.test.new;disp=(new,catlg,delete);"
             "space=trk(30,5);unit=sysda;volser=novol9", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "unit and volser: S99NOMNT set");
    CHECK(strcmp(dd, "SYS00001") == 0, "the returned DDNAME is handed back");

    printf("\n(6) MOUNT with UNIT= and VOLSER= -> S99NOCNV alone\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);UNIT=TAPE;"
             "VOLSER=TAPE01;MOUNT", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV, "mount asked for: no S99NOMNT");

    printf("\n(7) mount in lower case, first -> S99NOCNV alone\n");
    rc = run("mount;dsn=mvslove.test.new;disp=old;volser=pub001", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV, "mount asked for: no S99NOMNT");

    printf("\n(8) a keyword merely containing MOUNT does not count\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=OLD;VOLSER=PUB001;NOMOUNT", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "NOMOUNT is not MOUNT");

    return mbt_test_summary("TSTDSNMT");
}
