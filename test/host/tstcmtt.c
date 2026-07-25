/*
 * tstcmtt.c - libc370 #14 regression: the Master Trace Table (MTT) walk in
 * cmtt_get_array() (src/cmtt/cmttget.c) must never emit a bogus MTENTRY -
 * neither one that runs past the end of the table (the over-read that feeds
 * mvsmf#176) nor one reached by a backward jump (a non-terminating walk that
 * re-adds the same region until storage is exhausted).
 *
 * Two defects, both in the two walk loops cmttget.c:51-56 and :59-64:
 *
 *   (a) OVER-READ.  INBOUNDS() (cmttget.c:3) validates only the START of an
 *       entry:  (unsigned)e >= mttentpt && (unsigned)e < mttendpt.  An entry
 *       whose start is in-bounds but whose 10-byte header + mtentlen data
 *       runs past mttendpt passes that check and is added to the array.  The
 *       consumer then reads mtentdat off the end of the table.
 *
 *   (b) BACKWARD JUMP / NON-TERMINATION.  mtentlen is a SIGNED halfword
 *       (ieezb806.h:76).  The advance step
 *           e = (unsigned)e + e->mtentlen + 10
 *       with a negative mtentlen jumps BACKWARD.  If the target is still
 *       in-bounds the walk never terminates - it re-adds the same region(s)
 *       forever, each array_add() growing the array until GETMAIN fails.
 *
 * ====================================================================
 * LIMITATION - READ THIS BEFORE TRUSTING THIS TEST
 * --------------------------------------------------------------------
 * This test does NOT link/execute the real cmtt_get_array().  cmttget.c
 * cannot be exercised on the host for TWO independent reasons:
 *
 *   1. It emits file-scope S/370 assembler comments via asm("\n* ...")
 *      (cmttget.c:73,75); the host assembler rejects them - the same wall
 *      mvsmf's tstmtln.c hit with consapi.c.
 *
 *   2. The walk casts pointers to a 32-bit `unsigned` (INBOUNDS + the advance
 *      step).  That is CORRECT on the 24-bit MVS target, but on a 64-bit host
 *      it TRUNCATES the buffer address to the low 4 GB.  The truncated pointer
 *      is unmapped (macOS __PAGEZERO), so the very first backward jump that
 *      stays "in bounds" dereferences it and SIGSEGVs before any assertion can
 *      run.  (Verified: a faithful case-(b) table crashes rc=139.)
 *
 * So the two walk loops below are a hand-written MIRROR of cmttget.c:51-64,
 * with the bounds arithmetic WIDENED to host pointer width (uintptr_t) so it
 * runs natively.  The defects being pinned - a start-only bounds check, and a
 * signed-length backward advance - are layout- and width-INDEPENDENT (same
 * argument tstmtln.c makes for the sign of mtentlen), so a symbolic table
 * reproduces them on any host.  Consequences, same as tstmtln.c:
 *   - This is a RED->GREEN GATE for ONE specific fix (whole-entry bounds +
 *     bounded positive length before advancing).  It is NOT durable
 *     protection against a future refactor of cmttget.c, because it never
 *     executes that file.  If you change cmttget.c's walk you MUST change the
 *     walk_current()/walk_fixed() mirrors below by hand or this test silently
 *     diverges from reality.
 *   - The DURABLE version is an MVS integration test that builds the real
 *     cmttget.o and drives cmtt_get_array() against a synthetic MTTABLE on
 *     the 24-bit target; that is the follow-up, not this file.
 * ====================================================================
 *
 * Build / run (host; no libc370 headers, no mbt required):
 *     cc -std=gnu99 -Wall -Wextra -o tstcmtt tstcmtt.c && ./tstcmtt   # GREEN
 *     cc -std=gnu99 -Wall -Wextra -DTSTCMTT_BUGGY_WALK -o tstcmttx \
 *         tstcmtt.c && ./tstcmttx                                     # RED
 * RC: 0 = all checks passed, 1 = a check failed.  #14.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>

/* --------------------------------------------------------------------------
 * Minimal mbtcheck.h-compatible harness.  libc370 has no mbt submodule, so we
 * inline the exact CHECK / CHECK_EQ / mbt_test_summary contract from
 * mbt/include/mbtcheck.h; if libc370 later adopts mbt this becomes a one-line
 * #include swap.
 * ------------------------------------------------------------------------ */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                       \
    do {                                       \
        mbt_run++;                             \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#define CHECK_EQ(got, want, msg)                                          \
    do {                                                                  \
        long g_ = (long)(got), w_ = (long)(want);                         \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }    \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %ld, want %ld)\n", (msg), g_, w_); }\
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

/* --------------------------------------------------------------------------
 * Faithful minimal MTENTRY (ieezb806.h:69-80).  Only the SIGNED-halfword
 * mtentlen and the fixed 10-byte header are load-bearing; both are used
 * symbolically, so the host/target field-offset difference (void *mtentimm is
 * 4 bytes on S/370, 8 on a 64-bit host) does not matter.
 * ------------------------------------------------------------------------ */
typedef struct {
    char   mtentflg[2];
    char   mtenttag[2];
    void  *mtentimm;
    short  mtentlen;      /* 08 LENGTH OF CALLERS DATA (SIGNED halfword) */
    char   mtentdat[64];
} MTENTRY;

#define MT_HDR 10        /* cmttget.c advance/bounds use the literal 10 */

/* Host table analog.  mttentpt/mttendpt are `unsigned` (32-bit) in
 * ieezb806.h - correct for the 24-bit target - but WIDENED to uintptr_t here
 * so host addresses are not truncated (see LIMITATION #2). */
typedef struct {
    MTENTRY   *cur;      /* mttcurpt */
    MTENTRY   *wrp;      /* mttwrppt */
    uintptr_t  entpt;    /* mttentpt */
    uintptr_t  endpt;    /* mttendpt */
} MTT;

/* mirrors cmttget.c INBOUNDS (start-only) and the advance step, widened. */
#define H_INB(t, e) ((uintptr_t)(e) >= (t)->entpt && (uintptr_t)(e) < (t)->endpt)
#define H_ADV(e)    ((MTENTRY *)((char *)(e) + (e)->mtentlen + MT_HDR))

/* --------------------------------------------------------------------------
 * Array collector.  Faithful to @@aradd.c semantics that matter here (append,
 * count), plus a runaway WATCHDOG: the real array_add() grows without bound,
 * so on MVS a non-terminating walk exhausts storage (GETMAIN, S878).  Here we
 * cap the collector and longjmp() out - the host analog of that failure - so
 * non-termination surfaces as a clean assertion, not a hung test.
 * ------------------------------------------------------------------------ */
#define MTT_ADD_CAP 100000
static MTENTRY *g_slot[MTT_ADD_CAP + 8];
static unsigned g_count;
static jmp_buf  g_runaway;

static void a_reset(void) { g_count = 0; memset(g_slot, 0, sizeof g_slot); }

static void a_add(MTENTRY *e)
{
    if (g_count >= MTT_ADD_CAP) longjmp(g_runaway, 1);   /* runaway -> unwind */
    g_slot[g_count++] = e;
}

/* The two walks below are mirrors of cmttget.c:51-64.  Exactly one is compiled
 * (selected by -DTSTCMTT_BUGGY_WALK) so neither is flagged unused; both live
 * here adjacently so the fix reads as a diff.  RED runs walk_current (a verbatim
 * mirror of the shipping walk); GREEN runs walk_fixed (that walk + the #14 fix).
 */
#ifdef TSTCMTT_BUGGY_WALK

/* === MIRROR of cmttget.c:51-64 - the CURRENT (unfixed) walk ============== */
static void walk_current(MTT *t)
{
    MTENTRY *e;

    for (e = t->cur; H_INB(t, e); e = H_ADV(e))
        a_add(e);

    for (e = t->wrp; H_INB(t, e) && e < t->cur; e = H_ADV(e))
        a_add(e);
}
#  define WALK      walk_current
#  define WALK_NAME "walk_current (mirror of UNFIXED cmttget.c)"

#else

/* === MIRROR of cmttget.c:51-64 - WITH the #14 fix applied ================
 * Validate the WHOLE entry (start + 10 + mtentlen <= mttendpt) and require a
 * bounded POSITIVE length before advancing; a non-positive or over-long
 * length ENDS the walk instead of advancing into garbage.  This is exactly
 * the fix applied to cmttget.c, mirrored here.
 */
static void walk_fixed(MTT *t)
{
    MTENTRY *e;

    for (e = t->cur;
         H_INB(t, e) && e->mtentlen > 0
             && (uintptr_t)e + (unsigned)MT_HDR + (unsigned)e->mtentlen <= t->endpt;
         e = H_ADV(e))
        a_add(e);

    for (e = t->wrp;
         H_INB(t, e) && e < t->cur && e->mtentlen > 0
             && (uintptr_t)e + (unsigned)MT_HDR + (unsigned)e->mtentlen <= t->endpt;
         e = H_ADV(e))
        a_add(e);
}
#  define WALK      walk_fixed
#  define WALK_NAME "walk_fixed (mirror of FIXED cmttget.c)"

#endif

/* run the selected walk under the runaway watchdog; 1 => did not terminate */
static int run_walk(MTT *t)
{
    a_reset();
    if (setjmp(g_runaway)) return 1;   /* watchdog fired: non-terminating */
    WALK(t);
    return 0;
}

/* place an entry at buf+off with signed length len; return its address */
static MTENTRY *put(char *buf, unsigned off, short len)
{
    MTENTRY *e = (MTENTRY *)(buf + off);
    e->mtentlen = len;
    return e;
}

int main(void)
{
    static char buf[1024];       /* backing store; > endpt so header reads map */
    MTT t;
    unsigned n, over;
    int runaway;

    printf("=== TSTCMTT: #14 MTT walk (%s) ===\n", WALK_NAME);

    /* ---------------------------------------------------------------------
     * Case (a) OVER-READ.  Data area = [buf, buf+256).  Two valid entries,
     * then an entry at +250 whose 10+100 extent runs to +360, past endpt.
     * The start (+250) is in-bounds, so the start-only INBOUNDS admits it.
     * ------------------------------------------------------------------- */
    memset(buf, 0, sizeof buf);
    t.entpt = (uintptr_t)buf;
    t.endpt = (uintptr_t)(buf + 256);
    put(buf,   0,  40);          /* [0,50)    valid */
    put(buf,  50, 190);          /* [50,250)  valid */
    put(buf, 250, 100);          /* [250,360) OVER-READ: 250+10+100 > 256 */
    t.cur = (MTENTRY *)(buf + 0);
    t.wrp = (MTENTRY *)(buf + 0);/* wrp == cur -> loop 2 is a no-op here */

    runaway = run_walk(&t);
    CHECK(!runaway, "(a) over-read: walk terminates");

    over = 0;
    for (n = 0; n < g_count; n++) {
        MTENTRY *e = g_slot[n];
        if ((uintptr_t)e + (unsigned)MT_HDR + (unsigned)e->mtentlen > t.endpt)
            over++;
    }
    printf("  (a) collected %u entr%s, %u overrun the table\n",
           g_count, g_count == 1 ? "y" : "ies", over);
    CHECK(over == 0, "(a) over-read: no returned entry runs past mttendpt");
    CHECK_EQ(g_count, 2, "(a) over-read: only the two in-bounds entries kept");

    /* ---------------------------------------------------------------------
     * Case (b) BACKWARD JUMP / NON-TERMINATION.  cur=+200 has mtentlen=-100:
     * advance = 200 + (-100) + 10 = +110 (backward, in-bounds).  The entry at
     * +110 has mtentlen=+80: advance = 110 + 80 + 10 = +200.  The current
     * walk cycles +200 <-> +110 forever, re-adding the same regions.
     * ------------------------------------------------------------------- */
    memset(buf, 0, sizeof buf);
    t.entpt = (uintptr_t)buf;
    t.endpt = (uintptr_t)(buf + 256);
    put(buf, 200, (short)-100);  /* backward jump to +110 */
    put(buf, 110,  80);          /* jump forward to +200 -> cycle */
    t.cur = (MTENTRY *)(buf + 200);
    t.wrp = (MTENTRY *)(buf + 110);

    runaway = run_walk(&t);
    printf("  (b) runaway=%d, collected %u entr%s\n",
           runaway, g_count, g_count == 1 ? "y" : "ies");
    CHECK(!runaway, "(b) backward-jump: walk terminates (no runaway array_add)");
    CHECK(g_count <= 8, "(b) backward-jump: array size stays bounded");

    return mbt_test_summary("TSTCMTT");
}
