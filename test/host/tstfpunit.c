/*
 * tstfpunit.c - libc370 #172: fopen() must be able to say where a new data
 * set goes, and must say nothing when the caller did not ask.
 *
 * ISSUE #172: __fpnew() (src/stdio/@@fpnew.c) built RTDDN, DSNAME, STATUS(NEW),
 * NDISP(CATLG), DSORG, RECFM, LRECL, BLKSIZE and the space keys - and never a
 * DALUNIT (0x0015) or DALVLSER (0x0010).  __txunit() and __txvols() exist and
 * __dsalc() uses them; __fpnew() never called them.  Every data set that
 * fopen(name, "w...") created therefore landed wherever SVC 99's default unit
 * put it, and the caller had no way to say otherwise: the mode string knew
 * RECFM=, LRECL=, BLKSIZE= and SPACE=, the environment DATASET_RECFM,
 * DATASET_LRECL, DATASET_BLKSIZE and DATASET_SPACE, and nothing else.
 *
 * THE FIX: the same two keywords __dsalc() takes, "unit=" and "volser=", in
 * the mode string, and DATASET_UNIT / DATASET_VOLSER in the environment.  The
 * mode string wins over the environment, as it does for the other four.
 *
 * S99NOMNT, WHEN A UNIT OR VOLUME IS NAMED: measured on mvsdev (JOB01082), a
 * volser that is not mounted does not fail the request - SVC 99 goes into
 * allocation recovery, IEF238D REPLY DEVICE NAME OR 'CANCEL', and the task
 * waits for the operator.  So a request that names either carries S99NOMNT
 * ("do not mount volumes or consider offline units"), and one that names
 * neither keeps the flag byte it always had.  Cases (1)-(6) check that byte.
 *
 * THE TRAP THE FIX HAS TO STEP AROUND: __txunit() and __txvols() return 1 -
 * failure - for a NULL or empty argument.  Calling them unconditionally makes
 * EVERY DISP=NEW fopen() fail.  Case (1) is the guard against exactly that,
 * and case (6) the same for an environment variable that is set but empty.
 *
 * WHAT THIS TEST PINS
 * --------------------------------------------------------------------
 * It links and executes the REAL @@fpmode.c and @@fpnew.c with the real text
 * unit builders behind them, and captures the text unit array at the point
 * SVC 99 would be issued.  Asserted:
 *
 *   (1)  no unit, no volser anywhere -> __fpnew() succeeds and builds neither
 *        DALUNIT nor DALVLSER.  The regression guard: every existing caller
 *        gets the request it got before.
 *   (2)  "unit=sysda" in the mode string -> DALUNIT, one entry, "SYSDA".
 *   (3)  "volser=pub001" -> DALVLSER, one entry, "PUB001".
 *   (4)  "volser=(pub001,pub002)" -> DALVLSER, two entries, in that order -
 *        the parentheses survive __fpnew()'s comma-to-';' rewrite.  This case
 *        found a second defect: the rewrite folded everything to upper case
 *        EXCEPT what stood between the parentheses, so the list reached SVC 99
 *        as pub001,pub002 (count right, text wrong).  It never showed before
 *        because the only parenthesised value in use was space=trk(30,5).
 *   (5)  DATASET_UNIT / DATASET_VOLSER alone -> both units, from the
 *        environment.
 *   (6)  DATASET_UNIT / DATASET_VOLSER set but EMPTY -> neither unit, and
 *        __fpnew() still succeeds.
 *   (7)  mode string and environment both set -> the mode string wins.
 *   (8)  unit and volser alongside every other keyword (DCB, space, rlse) ->
 *        none of the others is lost, and the end-of-list marker still sits on
 *        the last text unit.
 *   (9)  "mount" with unit and volser -> S99NOMNT NOT set: the caller asked
 *        for the operator's mount (#181).
 *
 * WHAT IT DOES NOT PIN - that needs the MVS probe, not a host run:
 *
 *   - that SVC 99 puts the data set on the volume it was given.  Allocate
 *     with "unit=sysda,volser=<a mounted volume>", then read the volume back
 *     from the catalog, not from the return code.
 *
 * ====================================================================
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
 *        -D'__asm__(...)=' -D'asm(x)=' -D__32BIT__ \
 *        -I ../../include -o tstfpunit tstfpunit.c
 *     ./tstfpunit                                     # rc 0 when green
 *
 * The flags, the suppressed string.h, the shimmed __txrecf(), the recorder
 * and the by-value snapshot are tstfprls.c's, for the reasons written down
 * there (notes a-d).  One addition: DALVLSER is built by __nwtx9a(), not by
 * __nwtx99(), so the recorder wraps both and remembers how many payload bytes
 * each unit carries - the snapshot copies those bytes, and cases (2)-(7)
 * compare the text.  The texts here are plain bytes copied through, so unlike
 * the RECFM and SPACE units their contents ARE meaningful on this host.
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
#include "ext/array.h"

extern int __fpmode(FILE *fp, const char *mode);
extern int __fpnew(FILE *fp);

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness, same inline copy as tsttxdsn.c.
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

/* --------------------------------------------------------------------------
 * The recorder: every text unit the code under test creates, in creation
 * order, with the number of bytes it carries after dal and count.
 * ------------------------------------------------------------------------ */
#define MAX_TU   32
#define MAX_TEXT 64

static TXT99 *tu_made[MAX_TU];
static int    tu_bytes[MAX_TU];
static int    tu_count;
static int    tu_overflow;

static void record(TXT99 *tu, int bytes)
{
    if (!tu) return;
    if (tu_count < MAX_TU) {
        tu_made[tu_count]  = tu;
        tu_bytes[tu_count] = bytes;
        tu_count++;
    }
    else tu_overflow++;
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

    record(tu, 2 + count * size);           /* size, then the text */
    return tu;
}

TXT99 *__nwtx9a(int dal, int count, char **array)
{
    TXT99 *tu = real_nwtx9a(dal, count, array);
    int    bytes = 0;
    int    i;

    for (i = 0; i < count; i++) {
        bytes += 2 + (array[i] ? (int)strlen(array[i]) : 0);
    }
    record(tu, bytes);                      /* (size, text) per entry */
    return tu;
}

/* see tstfprls.c note (c): free the recorded pointers, not the array slots */
void __frtx9a(TXT99 ***txt99)
{
    int n;

    for (n = 0; n < tu_count; n++) {
        free(tu_made[n]);
        tu_made[n] = NULL;
    }
    if (txt99 && *txt99) arrayfree(txt99);
}

/* --------------------------------------------------------------------------
 * The SVC 99 shim: snapshots the request by value, hands back a DDNAME.
 * ------------------------------------------------------------------------ */
static int svc99_calls;
static int svc99_ntu;
static int svc99_endmark;
static int svc99_identity;
static int svc99_flag1;             /* the request block's first flag byte */

static struct {
    unsigned      dal;
    unsigned      count;
    int           bytes;                    /* payload bytes copied below */
    unsigned char text[MAX_TEXT];           /* from &size on              */
} svc99_snap[MAX_TU];

int __svc99(void *rb)
{
    RB99   *rb99 = (RB99 *)rb;
    TXT99 **p    = (TXT99 **)rb99->txtptr;
    int     n;

    svc99_calls++;
    svc99_ntu      = tu_count;
    svc99_endmark  = 0;
    svc99_identity = 1;
    svc99_flag1    = rb99->flag1;

    for (n = 0; n < tu_count; n++) {
        int bytes = tu_bytes[n] < MAX_TEXT ? tu_bytes[n] : MAX_TEXT;

        svc99_snap[n].dal   = tu_made[n]->dal;
        svc99_snap[n].count = tu_made[n]->count;
        svc99_snap[n].bytes = bytes;
        memcpy(svc99_snap[n].text, &tu_made[n]->size, bytes);

        if (n < tu_count - 1) {
            if (p[n] != tu_made[n]) svc99_identity = 0;
        }
        else {
            unsigned long want = ((unsigned long)(size_t)tu_made[n]
                                  & 0xFFFFFFFFUL) | 0x80000000UL;
            if ((unsigned long)(size_t)p[n] == want) svc99_endmark = 1;
        }
    }

    if (tu_count > 0 && tu_made[0]->dal == DALRTDDN) {
        memcpy(tu_made[0]->text, "SYS00001", 8);
    }

    return 0;
}

/* see tstfprls.c note (b) */
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
 * The code under test.
 * ------------------------------------------------------------------------ */
#include "../../src/stdio/@@fpmode.c"
#include "../../src/stdio/@@fpnew.c"

#include "../../src/mvs/dynalloc/@@txrddn.c"
#include "../../src/mvs/dynalloc/@@txdsn.c"
#include "../../src/mvs/dynalloc/@@txnew.c"
#include "../../src/mvs/dynalloc/@@txcat.c"
#include "../../src/mvs/dynalloc/@@txrlse.c"
#include "../../src/mvs/dynalloc/@@txlrec.c"
#include "../../src/mvs/dynalloc/@@txbksz.c"
#include "../../src/mvs/dynalloc/@@txcyl.c"
#include "../../src/mvs/dynalloc/@@txtrk.c"
#include "../../src/mvs/dynalloc/@@txblk.c"
#include "../../src/mvs/dynalloc/@@txspac.c"
#include "../../src/mvs/dynalloc/@@txorg.c"
#include "../../src/mvs/dynalloc/@@txunit.c"
#include "../../src/mvs/dynalloc/@@txvols.c"

#include "../../src/ext/array/@@aradd.c"
#include "../../src/ext/array/@@arnew.c"
#include "../../src/ext/array/@@arcou.c"
#include "../../src/ext/array/@@arget.c"
#include "../../src/ext/array/@@arfre.c"

/* --------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------ */
static int find_dal(unsigned dal)
{
    int n;

    for (n = 0; n < svc99_ntu; n++) {
        if (svc99_snap[n].dal == dal) return n;
    }
    return -1;
}

static int has_dal(unsigned dal)
{
    return find_dal(dal) >= 0;
}

/* entry i (0-based) of a (size, text) list in snapshot n equals want */
static int entry_is(int n, int i, const char *want)
{
    const unsigned char *p   = svc99_snap[n].text;
    const unsigned char *end = p + svc99_snap[n].bytes;
    unsigned short       len;

    for (;;) {
        if (p + 2 > end) return 0;
        memcpy(&len, p, 2);     /* a host short, as __nwtx9a() stored it */
        if (p + 2 + len > end) return 0;
        if (i == 0) {
            return len == strlen(want) && memcmp(p + 2, want, len) == 0;
        }
        p += 2 + len;
        i--;
    }
}

static void dump(const char *what)
{
    int n;

    printf("    %s: SVC 99 issued %d time(s), %d text unit(s):",
           what, svc99_calls, svc99_ntu);
    for (n = 0; n < svc99_ntu; n++) printf(" %04X", svc99_snap[n].dal);
    printf("\n");
    fflush(NULL);
}

static _FILE fh;

static int setup(const char *mode)
{
    int            rc;
    unsigned char *p = (unsigned char *)&fh;
    size_t         i;

    for (i = 0; i < sizeof(fh); i++) p[i] = 0;

    rc = __fpmode(&fh, mode);
    if (rc == 0) strcpy(fh.dataset, "MVSLOVE.TEST.NEW");

    tu_count       = 0;
    tu_overflow    = 0;
    svc99_calls    = 0;
    svc99_ntu      = 0;
    svc99_endmark  = 0;
    svc99_identity = 0;
    svc99_flag1    = 0;
    return rc;
}

static void env_clear(void)
{
    unsetenv("DATASET_UNIT");
    unsetenv("DATASET_VOLSER");
    unsetenv("DATASET_RECFM");
    unsetenv("DATASET_LRECL");
    unsetenv("DATASET_BLKSIZE");
    unsetenv("DATASET_SPACE");
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    int rc;
    int n;

    ctype_init();
    env_clear();

    printf("=== TSTFPUNIT - libc370 #172: UNIT and VOLSER through fopen() ===\n");

    /* ---------------------------------------------------------------- */
    printf("\n(1) nothing asked for -> no DALUNIT, no DALVLSER\n");
    CHECK_EQ(setup("wb,recfm=fb,lrecl=80,space=trk(30,5)"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    CHECK_EQ(svc99_calls, 1, "SVC 99 issued once");
    CHECK(has_dal(DALSTATS), "DALSTATS (DISP=NEW) present");
    CHECK(has_dal(DALTRK), "DALTRK present");
    CHECK(!has_dal(DALUNIT), "no DALUNIT");
    CHECK(!has_dal(DALVLSER), "no DALVLSER");
    CHECK_EQ(svc99_flag1, S99NOCNV, "flag1 is S99NOCNV alone, as before");

    CHECK_EQ(setup("w"), 0, "__fpmode(\"w\") ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() with a bare \"w\" succeeded");
    CHECK(!has_dal(DALUNIT), "no DALUNIT");
    CHECK(!has_dal(DALVLSER), "no DALVLSER");
    CHECK_EQ(svc99_flag1, S99NOCNV, "flag1 is S99NOCNV alone, as before");

    /* ---------------------------------------------------------------- */
    printf("\n(2) \"unit=sysda\" -> DALUNIT SYSDA\n");
    CHECK_EQ(setup("wb,unit=sysda"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALUNIT);
    CHECK(n >= 0, "DALUNIT present");
    if (n >= 0) {
        CHECK_EQ(svc99_snap[n].count, 1, "DALUNIT count 1");
        CHECK(entry_is(n, 0, "SYSDA"), "DALUNIT text SYSDA");
    }
    CHECK(!has_dal(DALVLSER), "no DALVLSER");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "unit alone: S99NOMNT set");

    /* ---------------------------------------------------------------- */
    printf("\n(3) \"volser=pub001\" -> DALVLSER PUB001\n");
    CHECK_EQ(setup("wb,volser=pub001"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALVLSER);
    CHECK(n >= 0, "DALVLSER present");
    if (n >= 0) {
        CHECK_EQ(svc99_snap[n].count, 1, "DALVLSER count 1");
        CHECK(entry_is(n, 0, "PUB001"), "DALVLSER entry 1 PUB001");
    }
    CHECK(!has_dal(DALUNIT), "no DALUNIT");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "volser alone: S99NOMNT set");

    /* ---------------------------------------------------------------- */
    printf("\n(4) \"volser=(pub001,pub002)\" -> DALVLSER, two entries\n");
    CHECK_EQ(setup("wb,volser=(pub001,pub002),lrecl=80"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALVLSER);
    CHECK(n >= 0, "DALVLSER present");
    if (n >= 0) {
        CHECK_EQ(svc99_snap[n].count, 2, "DALVLSER count 2");
        CHECK(entry_is(n, 0, "PUB001"), "DALVLSER entry 1 PUB001");
        CHECK(entry_is(n, 1, "PUB002"), "DALVLSER entry 2 PUB002");
    }
    CHECK(has_dal(DALLRECL), "the keyword after the list still parsed");

    /* ---------------------------------------------------------------- */
    printf("\n(5) DATASET_UNIT / DATASET_VOLSER alone\n");
    setenv("DATASET_UNIT", "3350", 1);
    setenv("DATASET_VOLSER", "WORK00", 1);
    CHECK_EQ(setup("wb"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALUNIT);
    CHECK(n >= 0 && entry_is(n, 0, "3350"), "DALUNIT 3350 from the environment");
    n = find_dal(DALVLSER);
    CHECK(n >= 0 && entry_is(n, 0, "WORK00"),
          "DALVLSER WORK00 from the environment");
    CHECK_EQ(svc99_flag1, S99NOCNV | S99NOMNT, "S99NOMNT set");
    env_clear();

    /* ---------------------------------------------------------------- */
    printf("\n(6) DATASET_UNIT / DATASET_VOLSER set but empty\n");
    setenv("DATASET_UNIT", "", 1);
    setenv("DATASET_VOLSER", "", 1);
    CHECK_EQ(setup("wb"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    CHECK(!has_dal(DALUNIT), "no DALUNIT");
    CHECK(!has_dal(DALVLSER), "no DALVLSER");
    CHECK_EQ(svc99_flag1, S99NOCNV, "empty values: no S99NOMNT");
    env_clear();

    /* ---------------------------------------------------------------- */
    printf("\n(7) mode string and environment -> the mode string wins\n");
    setenv("DATASET_UNIT", "3350", 1);
    setenv("DATASET_VOLSER", "WORK00", 1);
    CHECK_EQ(setup("wb,unit=sysda,volser=pub001"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALUNIT);
    CHECK(n >= 0 && entry_is(n, 0, "SYSDA"), "DALUNIT SYSDA, not 3350");
    n = find_dal(DALVLSER);
    CHECK(n >= 0 && entry_is(n, 0, "PUB001"), "DALVLSER PUB001, not WORK00");
    env_clear();

    /* ---------------------------------------------------------------- */
    printf("\n(8) alongside every other keyword\n");
    CHECK_EQ(setup("wb,recfm=fb,lrecl=80,blksize=800,space=trk(30,5),"
                   "unit=sysda,volser=pub001,rlse"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    CHECK(has_dal(DALRTDDN), "DALRTDDN present");
    CHECK(has_dal(DALDSNAM), "DALDSNAM present");
    CHECK(has_dal(DALSTATS), "DALSTATS present");
    CHECK(has_dal(DALNDISP), "DALNDISP present");
    CHECK(has_dal(DALDSORG), "DALDSORG present");
    CHECK(has_dal(DALRECFM), "DALRECFM present");
    CHECK(has_dal(DALLRECL), "DALLRECL present");
    CHECK(has_dal(DALBLKSZ), "DALBLKSZ present");
    CHECK(has_dal(DALTRK), "DALTRK present");
    CHECK(has_dal(DALPRIME), "DALPRIME present");
    CHECK(has_dal(DALRLSE), "DALRLSE present");
    CHECK(has_dal(DALUNIT), "DALUNIT present");
    CHECK(has_dal(DALVLSER), "DALVLSER present");
    CHECK(svc99_identity, "array slots are the units that were built");
    CHECK(svc99_endmark, "high-order bit set on the LAST slot");
    CHECK_EQ(tu_overflow, 0, "no text unit went unrecorded");

    /* ---------------------------------------------------------------- */
    printf("\n(9) \"mount\" with a volser -> no S99NOMNT (#181)\n");
    CHECK_EQ(setup("wb,unit=tape,volser=tape01,mount"), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    n = find_dal(DALVLSER);
    CHECK(n >= 0 && entry_is(n, 0, "TAPE01"), "DALVLSER TAPE01");
    CHECK_EQ(svc99_flag1, S99NOCNV, "mount: S99NOCNV alone");

    /* ---------------------------------------------------------------- */
    printf("\n(10) the caller's strtok() survives __fpnew()\n");
    {
        char  names[] = "ONE,TWO,THREE";
        char *t = strtok(names, ",");

        CHECK_EQ(setup("wb,recfm=fb,lrecl=80,space=trk(1,1),volser=pub001"),
                 0, "__fpmode() ok");
        rc = __fpnew(&fh);
        CHECK_EQ(rc, 0, "__fpnew() succeeded");
        t = strtok(NULL, ",");
        CHECK(t && strcmp(t, "TWO") == 0, "the next token is TWO");
    }

    return mbt_test_summary("TSTFPUNIT");
}
