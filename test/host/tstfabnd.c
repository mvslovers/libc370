/*
 * tstfabnd.c - libc370 #168: __fabandon() closes a FILE whose last
 * write failed, without re-driving that write.
 *
 * ISSUE #168: fclose() flushes before it closes.  When the pending
 * block is exactly what could not be written - an x37 on a data set
 * with no space left - the flush re-drives the identical WRITE, the
 * close abends in turn, __fpfree() never runs, and the DD stays
 * allocated for the life of the job.  The data set can then be
 * scratched neither by the program that created it nor by its user.
 *
 * Measured on mvsdev 2026-09-09, mvslovers/ftpd#129:
 *
 *     IEC031I D37-04,IFG0554T,FTPDT,FTPDT,SYS00006,251,WORK00,...
 *     FTPD070E ABEND SD37 RECOVERED CMD=STOR SOCKET=3 TOTAL=1
 *     FTPD076W CLOSE ABENDED AFTER STOR, DD=SYS00006 FREE RC=4
 *
 * ftpd opens with fopen(dsn,"wb") - the buffered __fputc path.  The
 * D37 fires inside __awrite(); the caller's ESTAE unwinds THROUGH
 * libc370, so @@fflush.c's reset: label never runs and fp->upto still
 * points past the block that failed.  That is the state this test
 * builds, and the whole question is what the next close does with it.
 *
 * This compiles the REAL fclose.c, @@fpterm.c and @@faband.c with the
 * real array TUs, the same two-resource lock model as tstfcls.c, and
 * shims for __fflush/__aclose/__adisc/___try/__fpfree that count and
 * record instead of touching MVS.  @@fflush.c itself is shimmed rather
 * than compiled: it calls memset(), whose libc370 definition is an
 * inline MVCL that a host assembler cannot assemble.  The shim does the
 * one thing that matters here - a non-empty buffer means one __awrite()
 * - which is exactly what fixflush()/varflush() do.
 *
 *   (A) THE DEFECT, as a control: fclose() on a FILE left in that state
 *       drives __awrite() once - it rewrites the block that abended.
 *       This check asserts the OLD behaviour and stays green; it is
 *       here so a future change to fclose() cannot silently make the
 *       rest of this test vacuous.
 *   (B) __fabandon() drives __awrite() ZERO times, and still reaches
 *       __aclose(), __fpfree(), grtfile and free().
 *   (C) __adisc() is called with fp->dcb, before __aclose() - so
 *       @@ACLOSE's opening FIXWRITE has nothing to write even when the
 *       DCB, not the C buffer, is what holds the pending block.
 *   (D) __aclose() is reached THROUGH ___try(), never directly.
 *   (E) return codes: 0 clean / >0 the abend code / -2 the DD stayed /
 *       -3 ESTAE CREATE failed, and in that last case NOTHING is torn
 *       down - the FILE is still registered and still has its DCB.
 *   (F) the stale ENQ.  The abended fwrite() held the FILE lock and the
 *       ESTAE retry never DEQ'd it, so lock() answers 8.  fclose()
 *       treats that as "an outer caller owns it" and leaves it held -
 *       on storage it then free()s.  __fabandon() must DEQ anyway.
 *   (G) a bad handle answers -1 and frees nothing.
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstfcls.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstfabnd.c \
 *        "$R/src/clib/@@aradd.c" "$R/src/clib/@@arnew.c" \
 *        "$R/src/clib/@@arcou.c" "$R/src/clib/@@ardel.c" \
 *        "$R/src/clib/@@arfre.c" "$R/src/clib/@@arget.c" && ./t
 *
 * What the host CANNOT answer, and test/mvs/tstfabnd.c exists for:
 * whether CLOSE with nothing pending completes at all on a data set
 * that is out of space, and whether the DD then really goes.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "clibcrt.h"
#include "cliblock.h"

/* ---- two-resource lock model (ENQ/DEQ RET=HAVE, per address) --------- */

#define MAXLK   8

static void     *hold[MAXLK];
static int      hold_n;

static int lk_held(void *thing)
{
    int     i;

    for (i = 0; i < hold_n; i++) {
        if (hold[i] == thing) return 1;
    }
    return 0;
}

int lock(void *thing, int read)
{
    (void)read;
    if (lk_held(thing)) return 8;   /* you already have the lock         */
    if (hold_n < MAXLK) hold[hold_n++] = thing;
    return 0;
}

int unlock(void *thing, int read)
{
    int     i;

    (void)read;
    for (i = 0; i < hold_n; i++) {
        if (hold[i] == thing) {
            hold[i] = hold[--hold_n];
            return 0;
        }
    }
    return 8;                       /* you didn't have the lock          */
}

/* ---- recording shims -------------------------------------------------- */

static FILE     *watchfp;
static int      awrite_calls;       /* the physical write - must stay 0   */
static int      aclose_calls;
static void     *aclose_handle;
static int      adisc_calls;
static void     *adisc_handle;
static int      adisc_before_aclose = -1;
static int      try_calls;
static int      aclose_via_try;     /* __aclose entered from inside ___try */
static int      in_try;
static int      fpfree_calls;
static int      freed_fp;
static int      freed_buf;

/* what the next ___try() pretends to do */
static int      try_abend;          /* 0, or a 0x00sssuuu abend code      */
static int      try_estae_fails;    /* nonzero: ESTAE CREATE failed       */
static int      fpfree_rc;          /* what __fpfree() answers            */

static void reset_shims(void)
{
    awrite_calls = aclose_calls = adisc_calls = try_calls = 0;
    aclose_via_try = fpfree_calls = freed_fp = freed_buf = 0;
    adisc_before_aclose = -1;
    aclose_handle = adisc_handle = 0;
    try_abend = try_estae_fails = fpfree_rc = 0;
}

/* @@FFLUSH.C, in miniature: a non-empty buffer is one physical write.
   On the target that __awrite() is where the D37 came from - and the
   reason fp->upto is still set here is that the abend unwound through
   @@fflush.c before its reset: label could clear it. */
int __fflush(FILE *fp)
{
    if (fp->upto != fp->buf) awrite_calls++;
    fp->upto    = fp->buf;
    fp->filepos = 0;
    return 0;
}

void __aclose(void *handle)
{
    aclose_calls++;
    aclose_handle = handle;
    if (in_try) aclose_via_try = 1;
}

void __adisc(void *handle)
{
    adisc_calls++;
    adisc_handle = handle;
    adisc_before_aclose = (aclose_calls == 0);
}

int __fpfree(FILE *fp)
{
    (void)fp;
    fpfree_calls++;
    return fpfree_rc;
}

/* ___try(): call func under "ESTAE".  A negative answer means the ESTAE
   CREATE failed and func was NEVER entered - @@@try.c returns before the
   BALR in that case, which is what makes -3 a "nothing happened" rc. */
int ___try(void *func, ...)
{
    va_list     ap;
    void        *handle;

    va_start(ap, func);
    handle = va_arg(ap, void *);
    va_end(ap);

    try_calls++;
    if (try_estae_fails) return -8;

    in_try = 1;
    ((void (*)(void *))func)(handle);
    in_try = 0;

    return try_abend;
}

static CLIBGRT  fakegrt;

CLIBGRT *__grtget(void)
{
    return &fakegrt;
}

int *__errno(void) { static int e; return &e; }

static void tst_free(void *p)
{
    if (p == watchfp) freed_fp = 1;
    else if (watchfp && p == watchfp->buf) freed_buf = 1;
    free(p);
}

#define free tst_free
#include "../../src/clib/fclose.c"
#include "../../src/clib/@@faband.c"
#include "../../src/clib/@@fpterm.c"
#undef free

/* ---- harness ---------------------------------------------------------- */

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK_EQ(got, want, msg)                                           \
    do {                                                                   \
        mbt_run++;                                                         \
        if ((got) == (want)) { mbt_passed++; printf("  PASS: %s\n", (msg)); } \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got %d, want %d)\n",                    \
                      (msg), (int)(got), (int)(want)); }                   \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* A FILE the way an fopen(dsn,"wb") leaves it AFTER a write abended:
   open, writable, dynamically allocated, registered in grt->grtfile, and
   with fp->upto still past the block @@fflush.c could not write. */
static char     thedcb[8];
static unsigned char asmbuf[256];

static FILE *make_file(int pending)
{
    FILE    *fp;
    int     i;

    fp = calloc(1, sizeof(_FILE));
    for (i = 0; _FILE_EYE[i]; i++) fp->eye[i] = _FILE_EYE[i];
    fp->flags   = _FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_DYNAMIC;
    fp->recfm   = _FILE_RECFM_F;
    fp->lrecl   = 80;
    fp->blksize = 800;
    fp->dcb     = thedcb;
    fp->asmbuf  = asmbuf;
    fp->buf     = calloc(1, 128);
    fp->upto    = fp->buf + (pending ? 80 : 0);
    fp->endbuf  = fp->buf + 128;
    arrayadd(&fakegrt.grtfile, fp);
    watchfp = fp;
    return fp;
}

int main(void)
{
    FILE        *fp;
    int          rc;
    static char  notafile[sizeof(_FILE)];

    printf("=== tstfabnd: __fabandon() - close without re-driving the "
           "failed write (#168) ===\n\n");

    /* ---- (A) the defect, as a control ------------------------------- */
    reset_shims();
    fp = make_file(1);
    fclose(fp);
    CHECK_EQ(awrite_calls, 1,
             "(A) fclose() rewrites the block that abended - the defect");
    CHECK_EQ(aclose_calls, 1, "(A) fclose() closes");

    /* ---- (B) abandon writes nothing --------------------------------- */
    reset_shims();
    fp = make_file(1);
    rc = __fabandon(fp);
    CHECK_EQ(awrite_calls, 0, "(B) __fabandon() writes NOTHING");
    CHECK_EQ(aclose_calls, 1, "(B) __fabandon() still closes");
    CHECK_EQ(fpfree_calls, 1, "(B) __fabandon() still frees the DD");
    CHECK_EQ(freed_buf, 1,    "(B) the buffer is freed");
    CHECK_EQ(freed_fp, 1,     "(B) the FILE is freed");
    CHECK_EQ((int)arraycount(&fakegrt.grtfile), 0,
             "(B) the FILE left grtfile");
    CHECK_EQ(rc, 0, "(B) rc=0 on a clean abandon");

    /* ---- (C) the DCB is told there is nothing pending ---------------- */
    CHECK_EQ(adisc_calls, 1,              "(C) __adisc() called once");
    CHECK_EQ(adisc_handle == thedcb, 1,  "(C) __adisc() got fp->dcb");
    CHECK_EQ(adisc_before_aclose, 1,      "(C) __adisc() before __aclose()");

    /* ---- (D) CLOSE runs under ESTAE ---------------------------------- */
    CHECK_EQ(try_calls, 1,      "(D) exactly one ___try()");
    CHECK_EQ(aclose_via_try, 1, "(D) __aclose() entered from inside it");
    CHECK_EQ(aclose_handle == thedcb, 1, "(D) __aclose() got fp->dcb");

    /* ---- (E) return codes -------------------------------------------- */
    reset_shims();
    fp = make_file(1);
    try_abend = 0x00D37000;             /* CLOSE abended anyway          */
    rc = __fabandon(fp);
    CHECK_EQ(rc, 0x00D37000, "(E) a CLOSE abend is reported, not raised");
    CHECK_EQ(freed_fp, 1,    "(E) the FILE still goes after a CLOSE abend");
    CHECK_EQ(hold_n, 0,      "(E) no lock left held after a CLOSE abend");

    reset_shims();
    fp = make_file(1);
    fpfree_rc = 4;                      /* the DD would not unallocate   */
    rc = __fabandon(fp);
    CHECK_EQ(rc, -2, "(E) rc=-2 when the DD could not be unallocated");

    reset_shims();
    fp = make_file(1);
    try_estae_fails = 1;
    rc = __fabandon(fp);
    CHECK_EQ(rc, -3,           "(E) rc=-3 when ESTAE CREATE failed");
    CHECK_EQ(aclose_calls, 0,  "(E) ... and CLOSE was not attempted");
    CHECK_EQ(freed_fp, 0,      "(E) ... and the FILE was not freed");
    CHECK_EQ(fp->dcb == thedcb, 1, "(E) ... and it still has its DCB");
    CHECK_EQ((int)arraycount(&fakegrt.grtfile), 1,
             "(E) ... and it is still registered");
    CHECK_EQ(hold_n, 0,        "(E) ... and the FILE lock was released");
    /* clean up the FILE this case deliberately left standing */
    try_estae_fails = 0;
    __fabandon(fp);

    /* ---- (F) the stale ENQ from the abended write --------------------- */
    reset_shims();
    fp = make_file(1);
    lock(fp, 0);                        /* what the dead fwrite() left    */
    CHECK_EQ(lk_held(fp), 1, "(F) the FILE lock is held on entry");
    rc = __fabandon(fp);
    CHECK_EQ(rc, 0,     "(F) abandon still succeeds");
    CHECK_EQ(hold_n, 0, "(F) the stale FILE lock was DEQ'd");

    /* ---- (G) a bad handle -------------------------------------------- */
    reset_shims();
    watchfp = 0;
    CHECK_EQ(__fabandon(NULL), -1, "(G) NULL answers -1");
    CHECK_EQ(__fabandon((FILE *)notafile), -1,
             "(G) no eye catcher answers -1");
    CHECK_EQ(aclose_calls, 0, "(G) ... and nothing was closed");
    CHECK_EQ(hold_n, 0,       "(G) ... and no lock was taken");

    return mbt_test_summary("tstfabnd");
}
