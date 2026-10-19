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
 * And #408, read from the text units __dsalc() built:
 *
 *   (9)-(11) DISP=(..,UNCATLG,UNCATLG) and (OLD,UNCAT) uncatalog; they
 *        cataloged (strstr() found "CAT" first).  CATLG is the control.
 *   (12)-(15) SPACE=(TRK,(1,2)), (CYL,(5,2,10)) and (800,(10,5)), the JCL
 *        forms, give the unit and the quantities; they failed in the
 *        blocks branch.  TRK(3,4) is the control.
 *   (16)-(18) DSN=A.B(MEM) gives a DALMEMBR unit "MEM" and a DALDSNAM
 *        unit "A.B"; the red made it the name "A.B=MEM".  DSN= with a
 *        member beside a JCL-form SPACE=, and DSNAME= in lower case.
 *   (19)-(20) FREE=CLOSE adds the DALCLOSE unit; without it, none.  The
 *        red ignored the keyword.
 * Not here: opts NULL no longer going on with the caller's strtok() - the
 * only text unit is then also the last, whose pointer __dsalc() marks with
 * X'80000000', which a 64-bit host cannot follow.
 *
 * WHAT IT DOES NOT PIN - that needs the MVS probe (test/mvs/tstdsnmt.c):
 * that SVC 99 then really returns instead of raising IEF238D.  The probe
 * cannot see a wait from inside either; the job log is the evidence.
 *
 * ====================================================================
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -fsanitize=address -Wno-int-to-pointer-cast \
 *        -Wno-pointer-to-int-cast \
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
void   *memmove(void *, const void *, size_t);
char   *strchr(const char *, int);
char   *strstr(const char *, const char *);
char   *strtok(char *, const char *);
size_t  strspn(const char *, const char *);   /* src/internal/tok.h */
size_t  strcspn(const char *, const char *);
size_t  strlen(const char *);
char   *strpbrk(const char *, const char *);
int     strcmp(const char *, const char *);
int     strncmp(const char *, const char *, size_t);

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

/* the text units as SVC 99 would have seen them: __dsalc() frees them
   before it returns */
static struct { int dal; int size; unsigned char text[48]; } seen[MAX_TU];
static int seen_n;

int __svc99(void *rb)
{
    RB99 *rb99 = (RB99 *)rb;
    int  n;

    svc99_calls++;
    svc99_flag1 = rb99->flag1;
    for (seen_n = 0, n = 0; n < tu_count && seen_n < MAX_TU; n++) {
        if (!tu_made[n]) continue;
        seen[seen_n].dal = tu_made[n]->dal;
        seen[seen_n].size = tu_made[n]->count ? tu_made[n]->size : -1;
        memset(seen[seen_n].text, 0, sizeof(seen[0].text));
        memcpy(seen[seen_n].text, tu_made[n]->text,
               tu_made[n]->count && tu_made[n]->size < sizeof(seen[0].text)
               ? tu_made[n]->size : (tu_made[n]->count ? sizeof(seen[0].text) : 0));
        seen_n++;
    }

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
/* the strings __dsalc() hands to the space builders (#408): their 3-byte
   quantities are the low bytes of an int on S/370, always zero on this
   little-endian host, so the arguments are what can be checked */
static char got_spac[32], got_blk[32];
#define __txblk real_txblk
#include "../../src/mvs/dynalloc/@@txblk.c"
#undef __txblk
#define __txspac real_txspac
#include "../../src/mvs/dynalloc/@@txspac.c"
#undef __txspac
int __txblk(TXT99 ***txt99, const char *blocks)
{
    snprintf(got_blk, sizeof(got_blk), "%s", blocks ? blocks : "(null)");
    return real_txblk(txt99, blocks);
}
int __txspac(TXT99 ***txt99, const char *space)
{
    snprintf(got_spac, sizeof(got_spac), "%s", space ? space : "(null)");
    return real_txspac(txt99, space);
}
#include "../../src/mvs/dynalloc/@@txunit.c"
#include "../../src/mvs/dynalloc/@@txvols.c"
#include "../../src/mvs/dynalloc/@@txclos.c"

#include "../../src/ext/array/@@aradd.c"
#include "../../src/ext/array/@@arnew.c"
#include "../../src/ext/array/@@arcou.c"
#include "../../src/ext/array/@@arget.c"
#include "../../src/ext/array/@@arfre.c"

/* the text unit with key dal that __dsalc() made, its first byte, and its
   3-byte quantity the way __txspac()/__txblk() store it */
static unsigned char *tufind(int dal)
{
    int n;

    for (n = 0; n < seen_n; n++) {
        if (seen[n].dal == dal) return seen[n].text;
    }
    return NULL;
}

/* the text unit with key dal holds exactly the characters want */
static int tuis(int dal, const char *want)
{
    int n;

    for (n = 0; n < seen_n; n++) {
        if (seen[n].dal != dal) continue;
        return seen[n].size == (int)strlen(want)
            && memcmp(seen[n].text, want, strlen(want)) == 0;
    }
    return 0;
}

static int tuval(int dal)
{
    unsigned char *t = tufind(dal);

    return t ? t[0] : -1;
}

/* ------------------------------------------------------------------------ */
static int run(const char *opts, char *dd)
{
    tu_count    = 0;
    seen_n      = 0;
    got_spac[0] = got_blk[0] = 0;
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

    /* #408 ------------------------------------------------------------ */
    printf("\n#408 (9) DISP=(NEW,UNCATLG,UNCATLG) -> uncatalog, both\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,UNCATLG,UNCATLG);SPACE=TRK(1,1)", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tuval(DALNDISP) == 0x01, "normal disposition UNCATLG (X'01'), not CATLG");
    CHECK(tuval(DALCDISP) == 0x01, "abnormal disposition UNCATLG (X'01')");

    printf("\n(10) DISP=(OLD,UNCAT) -> uncatalog\n");
    rc = run("DSN=MVSLOVE.TEST.OLD;DISP=(OLD,UNCAT)", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tuval(DALNDISP) == 0x01, "UNCAT is X'01'");

    printf("\n(11) control: DISP=(NEW,CATLG,DELETE)\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG,DELETE);SPACE=TRK(1,1)", dd);
    CHECK(tuval(DALNDISP) == 0x02, "CATLG is X'02'");

    printf("\n(12) SPACE=(TRK,(1,2)), the JCL form\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=(TRK,(1,2))", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tufind(DALTRK) != NULL, "tracks");
    CHECK(strcmp(got_spac, "1,2") == 0, "quantities \"1,2\"");

    printf("\n(13) SPACE=(CYL,(5,2,10))\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=(CYL,(5,2,10))", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tufind(DALCYL) != NULL, "cylinders");
    CHECK(strcmp(got_spac, "5,2,10") == 0, "quantities \"5,2,10\"");

    printf("\n(14) SPACE=(800,(10,5)), blocks\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=(800,(10,5))", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(strcmp(got_blk, "800") == 0, "block length \"800\"");
    CHECK(strcmp(got_spac, "10,5") == 0, "quantities \"10,5\"");

    printf("\n(15) control: SPACE=TRK(3,4)\n");
    rc = run("DSN=MVSLOVE.TEST.NEW;DISP=(NEW,CATLG);SPACE=TRK(3,4)", dd);
    CHECK(tufind(DALTRK) != NULL && strcmp(got_spac, "3,4") == 0,
          "tracks, quantities \"3,4\"");

    printf("\n(16) DSN=A.B(MEM) -> member unit and the name alone\n");
    rc = run("DSN=SYS1.PROC(TSO);DISP=SHR", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tuis(DALMEMBR, "TSO"), "DALMEMBR is \"TSO\"");
    CHECK(tuis(DALDSNAM, "SYS1.PROC"), "DALDSNAM is \"SYS1.PROC\"");

    printf("\n(17) DSN= with a member and SPACE=(TRK,(1,2))\n");
    rc = run("DSN=A.B(MEMBER1);DISP=(NEW,CATLG);SPACE=(TRK,(1,2,1))", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tuis(DALMEMBR, "MEMBER1"), "DALMEMBR is \"MEMBER1\"");
    CHECK(tuis(DALDSNAM, "A.B"), "DALDSNAM is \"A.B\"");
    CHECK(strcmp(got_spac, "1,2,1") == 0, "quantities \"1,2,1\"");

    printf("\n(18) dsname= in lower case, comma separated\n");
    rc = run("dsname=a.b(x),disp=shr", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tuis(DALMEMBR, "X"), "DALMEMBR is \"X\"");
    CHECK(tuis(DALDSNAM, "A.B"), "DALDSNAM is \"A.B\"");
    CHECK(tuval(DALSTATS) == 0x08, "DISP=SHR (X'08') still parsed");

    printf("\n(19) FREE=CLOSE -> DALCLOSE\n");
    rc = run("DSN=A.B;DISP=SHR;free=close", dd);
    CHECK_EQ(rc, 0, "__dsalc() succeeds");
    CHECK(tufind(DALCLOSE) != NULL, "a DALCLOSE unit");

    printf("\n(20) control: no FREE=CLOSE, no DALCLOSE\n");
    rc = run("DSN=A.B;DISP=SHR", dd);
    CHECK(tufind(DALCLOSE) == NULL, "no DALCLOSE unit");

    return mbt_test_summary("TSTDSNMT");
}
