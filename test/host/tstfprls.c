/*
 * tstfprls.c - libc370 #167: fopen() must be able to ask for RLSE, and must
 * only ask when the caller said so.
 *
 * ISSUE #167: __txrlse() (src/clib/@@txrlse.c) builds a DALRLSE (0x000D) text
 * unit and is declared in include/svc99.h - and nothing in the library ever
 * called it.  There was therefore no way to get unused space released for a
 * data set written through fopen(), which is what mvslovers/ftpd#100 and
 * ftpd#127 need: an FTP STOR has no size at allocation time, so allocating
 * large enough for a big upload strands that space on every small one.
 *
 * WHY THE FIX IS IN fopen() AND NOT IN __dsalc():
 *
 *   RLSE is a JFCB attribute honoured at CLOSE of the DCB opened against the
 *   DD that carried it.  fclose() runs __aclose(fp->dcb) BEFORE __fpfree()
 *   drops the DD (src/clib/fclose.c) - so the DD that fopen() allocated is
 *   still there when CLOSE looks, and an attribute set on it takes effect.
 *   ftpd, by contrast, allocates with __dsalcf(), __dsfree()s that DD, and
 *   then fopen()s the data set by name: whatever __dsalc() put on the first
 *   DD is gone before anything opens anything.  An RLSE keyword in the opts
 *   parser would be a no-op for exactly the caller that asked for this.
 *
 * WHAT THIS TEST PINS
 * --------------------------------------------------------------------
 * It links and executes the REAL @@fpmode.c, @@fpold.c and @@fpnew.c with the
 * real text-unit builders behind them, and captures the text unit array at the
 * point SVC 99 would be issued.  Asserted:
 *
 *   (1)  "wb"           -> __fpold() builds RTDDN, DSNAME, STATS(OLD) and no
 *                          DALRLSE.  The regression guard: the behaviour of
 *                          every existing consumer is unchanged.
 *   (2)  "wb,rlse"      -> __fpold() builds DALRLSE, count 0, size 0.
 *   (3)  "wb,rlse" with a member name -> no DALRLSE.  fopen() tries __fpshr()
 *                          for a PDS member and FALLS THROUGH to __fpold() if
 *                          that fails, so the flag alone would put partial
 *                          release on a PO data set - taking the space the
 *                          next member needs.  The caller is not asked to
 *                          know that; the library skips it.
 *   (4)  "wb"           -> __fpnew() builds no DALRLSE.
 *   (5)  "wb,rlse"      -> __fpnew() builds DALRLSE alongside the DCB and
 *                          space units, and does not disturb them.
 *   (6)  __fpmode() sets _FILE_FLAG_RLSE for "rlse" and only for "rlse", and
 *                          coexists with the other comma options (record).
 *   (7)  the end-of-list marker is still set on the LAST text unit after the
 *                          new one is appended - i.e. DALRLSE did not land
 *                          past the high-order bit that tells SVC 99 where
 *                          the array stops.
 *
 * WHAT IT DOES NOT PIN - both need the MVS probe, not a host run:
 *
 *   - that SVC 99 ACCEPTS DALRLSE in this request shape.  The JCL equivalent
 *     (DISP=OLD,SPACE=(,,RLSE)) is valid, but __fpold() sends DALRLSE with no
 *     space keys at all, and only a real SVC 99 can answer whether that comes
 *     back with S99ERROR=0.  That is ftpd's exact shape, so it is the one
 *     that has to be measured.
 *   - that MVS then RELEASES the space at CLOSE.  Allocate TRK(30,5), write
 *     one record, fclose(), and compare the retained tracks against a control
 *     run without "rlse".
 *
 * TWO SHARP EDGES OF THE KEYWORD, neither of them a defect
 * --------------------------------------------------------------------
 * APPEND.  Case (6) pins that "ab,rlse" is accepted, and accepting it is
 * right - a caller may well want the last append trimmed.  But RLSE is what
 * makes append expensive: every fclose() gives the unused primary back, so
 * every following append has to take a SECONDARY extent, and a data set gets
 * 16 of those on one volume.  Repeated append-with-RLSE walks it into
 * DS1NOEPV exhaustion (x37) at a rate the caller chose.  Say it out loud
 * rather than leave it to be discovered; it is also precisely why RLSE is not
 * the default.
 *
 * WHERE THE KEYWORD APPLIES.  __fpmode() sets _FILE_FLAG_RLSE for ANY mode
 * string containing "rlse", but only __fpold() and __fpnew() act on it.  It
 * is silently ignored for:
 *
 *   - read opens                     (__fpshr, DISP=SHR)
 *   - "&TEMP" data sets              (__fptmp, VIO)
 *   - "DD:ddname"                    (no allocation - the DD already exists)
 *   - "*" / "*ddname" SYSOUT         (__fpstar)
 *   - PDS members                    (__fpshr; and deliberately skipped in
 *                                     __fpold/__fpnew - see case (3))
 *
 * Only the first of those is even arguable: RLSE on a DISP=SHR input open
 * would be honoured by CLOSE for a data set nobody is writing, which is not
 * something a C runtime should do behind an "r" mode.
 *
 * ====================================================================
 * BUILD AND RUN (host, from test/host)
 *
 *   -D'__asm__(...)=' erases the file-scope S/370 assembler statements in the
 *                     array TUs (__asm__("\n&FUNC SETC 'arrayadd'")), which
 *                     the host assembler rejects.  Variadic because some of
 *                     them are extended asm with operand lists.  It does NOT
 *                     touch the asm("@@ARADD") labels in clibary.h - different
 *                     spelling - so host symbols keep the library's names.
 *   -D'asm(x)='       erases the asm("@@ARADD") symbol labels in clibary.h.
 *                     Not needed on macOS/clang, REQUIRED on Linux with GNU
 *                     as: '@' is not valid in a symbol name in a .size/.type
 *                     directive.  Erasing them costs nothing here - the host
 *                     symbols then carry their C names, declaration and
 *                     definition lose the label together.
 *   -D__32BIT__       is what libc370's own stddef.h/stdlib.h key size_t off.
 *                     Without it size_t is an unknown type and every compile
 *                     against -I include fails.
 *
 *     cc -std=gnu99 -Wall -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
 *        -D'__asm__(...)=' -D'asm(x)=' -D__32BIT__ \
 *        -I ../../include -o tstfprls tstfprls.c
 *     ./tstfprls                                      # rc 0 when green
 *
 * Verified green both ways on macOS/clang (51/51, rc 0).
 *
 * NO -fsanitize=address, and not by preference: @@fpnew.c rewrites its mode
 * string in place with strcpy(p, p+1) (lines 43 and 47), which is an
 * overlapping copy - undefined in C, benign with a forward byte-at-a-time
 * strcpy, and an immediate abort under ASAN.  @@dsalc.c does the same.  It
 * predates #167 and is not touched here.
 *
 * FOUR HOST-PORT DETAILS, none of them guessable:
 *
 *   a) clibstr.h is SUPPRESSED (#define CLIBSTR_H below) and the handful of
 *      string functions declared here instead.  Its memset() and memclr() are
 *      static __inline S/370 assembler; -D'__asm__(...)=' does not reach them
 *      (they are written __asm__ __volatile__(...), and a function-like macro
 *      only expands when the next token is '('), so any TU that CALLS memset -
 *      @@fpmode.c and @@fpnew.c both do - fails in the assembler.  Suppressing
 *      the header gives them the host's real memset, which is what the test
 *      wants anyway.
 *
 *   b) @@txrecf.c is NOT linked in, it is shimmed.  It folds case with
 *      (c | ' '), which is OR 0x40 in EBCDIC - a fold to UPPER, correct on the
 *      target - and OR 0x20 in ASCII, a fold to lower, so its uppercase table
 *      matches nothing here and __fpnew() fails at the RECFM unit before it
 *      ever reaches the space keys.  Related: @@txspac.c takes the low three
 *      bytes of an int as &p[1], which is the big-endian view.  The text unit
 *      CONTENTS are therefore not meaningful on this host; presence and order
 *      are, and that is all this file asserts (plus count/size on DALRLSE,
 *      which are literals).
 *
 *   c) THE END-OF-LIST MARKER TRUNCATES ON A 64-BIT HOST.  @@fpold.c and
 *      @@fpnew.c set it with (TXT99*)((unsigned)txt99[count] | 0x80000000),
 *      which is correct on the 24-bit target and lops the top 32 bits off a
 *      macOS heap pointer.  The last array element is therefore unreadable
 *      here.  That is why this file wraps __nwtx99() to record every text unit
 *      it hands out, reads the DAL keys from those recordings, and checks the
 *      last array slot by ARITHMETIC against the recorded pointer (case 7)
 *      rather than by dereferencing it.  It is also why __frtx9a() is shimmed:
 *      the real one would free() the truncated pointer.
 *
 *   d) The request is snapshotted BY VALUE at the SVC 99 call.  __fpold() and
 *      __fpnew() run FreeTXT99Array() on the way out, so every TXT99 is freed
 *      before the assertions run.
 * ==================================================================== */

/* (a) suppress clibstr.h - see the note above */
#define CLIBSTR_H
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
size_t  strlen(const char *);
int     strcmp(const char *, const char *);

#include <stdio.h>
#include <stdlib.h>
#include "svc99.h"
#include "clibary.h"

/* Pre-fix fallback, so this file COMPILES and RUNS red against the library as
 * it was: without the fix the bit is simply never set, and cases (2), (3), (5)
 * and (6) fail as assertions rather than as a build error. */
#ifndef _FILE_FLAG_RLSE
#define _FILE_FLAG_RLSE 0x0040
#endif

extern int __fpmode(FILE *fp, const char *mode);
extern int __fpold(FILE *fp);
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

/* libc370's ctype.h is three table lookups; build the tables for the host's
 * ASCII, which is all the mode/opts strings in this test are. */
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
 * The recorder.  @@nwtx99.c is included under a rename so every text unit the
 * code under test creates passes through here first, in creation order - see
 * note (c): the last array slot cannot be read back on this host.
 * ------------------------------------------------------------------------ */
#define MAX_TU 32

static TXT99 *tu_made[MAX_TU];
static int    tu_count;
static int    tu_overflow;

static TXT99 *real_nwtx99(int dal, int count, int size, const char *text);

#define __nwtx99 real_nwtx99
#include "../../src/clib/@@nwtx99.c"
#undef __nwtx99

TXT99 *__nwtx99(int dal, int count, int size, const char *text)
{
    TXT99 *tu = real_nwtx99(dal, count, size, text);

    if (tu) {
        if (tu_count < MAX_TU) tu_made[tu_count++] = tu;
        else                   tu_overflow++;
    }
    return tu;
}

/* FreeTXT99Array(): the real __frtx9a() walks the array and free()s each
 * element, which on this host means free()ing the truncated end marker.  Free
 * the recorded pointers instead - same storage, reached the safe way. */
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
 * The SVC 99 shim: captures the request, hands back a DDNAME, reports success.
 * ------------------------------------------------------------------------ */
static int       svc99_calls;
static int       svc99_ntu;          /* text units in the captured request   */
static int       svc99_endmark;      /* last slot carried the high-order bit */
static int       svc99_identity;     /* array slots are the units we recorded*/

/* The request is snapshotted BY VALUE, not by pointer: the code under test
 * runs FreeTXT99Array() on its way out, so every TXT99 is gone by the time
 * the assertions read it. */
static struct {
    unsigned dal;
    unsigned count;
    unsigned size;
} svc99_snap[MAX_TU];

int __svc99(void *rb)
{
    RB99   *rb99 = (RB99 *)rb;
    TXT99 **p    = (TXT99 **)rb99->txtptr;
    int     n;

    svc99_calls++;
    svc99_ntu     = tu_count;
    svc99_endmark = 0;
    svc99_identity = 1;

    for (n = 0; n < tu_count; n++) {
        svc99_snap[n].dal   = tu_made[n]->dal;
        svc99_snap[n].count = tu_made[n]->count;
        svc99_snap[n].size  = tu_made[n]->size;

        if (n < tu_count - 1) {
            if (p[n] != tu_made[n]) svc99_identity = 0;
        }
        else {
            /* the end marker, truncated to 32 bits on this host (note c) */
            unsigned long want = ((unsigned long)(size_t)tu_made[n]
                                  & 0xFFFFFFFFUL) | 0x80000000UL;
            if ((unsigned long)(size_t)p[n] == want) svc99_endmark = 1;
        }
    }

    /* the RTDDN unit comes back holding the assigned DDNAME */
    if (tu_count > 0 && tu_made[0]->dal == DALRTDDN) {
        memcpy(tu_made[0]->text, "SYS00001", 8);
    }

    return 0;
}

/* __txrecf() is NOT linked in: it folds case with (c | ' '), which is OR 0x40
 * in EBCDIC - a fold to UPPER, correct on the target - and OR 0x20 in ASCII, a
 * fold to lower, so its uppercase table never matches on this host and every
 * call returns 1.  It is not the code under test; __fpnew() only needs it to
 * succeed and to put DALRECFM in the array. */
int __txrecf(TXT99 ***txt99, const char *recfm)
{
    unsigned char type = 0x80;      /* F - the value is not what is asserted */
    TXT99         *tu;

    if (!recfm) return 1;
    tu = NewTXT99(DALRECFM, 1, 1, (const char *)&type);
    if (!tu) return 1;
    return arrayadd(txt99, tu);
}

/* --------------------------------------------------------------------------
 * The code under test.
 * ------------------------------------------------------------------------ */
#include "../../src/clib/@@fpmode.c"
#include "../../src/clib/@@fpold.c"
#include "../../src/clib/@@fpnew.c"

#include "../../src/clib/@@txrddn.c"
#include "../../src/clib/@@txdsn.c"
#include "../../src/clib/@@txold.c"
#include "../../src/clib/@@txnew.c"
#include "../../src/clib/@@txcat.c"
#include "../../src/clib/@@txrlse.c"
#include "../../src/clib/@@txlrec.c"
#include "../../src/clib/@@txbksz.c"
#include "../../src/clib/@@txcyl.c"
#include "../../src/clib/@@txtrk.c"
#include "../../src/clib/@@txblk.c"
#include "../../src/clib/@@txspac.c"

#include "../../src/clib/@@txorg.c"

#include "../../src/clib/@@aradd.c"
#include "../../src/clib/@@arnew.c"
#include "../../src/clib/@@arcou.c"
#include "../../src/clib/@@arget.c"
#include "../../src/clib/@@arfre.c"

/* --------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------ */
static void reset(void)
{
    tu_count     = 0;
    tu_overflow  = 0;
    svc99_calls  = 0;
    svc99_ntu    = 0;
    svc99_endmark = 0;
    svc99_identity = 0;
}

static int has_dal(unsigned dal)
{
    int n;

    for (n = 0; n < svc99_ntu; n++) {
        if (svc99_snap[n].dal == dal) return 1;
    }
    return 0;
}

static int find_dal(unsigned dal)
{
    int n;

    for (n = 0; n < svc99_ntu; n++) {
        if (svc99_snap[n].dal == dal) return n;
    }
    return -1;
}

static void dump(const char *what)
{
    int n;

    printf("    %s: SVC 99 issued %d time(s), %d text unit(s):",
           what, svc99_calls, svc99_ntu);
    for (n = 0; n < svc99_ntu; n++) printf(" %04X", svc99_snap[n].dal);
    printf("\n");
    fflush(NULL);   /* libc370's stdout is a macro over __gtout(); flush all */
}

/* a FILE the tests can hand to __fpold()/__fpnew() */
static _FILE fh;

static int setup(const char *mode, const char *dsn, const char *member)
{
    int rc;
    unsigned char *p = (unsigned char *)&fh;
    size_t         i;

    for (i = 0; i < sizeof(fh); i++) p[i] = 0;

    rc = __fpmode(&fh, mode);
    if (rc == 0) {
        strcpy(fh.dataset, dsn);
        if (member) strcpy(fh.member, member);
    }
    reset();
    return rc;
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    int    rc;
    int    n;

    ctype_init();

    printf("=== TSTFPRLS - libc370 #167: RLSE through fopen() ===\n");

    /* ---------------------------------------------------------------- */
    printf("\n(1) fopen(dsn,\"wb\") -> __fpold(): no DALRLSE\n");
    CHECK_EQ(setup("wb", "MVSLOVE.TEST.DATA", NULL), 0, "__fpmode(\"wb\") ok");
    CHECK_EQ(fh.flags & _FILE_FLAG_RLSE, 0, "_FILE_FLAG_RLSE not set");
    rc = __fpold(&fh);
    dump("__fpold");
    CHECK_EQ(rc, 0, "__fpold() succeeded");
    CHECK_EQ(svc99_calls, 1, "SVC 99 issued once");
    CHECK(has_dal(DALRTDDN), "DALRTDDN present");
    CHECK(has_dal(DALDSNAM), "DALDSNAM present");
    CHECK(has_dal(DALSTATS), "DALSTATS (DISP=OLD) present");
    CHECK(!has_dal(DALRLSE), "no DALRLSE without \"rlse\"");
    CHECK_EQ(memcmp(fh.ddname, "SYS00001", 8), 0, "DDNAME returned");

    /* ---------------------------------------------------------------- */
    printf("\n(2) fopen(dsn,\"wb,rlse\") -> __fpold(): DALRLSE\n");
    CHECK_EQ(setup("wb,rlse", "MVSLOVE.TEST.DATA", NULL), 0,
             "__fpmode(\"wb,rlse\") ok");
    CHECK(fh.flags & _FILE_FLAG_RLSE, "_FILE_FLAG_RLSE set");
    rc = __fpold(&fh);
    dump("__fpold");
    CHECK_EQ(rc, 0, "__fpold() succeeded");
    CHECK(has_dal(DALRLSE), "DALRLSE present");
    CHECK(has_dal(DALSTATS), "DALSTATS (DISP=OLD) still present");
    n = find_dal(DALRLSE);
    CHECK(n >= 0, "DALRLSE unit found");
    if (n >= 0) {
        CHECK_EQ(svc99_snap[n].count, 0, "DALRLSE count is 0");
        CHECK_EQ(svc99_snap[n].size, 0, "DALRLSE size is 0");
    }

    /* ---------------------------------------------------------------- */
    printf("\n(3) member name present -> no DALRLSE (PO data set)\n");
    CHECK_EQ(setup("wb,rlse", "MVSLOVE.TEST.PDS", "MEMBER1"), 0,
             "__fpmode(\"wb,rlse\") ok");
    CHECK(fh.flags & _FILE_FLAG_RLSE, "_FILE_FLAG_RLSE set");
    rc = __fpold(&fh);
    dump("__fpold");
    CHECK_EQ(rc, 0, "__fpold() succeeded");
    CHECK(!has_dal(DALRLSE), "no DALRLSE for a PDS member");

    /* ---------------------------------------------------------------- */
    printf("\n(4) fopen(dsn,\"wb,...\") -> __fpnew(): no DALRLSE\n");
    CHECK_EQ(setup("wb,recfm=fb,lrecl=80,blksize=800,space=trk(30,5)",
                   "MVSLOVE.TEST.NEW", NULL), 0, "__fpmode() ok");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    CHECK(has_dal(DALTRK), "DALTRK present");
    CHECK(has_dal(DALPRIME), "DALPRIME present");
    CHECK(!has_dal(DALRLSE), "no DALRLSE without \"rlse\"");

    /* ---------------------------------------------------------------- */
    printf("\n(5) fopen(dsn,\"wb,...,rlse\") -> __fpnew(): DALRLSE\n");
    CHECK_EQ(setup("wb,recfm=fb,lrecl=80,blksize=800,space=trk(30,5),rlse",
                   "MVSLOVE.TEST.NEW", NULL), 0, "__fpmode() ok");
    CHECK(fh.flags & _FILE_FLAG_RLSE, "_FILE_FLAG_RLSE set");
    rc = __fpnew(&fh);
    dump("__fpnew");
    CHECK_EQ(rc, 0, "__fpnew() succeeded");
    CHECK(has_dal(DALRLSE), "DALRLSE present");
    CHECK(has_dal(DALTRK), "DALTRK still present");
    CHECK(has_dal(DALPRIME), "DALPRIME still present");
    CHECK(has_dal(DALRECFM), "DALRECFM still present");
    CHECK(has_dal(DALLRECL), "DALLRECL still present");
    CHECK(has_dal(DALNDISP), "DALNDISP (,CATLG) still present");

    /* ---------------------------------------------------------------- */
    printf("\n(6) __fpmode(): the keyword, and only the keyword\n");
    CHECK_EQ(setup("rb", "MVSLOVE.TEST.DATA", NULL), 0, "__fpmode(\"rb\") ok");
    CHECK_EQ(fh.flags & _FILE_FLAG_RLSE, 0, "\"rb\": no RLSE");
    CHECK_EQ(setup("wb,record", "MVSLOVE.TEST.DATA", NULL), 0,
             "__fpmode(\"wb,record\") ok");
    CHECK_EQ(fh.flags & _FILE_FLAG_RLSE, 0, "\"wb,record\": no RLSE");
    CHECK(fh.flags & _FILE_FLAG_RECORD, "\"wb,record\": RECORD set");
    CHECK_EQ(setup("wb,record,rlse", "MVSLOVE.TEST.DATA", NULL), 0,
             "__fpmode(\"wb,record,rlse\") ok");
    CHECK(fh.flags & _FILE_FLAG_RECORD, "\"wb,record,rlse\": RECORD set");
    CHECK(fh.flags & _FILE_FLAG_RLSE, "\"wb,record,rlse\": RLSE set");
    CHECK_EQ(setup("ab,rlse", "MVSLOVE.TEST.DATA", NULL), 0,
             "__fpmode(\"ab,rlse\") ok");
    CHECK(fh.flags & _FILE_FLAG_APPEND, "\"ab,rlse\": APPEND set");
    CHECK(fh.flags & _FILE_FLAG_RLSE, "\"ab,rlse\": RLSE set");

    /* ---------------------------------------------------------------- */
    printf("\n(7) the end-of-list marker still sits on the last unit\n");
    CHECK_EQ(setup("wb,rlse", "MVSLOVE.TEST.DATA", NULL), 0, "__fpmode() ok");
    rc = __fpold(&fh);
    CHECK_EQ(rc, 0, "__fpold() succeeded");
    CHECK(svc99_identity, "array slots are the units that were built");
    CHECK(svc99_endmark, "high-order bit set on the LAST slot");
    CHECK_EQ(tu_overflow, 0, "no text unit went unrecorded");

    return mbt_test_summary("TSTFPRLS");
}
