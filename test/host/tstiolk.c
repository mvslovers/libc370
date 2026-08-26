/*
 * tstiolk.c - libc370 #145 (host side): vvprintf() must hold the FILE
 * lock across the WHOLE line, and a nested public fputs()/fputc() must
 * never release a lock the caller still owns.
 *
 * ISSUE #145: vvprintf() takes the FILE lock, but the %s and %eEgGfF
 * branches call the PUBLIC fputs(), and every __examin() conversion
 * emits through putc() - the public fputc().  Those wrappers lock and
 * then UNCONDITIONALLY unlock the same FILE; lock()/unlock() are
 * ENQ/DEQ RET=HAVE with no nesting count, so the inner unlock() gives
 * the outer hold away at the first conversion and the rest of the line
 * runs unserialized.  test/mvs/tstiolk.c is the target-side proof
 * (JOB02235: the literal ftpd#117 IEC020I 001-1; JOB02237: a writer
 * wedged after 8 of 400 lines).
 *
 * This test compiles the REAL TUs - vvprintf.c, @@examin.c, @@fputs.c,
 * fputs.c, fputc.c - against a host model of the lock primitives and a
 * probe __fputc() that records, per emitted byte, the lock "epoch" it
 * was written under (0 = no lock held).  The model implements the
 * RET=HAVE semantics MEASURED on MVS 3.8 (JOB02237 round 1: nested
 * lock=8, unlock-not-held=8 without abend), so what is asserted here is
 * the library's use of the primitives, not a guess about them.
 *
 * What must hold (GREEN, the #145 fix):
 *
 *   1. every vvprintf() line costs exactly ONE acquire and ONE release,
 *      and every byte of it - conversions included - is written under
 *      that one hold;
 *   2. after vvprintf() returns the lock is free;
 *   3. a public fputs()/fputc() nested under a caller's lock() leaves
 *      the caller still owning the lock, and the caller's unlock()
 *      succeeds;
 *   4. a public fputs() WITHOUT an outer hold still locks and releases
 *      like before (the ownership test must not leak holds);
 *   5. (#147 item 2) puts() is ONE critical section: the string and its
 *      newline go out under a single hold, so a concurrent printf can
 *      no longer split the line at the '\n'.
 *
 * RED against the pre-fix source: the "[%s] %s" shape acquires twice
 * and writes half its bytes with no lock held, "%-8s" acquires eight
 * times (one ENQ/DEQ pair per byte through putc), and after a nested
 * fputs() the caller's hold is gone.  RED for (5) against the pre-#147
 * puts.c: two acquires, the '\n' in an epoch of its own.
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstvsnp.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tstiolk.c && ./t
 *
 * The lock()/unlock()/testlock() definitions below MUST stay above the
 * #includes of the library TUs: cliblock.h declares them with
 * asm("@@LK") labels, and only a definition that precedes the
 * declaration makes the host compiler drop the label (it warns
 * "attribute declaration must precede definition" - that warning is
 * load-bearing).
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- lock model: ENQ/DEQ RET=HAVE as measured on MVS (JOB02237) ------ */

static int lk_held  = 0;    /* is the (single) FILE lock held?           */
static int lk_epoch = 0;    /* bumped per acquire, stamps emitted bytes  */
static int lk_acq   = 0;    /* successful acquires                       */
static int lk_rel   = 0;    /* successful releases                       */

int lock(void *thing, int read)
{
    (void)thing; (void)read;
    if (lk_held) return 8;          /* you already have the lock         */
    lk_held = 1;
    lk_epoch++;
    lk_acq++;
    return 0;
}

int unlock(void *thing, int read)
{
    (void)thing; (void)read;
    if (!lk_held) return 8;         /* you didn't have the lock          */
    lk_held = 0;
    lk_rel++;
    return 0;
}

int testlock(void *thing, int read)
{
    (void)thing; (void)read;
    return lk_held ? 8 : 0;
}

/* ---- probe sink: every byte records the epoch it was written under -- */

#define OUTMAX  256

static char out[OUTMAX];
static int  outn;
static int  ep[OUTMAX];

int __fputc(int c, FILE *fp)
{
    (void)fp;
    if (outn < OUTMAX) {
        out[outn] = (char)c;
        ep[outn]  = lk_held ? lk_epoch : 0;
        outn++;
    }
    return c;
}

/* ---- the real TUs ---------------------------------------------------- */

#include "../../src/clib/vvprintf.c"
#undef outch
#undef inch
#undef unused
#include "../../src/clib/@@examin.c"
#include "../../src/clib/@@fputs.c"
#include "../../src/clib/fputs.c"
#include "../../src/clib/fputc.c"

/* puts() is a public libc symbol on the host too, and the compiler
   likes to rewrite printf("...\n") into puts() calls - which would
   land the harness's own output in the probe sink.  Compile the
   library's puts under a test name and call it explicitly. */
#define puts tst_puts
#include "../../src/clib/puts.c"
#undef puts

/* puts() writes to stdout, which clibio.h resolves via __gtout() */
static FILE *tst_stdout;
FILE **__gtout(void) { return &tst_stdout; }

/* ---- shims (same set as tstvsnp.c) ----------------------------------- */

void __dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
              char *result)
{
    (void)num; (void)cnvtype; (void)nwidth; (void)nprecision;
    strcpy(result, "1.500000");
}

void    __64_from_i32(__64 *n, int32_t i32) { (void)i32; n->u32[0] = n->u32[1] = 0; }
void    __64_from_u32(__64 *n, uint32_t u32) { (void)u32; n->u32[0] = n->u32[1] = 0; }
int32_t __64_to_i32(__64 *n) { (void)n; return 0; }
int     __64_is_zero(__64 *n) { (void)n; return 1; }
void    __64_copy(__64 *dst, __64 *src) { (void)src; dst->u32[0] = dst->u32[1] = 0; }
void    __64_divmod(__64 *a, __64 *b, __64 *c, __64 *d)
{ (void)a; (void)b; c->u32[0] = c->u32[1] = 0; d->u32[0] = d->u32[1] = 0; }

int *__errno(void) { static int e; return &e; }

static unsigned short isbuf_tbl[256];
unsigned short *__isbuf = isbuf_tbl;
static short toup_tbl[256];
short *__toup = toup_tbl;

static void ctype_init(void)
{
    int c;

    for (c = 0; c < 256; c++) toup_tbl[c] = (short)c;
    for (c = 'a'; c <= 'z'; c++) toup_tbl[c] = (short)(c - 'a' + 'A');
    for (c = '0'; c <= '9'; c++) isbuf_tbl[c] |= 0x0008U;   /* isdigit */
}

/* ---- harness ---------------------------------------------------------- */

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

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

/* run one vvprintf() and collect the lock ledger */
static void reset(void)
{
    /* no memset here: clibstr.h's memset is inline S/370 asm and does
       not survive the host compile (see tstvsnp.c) - run() NUL-
       terminates out[] after every call, which is all strcmp needs */
    outn = 0;
    lk_held = 0;
    lk_epoch = 0;
    lk_acq = 0;
    lk_rel = 0;
    out[0] = 0;
}

static int all_in_epoch_1(void)
{
    int i;

    for (i = 0; i < outn; i++) {
        if (ep[i] != 1) return 0;
    }
    return 1;
}

static void run(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    reset();
    va_start(ap, fmt);
    vvprintf(fmt, ap, fp, NULL);
    va_end(ap);
    out[outn < OUTMAX ? outn : OUTMAX - 1] = 0;
}

int main(void)
{
    _FILE   f = {{0}};
    FILE    *fp = &f;
    int     rc;

    ctype_init();
    f.flags = _FILE_FLAG_WRITE;

    printf("=== tstiolk: one lock per vvprintf line; nested public stdio "
           "must not release it (#145) ===\n\n");

    printf("(1) the ftpd#117 shape, [%%s] %%s:\n");
    run(fp, "[%s] %s\n", "INFO", "hello");
    CHECK(strcmp(out, "[INFO] hello\n") == 0, "(1) text rendered");
    CHECK_EQ(lk_acq, 1, "(1) exactly one acquire");
    CHECK_EQ(lk_rel, 1, "(1) exactly one release");
    CHECK(all_in_epoch_1(), "(1) every byte written under the one hold");
    CHECK_EQ(lk_held, 0, "(1) lock free afterwards");

    printf("\n(2) the simple path, %%d (control):\n");
    run(fp, "n=%d\n", 42);
    CHECK(strcmp(out, "n=42\n") == 0, "(2) text rendered");
    CHECK_EQ(lk_acq, 1, "(2) exactly one acquire");
    CHECK(all_in_epoch_1(), "(2) every byte under the one hold");
    CHECK_EQ(lk_held, 0, "(2) lock free afterwards");

    printf("\n(3) the __examin path, S%%03X:\n");
    run(fp, "S%03X\n", 0xD37);
    CHECK(strcmp(out, "SD37\n") == 0, "(3) text rendered");
    CHECK_EQ(lk_acq, 1, "(3) exactly one acquire (was 3: one per putc)");
    CHECK(all_in_epoch_1(), "(3) every byte under the one hold");
    CHECK_EQ(lk_held, 0, "(3) lock free afterwards");

    printf("\n(4) the float path, %%f (stubbed __dblcvt):\n");
    run(fp, "%f\n", 1.5);
    CHECK(strcmp(out, "1.500000\n") == 0, "(4) text rendered");
    CHECK_EQ(lk_acq, 1, "(4) exactly one acquire");
    CHECK(all_in_epoch_1(), "(4) every byte under the one hold");
    CHECK_EQ(lk_held, 0, "(4) lock free afterwards");

    printf("\n(5) width padding through __examin, [%%-8s]:\n");
    run(fp, "[%-8s]\n", "AB");
    CHECK(strcmp(out, "[AB      ]\n") == 0, "(5) text rendered");
    CHECK_EQ(lk_acq, 1, "(5) exactly one acquire (was 8: one per byte)");
    CHECK(all_in_epoch_1(), "(5) every byte under the one hold");
    CHECK_EQ(lk_held, 0, "(5) lock free afterwards");

    printf("\n(6) nested public fputs() under a caller's lock:\n");
    reset();
    rc = lock(fp, 0);
    CHECK_EQ(rc, 0, "(6) outer lock acquired");
    fputs("x", fp);
    CHECK_EQ(lk_held, 1, "(6) caller still owns the lock after fputs");
    rc = unlock(fp, 0);
    CHECK_EQ(rc, 0, "(6) caller's unlock succeeds");

    printf("\n(7) nested public fputc() under a caller's lock:\n");
    reset();
    rc = lock(fp, 0);
    CHECK_EQ(rc, 0, "(7) outer lock acquired");
    fputc('y', fp);
    CHECK_EQ(lk_held, 1, "(7) caller still owns the lock after fputc");
    rc = unlock(fp, 0);
    CHECK_EQ(rc, 0, "(7) caller's unlock succeeds");

    printf("\n(8) public fputs() with NO outer hold still locks/releases:\n");
    reset();
    fputs("z", fp);
    CHECK_EQ(lk_acq, 1, "(8) fputs acquired the lock");
    CHECK_EQ(lk_rel, 1, "(8) fputs released the lock");
    CHECK_EQ(lk_held, 0, "(8) lock free afterwards");

    printf("\n(9) puts() is one critical section (#147 item 2):\n");
    tst_stdout = fp;
    reset();
    rc = tst_puts("hi");
    out[outn < OUTMAX ? outn : OUTMAX - 1] = 0;
    CHECK(strcmp(out, "hi\n") == 0, "(9) text incl. newline rendered");
    CHECK(rc != EOF, "(9) return is not EOF");
    CHECK_EQ(lk_acq, 1, "(9) exactly one acquire (was 2: fputs + putc)");
    CHECK(all_in_epoch_1(), "(9) string AND newline under the one hold");
    CHECK_EQ(lk_held, 0, "(9) lock free afterwards");

    return mbt_test_summary("tstiolk");
}
