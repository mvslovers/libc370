/*
 * tstjestx.c - libc370 #111: the JES2 internal-text parsers in
 * src/jes/jesjob.c must bound every memcpy() by its destination, not by the
 * one-byte length field in the text stream.
 *
 * ISSUE #111: process_job() (jesjob.c:608) and process_exec() (jesjob.c:646)
 * both do
 *
 *     len = t->len & 0x7F;            // masks the high bit, does not bound it
 *     memcpy(userid, t->data, len);
 *     userid[len] = 0;
 *
 * t->len is one byte, so len runs to 127.  The destinations are
 * process_intxt()'s locals, all TWELVE bytes (jesjob.c:517-521):
 *
 *     unsigned char jobname[12], userid[12], stepname[12],
 *                   procstep[12], program[12];
 *
 * Five keys reach them unclamped: USERK and JOBK through process_job(),
 * PGMEK, PROCEK and EXECK through process_exec().
 *
 * The values do not stay in that frame either.  process_sysout()/
 * process_sysin() strcpy() stepname and procstep into JESDD fields that are
 * NINE bytes, and process_intxt() strcpy()s userid into a nine-byte
 * JESJOB.owner.  So one bad length byte writes past a stack buffer AND past a
 * heap one - which is why clamping to 8 rather than to 11 is the fix: 8 + NUL
 * is exactly what those nine-byte fields hold.
 *
 * THIS IS PR #22'S BUG, IN THE FUNCTIONS IT DID NOT TOUCH.  58319b2, "Fix
 * S0C4 in process_dd for multi-digit SPACE values", fixed precisely this in
 * the sibling parser - MIN(t->len, 8) for ddname, MIN(t->len, 44) for dsname,
 * MIN(t->len, 4) for sysout - and left process_exec()/process_job() with the
 * mask and no clamp.  Case (7) below is that fix's regression guard, which it
 * never had.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The destinations are heap allocations of exactly 12 bytes, so a copy of
 *   13 is a heap-buffer-overflow ASAN aborts on rather than a silent success
 *   into the next local.  ASAN is load-bearing here, not decoration (same
 *   reasoning as test/host/tstrldwk.c).
 *
 * - It pins the COPY, not the walk.  A desynchronised text stream can still
 *   step process_intxt()'s outer loop onto nonsense; what this guarantees is
 *   that nonsense truncates instead of overflowing.  Bounding the walk itself
 *   is a separate question and is NOT addressed here.
 *
 * - It does not prove #108.  Whether this overflow is the S0C4 in
 *   mvslovers/mvsmf#282 is unsettled - see #108.  This is a defect on its own
 *   terms and is fixed on its own terms.
 *
 * - Text bytes here are ASCII, not EBCDIC.  Nothing under test compares
 *   against a character literal except strtok(buf, " "), and host ' ' matches
 *   host ' '.  A test that cared about collating order could not do this.
 *
 * - The whole translation unit is #include'd because the three parsers are
 *   static (same recipe as test/host/tstrldwk.c for @@loadhi.c).  jesjob()
 *   itself is never called; the shims below only have to resolve.
 * ====================================================================
 *
 * BUILD / RUN (host, from test/host):
 *
 *   -D'__asm__(...)='  erases the file-scope S/370 statements
 *                      (__asm__("\n&FUNC SETC 'process_job'")).  Variadic
 *                      because extended asm carries commas.
 *   -D__volatile__=    turns clibstr.h's inline memset(), written
 *                      "__asm__ __volatile__(", into something the erase
 *                      above can reach.  NOTE this leaves memset() a no-op
 *                      stub, so this file uses calloc() and explicit stores
 *                      rather than memset().
 *   -D__32BIT__        is what libc370's stddef.h/stdlib.h key size_t off.
 *   -U__LP64__         time64.h is "#error Your time_t is already 64-bit"
 *                      under __LP64__, and clibjes2.h pulls it in for
 *                      JESJOB.start_time64.
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstjestx.c \
 *        "$R/src/clib/@@aradd.c" "$R/src/clib/@@arnew.c" \
 *        "$R/src/clib/@@arcou.c" "$R/src/clib/@@arget.c" \
 *        "$R/src/clib/@@arfre.c"
 *     ./t                                             # 14/14, rc 0
 *
 * RED, against the pre-fix source.  Cases (1)-(4) execute and pass, then case
 * (5) aborts.  The ASAN report is the ONLY output: the abort does not flush
 * stdout, so the four PASS lines are lost with it.
 *
 *     ==ERROR: AddressSanitizer: heap-buffer-overflow
 *     WRITE of size 64 at 0x60200000029c thread T0
 *         #0 __asan_memcpy
 *         #1 process_job+0x2ac
 *     0x60200000029c is located 0 bytes after 12-byte region
 *         allocated by thread T0 here: ... dest12
 *
 * "WRITE of size 64, 0 bytes after a 12-byte region" is the whole defect in
 * one line.  On MVS that write does not land in a redzone: it lands in
 * process_intxt()'s frame, on the locals declared after the buffer and
 * eventually on the saved registers, and the nine-byte JESDD/JESJOB fields
 * take the same overflow on the heap a moment later.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* jesjob.c declares neither of these itself - it calls wtof() and toupper()
 * with no prototype in scope, two more of #39's translation units.  Pulling
 * the headers in ahead of it keeps that out of this test's build line; it is
 * not a workaround for anything under test here. */
#include <ctype.h>
#include "clibwto.h"

#include "../../src/jes/jesjob.c"

/* ---- shims -------------------------------------------------------------
 * Defined AFTER the translation unit so each one matches the prototype the
 * library's own headers declare, rather than a second opinion about it.
 * Only the three parsers are exercised; these merely have to resolve.
 */
void wtof(const char *text, ...) { (void)text; }

int __jsrd4(HASPJS *js, unsigned mttr, void *buf4k, unsigned buflen)
{ (void)js; (void)mttr; (void)buf4k; (void)buflen; return -1; }

char *strcpyp(char *target, int tlen, void *source, int pad)
{ (void)tlen; (void)source; (void)pad; return target; }

void *memcpyp(void *target, int tlen, void *source, int slen, int pad)
{ (void)tlen; (void)source; (void)slen; (void)pad; return target; }

int __patmat(const char *str, const char *pat) { (void)str; (void)pat; return 0; }

time64_t mktime64(struct tm *tm) { time64_t r; (void)tm; r.u32[0] = r.u32[1] = 0; return r; }

void __64_init(__64 *n) { n->u32[0] = n->u32[1] = 0; }

int *__errno(void) { static int e; return &e; }

/* libc370's toupper(c) is a macro indexing __toup[], reached from jesjob().
** Never used from here; filled in at startup so it is not a trap either. */
static short toup_tbl[256];
short *__toup = toup_tbl;

/* ---- harness ---------------------------------------------------------- */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_STR(got, want, msg)                                          \
    do {                                                                   \
        mbt_run++;                                                         \
        if (strcmp((const char *)(got), (want)) == 0) {                    \
            mbt_passed++; printf("  PASS: %s\n", (msg)); }                 \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got \"%s\", want \"%s\")\n",            \
                      (msg), (const char *)(got), (want)); }               \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* ---- fixtures ----------------------------------------------------------
 * One text string carrying a single [key][count=1][len][data] unit, then a
 * terminator.  The walk in each parser is
 *
 *     for (t = buf; t->key && t->key != ENDK; t = buf)
 *         ... for (buf += 2, n = 0; n < t->count; n++)
 *                 buf += 1 + (buf[0] & 0x7F);
 *
 * so from the key it steps 2 to the length byte, then 1 + len past the data,
 * landing exactly on the terminator.
 *
 * prefix_len is 6 for __JOBSTR (STRLTH, STRINDCS, STRJINDC, STRJIND2,
 * STRJLABD, then STRJKEY) and 4 for __EXECSTR (STRLTH, STRINDCS, STREINDC,
 * then STREKEY).
 */
static unsigned char *make_txt(unsigned prefix_len, unsigned char type,
                               unsigned char key, unsigned char lenbyte,
                               const char *data, unsigned datalen)
{
    unsigned       total = prefix_len + 3 + datalen + 1;
    unsigned char *p     = calloc(1, total);
    unsigned       i;

    p[0] = (unsigned char)(total >> 8);
    p[1] = (unsigned char)(total & 0xFF);
    p[2] = type;

    p[prefix_len + 0] = key;
    p[prefix_len + 1] = 1;          /* count: one value follows            */
    p[prefix_len + 2] = lenbyte;    /* the length the parser will trust    */
    for (i = 0; i < datalen; i++) p[prefix_len + 3 + i] = (unsigned char)data[i];
    p[prefix_len + 3 + datalen] = ENDK;

    return p;
}

static unsigned char *make_jobstr(unsigned char key, unsigned char lenbyte,
                                  const char *data, unsigned datalen)
{ return make_txt(6, JOBSTR, key, lenbyte, data, datalen); }

static unsigned char *make_execstr(unsigned char key, unsigned char lenbyte,
                                   const char *data, unsigned datalen)
{ return make_txt(4, EXECSTR, key, lenbyte, data, datalen); }

/* A destination sized exactly as process_intxt() declares it, on the heap so
 * ASAN bounds it.  Twelve bytes: one past it is a diagnosable overflow. */
static char *dest12(void) { return calloc(1, 12); }

/* 64 printable bytes - longer than the buffer, shorter than the 127 the
 * length field can express, so the copy is unambiguous in the ASAN report. */
static const char LONG64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz??";

int main(void)
{
    unsigned char *txt;
    char          *a, *b, *c;
    char          *ddname, *dsname, *sysout;
    unsigned       sysin;

    printf("=== tstjestx: internal-text parsers must bound every copy "
           "(#111) ===\n\n");

    /* ------------------------------------------------------------------
     * (1)-(4) Well-formed input.  These establish that the clamp does not
     * change ordinary parsing - every real MVS name is eight or fewer.
     * ---------------------------------------------------------------- */
    printf("(1) process_job, USER=IBMUSER:\n");
    a = dest12(); b = dest12();
    txt = make_jobstr(USERK, 7, "IBMUSER", 7);
    process_job((char *)txt, a, b);
    CHECK_STR(b, "IBMUSER", "(1) userid parsed");
    free(txt); free(a); free(b);

    printf("\n(2) process_job, JOB=MBTTEST1 (a full eight):\n");
    a = dest12(); b = dest12();
    txt = make_jobstr(JOBK, 8, "MBTTEST1", 8);
    process_job((char *)txt, a, b);
    CHECK_STR(a, "MBTTEST1", "(2) jobname parsed, all eight kept");
    free(txt); free(a); free(b);

    printf("\n(3) process_exec, PGM=IEBGENER:\n");
    a = dest12(); b = dest12(); c = dest12();
    txt = make_execstr(PGMEK, 8, "IEBGENER", 8);
    process_exec((char *)txt, a, b, c);
    CHECK_STR(c, "IEBGENER", "(3) program parsed");
    free(txt); free(a); free(b); free(c);

    printf("\n(4) process_exec, EXEC=STEP1:\n");
    a = dest12(); b = dest12(); c = dest12();
    txt = make_execstr(EXECK, 5, "STEP1", 5);
    process_exec((char *)txt, a, b, c);
    CHECK_STR(a, "STEP1", "(4) stepname parsed");
    free(txt); free(a); free(b); free(c);

    /* ------------------------------------------------------------------
     * (5)-(6) #111.  A length byte past the destination.  Pre-fix these
     * abort under ASAN; post-fix they truncate at eight, which is what
     * the nine-byte JESDD/JESJOB fields downstream can hold.
     * ---------------------------------------------------------------- */
    printf("\n(5) process_job, USER= with a 64-byte length (#111):\n");
    a = dest12(); b = dest12();
    txt = make_jobstr(USERK, 64, LONG64, 64);
    process_job((char *)txt, a, b);
    CHECK(strlen(b) <= 8,       "(5) userid bounded by its destination");
    CHECK_STR(b, "ABCDEFGH",    "(5) and truncated, not mangled");
    free(txt); free(a); free(b);

    printf("\n(6) process_exec, PGM= with a 64-byte length (#111):\n");
    a = dest12(); b = dest12(); c = dest12();
    txt = make_execstr(PGMEK, 64, LONG64, 64);
    process_exec((char *)txt, a, b, c);
    CHECK(strlen(c) <= 8,       "(6) program bounded by its destination");
    CHECK_STR(c, "ABCDEFGH",    "(6) and truncated, not mangled");
    free(txt); free(a); free(b); free(c);

    printf("\n(6b) process_exec, EXEC= with a 64-byte length (#111):\n");
    a = dest12(); b = dest12(); c = dest12();
    txt = make_execstr(EXECK, 64, LONG64, 64);
    process_exec((char *)txt, a, b, c);
    CHECK(strlen(a) <= 8,       "(6b) stepname bounded by its destination");
    free(txt); free(a); free(b); free(c);

    printf("\n(6c) process_exec, PROC= with a 64-byte length (#111):\n");
    a = dest12(); b = dest12(); c = dest12();
    txt = make_execstr(PROCEK, 64, LONG64, 64);
    process_exec((char *)txt, a, b, c);
    CHECK(strlen(b) <= 8,       "(6c) procname bounded by its destination");
    free(txt); free(a); free(b); free(c);

    printf("\n(6d) process_job, JOB= with a 64-byte length (#111):\n");
    a = dest12(); b = dest12();
    txt = make_jobstr(JOBK, 64, LONG64, 64);
    process_job((char *)txt, a, b);
    CHECK(strlen(a) <= 8,       "(6d) jobname bounded by its destination");
    free(txt); free(a); free(b);

    /* ------------------------------------------------------------------
     * (7) The control, and PR #22's missing regression guard.
     * process_dd() has been clamped since 58319b2; an overlong DD name
     * must truncate there too, and must have done so all along.
     * ---------------------------------------------------------------- */
    printf("\n(7) process_dd, DD= with a 64-byte length (PR #22 guard):\n");
    ddname = dest12(); dsname = calloc(1, 56); sysout = calloc(1, 12);
    txt = make_txt(4, DDSTR, DDK, 64, LONG64, 64);
    process_dd((char *)txt, (char *)txt + 4 + 3 + 64, ddname, dsname, sysout,
               &sysin);
    CHECK(strlen(ddname) <= 8,  "(7) ddname still bounded (#22 holds)");
    CHECK_STR(ddname, "ABCDEFGH", "(7) and truncated at eight");
    free(txt); free(ddname); free(dsname); free(sysout);

    /* ------------------------------------------------------------------
     * (8) The length field's high bit is a flag, not magnitude.  The mask
     * was already there; this pins that the clamp did not drop it.
     * ---------------------------------------------------------------- */
    printf("\n(8) high bit set on the length byte:\n");
    a = dest12(); b = dest12();
    txt = make_jobstr(USERK, 0x80 | 7, "IBMUSER", 7);
    process_job((char *)txt, a, b);
    CHECK_STR(b, "IBMUSER", "(8) 0x87 still means seven");
    free(txt); free(a); free(b);

    return mbt_test_summary("tstjestx");
}
