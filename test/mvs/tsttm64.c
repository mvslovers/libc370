/*
 * tsttm64.c - libc370 #49: the unit contract of the time64 clock family and
 * the divisor layer under it (MVS target, batch).
 *
 * ISSUE #49: clock64() (src/time64/tm64clck.c) divided the microseconds out
 * by 1000, so it returned MILLISECONDS while clock64_t and the header said
 * seconds.  mclock64() is the same seven lines, so the two were the same
 * function.  time64() then divided by CLOCKS_PER_SEC (1000) and came out in
 * seconds after all - the bug was cancelled inside the library by a second
 * wrong constant.  The fix had to move BOTH sites at once, and this file is
 * the net that proves it moved only what it meant to.
 *
 * The file landed one PR AHEAD of the fix (step A) with cases (1) and (2)
 * written against the old behaviour.  The fix (step B) rewrote exactly those
 * two and left everything else untouched, which is the evidence that
 * time64() did not move.
 *
 * THIS FILE CHANGES NO BEHAVIOUR.  It calls the functions exactly as they
 * ship and only compares their values to each other.  There is deliberately
 * no STCK injection and no scaling kernel here.
 *
 * ====================================================================
 * THREE CLASSES, AND THEY ARE NOT THE SAME KIND OF TEST
 * --------------------------------------------------------------------
 * (A) UNIT CONTRACT - the two cases the fix deliberately moved.  They now
 *     record that clock64() == time64() (both seconds) and that clock64()
 *     is 1000x smaller than mclock64().  Before the fix they read
 *     clock64() == mclock64() and clock64()/1000 == time64(); the old text
 *     is quoted at each case so the change is legible from here.  Any
 *     future change to the units belongs in these two and nowhere else.
 *
 * (B) INVARIANT - untouched by the fix and green on both sides of it.  This
 *     is the actual safety net.  If a case here goes red while someone is
 *     editing tm64clck.c or tm64time.c, the edit is wrong, not the test.
 *
 * (C) ARITHMETIC - fixed vectors through __64_div_u32()/__64_divmod_u32().
 *     The whole fix is a change of divisor, so the divider itself gets
 *     independent coverage at both 1000 and 1000000.
 *
 * ====================================================================
 * WHY ALL OF IT RUNS HERE AND NOT ON A HOST
 * --------------------------------------------------------------------
 * The obvious split - scaling arithmetic on the host, clock readings on
 * MVS - does not survive contact with __64.  That type keeps three views
 * of the same eight bytes and mixes them freely: __64_cmp compares .u64,
 * __64_div and __64_sub work on array[] of uint16_t with array[0] the most
 * significant halfword, __64_lshift_one_bit and __64_from_u32 work on u32[]
 * with u32[0] the high word.  On this target all three agree and the code
 * is correct.  On a little-endian host they do not, and nothing says so:
 * the sources compile, link and run, and the scaling comes out 0.  Word
 * size is not the issue, so -m32 does not help either - an ILP32 x86 host
 * is still little-endian.
 *
 * So every case below, including class (C), belongs on the target.  The
 * host-side companion test/host/tsttm64vec.c deliberately touches no
 * libc370 code at all: it re-derives the expected quotients of class (C)
 * with native 64-bit arithmetic, which catches a typo in the vector table
 * without pretending to exercise the library.
 *
 * ====================================================================
 * THE ONE CASE THAT NEEDS A REAL WAIT
 * --------------------------------------------------------------------
 * Case (9) is the cheapest guard on the most expensive silent assumption
 * in the library.  dispatch_work() in src/thdmgr/@@cminit.c does
 *
 *     now = time64(NULL);
 *     __64_sub(&now, &work->wait_time, &tmp);
 *     post_timer = __64_to_i32(&tmp);
 *
 * and tests post_timer for truth.  Nothing there names a unit; the
 * assumption that time64() counts SECONDS is entirely implicit, and the
 * only place it is written down is a header comment on the option that
 * drives it (include/clibthdi.h:61, "on=post timer desired (1 sec)").  Get
 * it wrong and there is no abend and no message: milliseconds post every
 * waiting worker on essentially every manager pass, seconds/1000 stop the
 * timer posts for up to ~16 minutes.
 *
 * Case (9) reproduces exactly that arithmetic over a real two-second
 * STIMER wait, with no thread manager to set up.  A difference of 1..3 is
 * seconds.  ~2000 is milliseconds.  0 is seconds/1000.
 *
 * ====================================================================
 * WHAT THIS DOES NOT PIN
 * --------------------------------------------------------------------
 * - Absolute correctness of the TOD epoch.  Everything here is relative or
 *   bounded; cases (3) and (4) would legitimately fail on a system whose
 *   TOD clock is set to the wrong century, and that is a system fault, not
 *   a library one.  The window is wide (2000..2100) so only real damage
 *   trips it.
 * - The 16-bit limb arithmetic underneath __64_div for values >= 2^63,
 *   where its own overflow guard takes over.  Cases (16a) and (16b) stay
 *   below that line deliberately.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude test/mvs/tsttm64.c -o TSTTM64 \
 *           -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tsttm64.jcl.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND
 * CODE).  A class (B) failure is a finding about the library or the system,
 * not an expectation to adjust - read the SYSPRINT first.
 */
#include <stdio.h>
#include <time64.h>

static int bad = 0;

/* Every case reports one line and bumps `bad` on failure.  `note` carries
   the observed values, so a red run says what it saw and not just that it
   was unhappy. */
static void check(const char *what, int good, const char *note)
{
    printf("  %-46s %s   %s\n", what, good ? "ok  " : "FAIL", note);
    if (!good) bad++;
}

/* Build a __64 from its two halves in TARGET layout: u32[0] is the high
   word.  Same convention as __64_from_u32() in src/clib/@@64fu32.c. */
static void put64(__64 *n, unsigned int hi, unsigned int lo)
{
    n->u32[0] = hi;
    n->u32[1] = lo;
}

static int is64(const __64 *n, unsigned int hi, unsigned int lo)
{
    return n->u32[0] == hi && n->u32[1] == lo;
}

/* |a - b|.  Every comparison below reads two clocks in sequence, so the
   later read can have crossed a unit boundary and the difference can fall
   either way by one.  Subtracting in the wrong order would underflow and
   __64_to_i32() would hand back the low 31 bits of a near-2^64 value - a
   spurious failure in a test whose whole job is to be trustworthy. */
static int absdiff(const __64 *a, const __64 *b)
{
    __64 x = *a;
    __64 y = *b;
    __64 d;

    if (__64_cmp(&x, &y) == __64_SMALLER) __64_sub(&y, &x, &d);
    else                                  __64_sub(&x, &y, &d);

    /* __64_to_i32() hands back the low 31 bits and DISCARDS the high word
       (src/clib/@@64ti32.c), so a difference of 2^32 would read as 0 - a
       PASS on a wildly wrong value, in the cases that exist to catch
       exactly that.  Anything that does not fit is reported as -1, and
       every caller tests for >= 0. */
    if (d.u32[0] != 0 || (d.u32[1] & 0x80000000U) != 0) return -1;

    return (int)__64_to_i32(&d);
}

/* ------------------------------------------------------------------ */
/* (C) arithmetic: fixed vectors through the divisor layer             */
/* ------------------------------------------------------------------ */

static void div_vec(const char *what, unsigned int ahi, unsigned int alo,
                    unsigned int d, unsigned int qhi, unsigned int qlo)
{
    __64 a;
    __64 q;
    char note[64];

    put64(&a, ahi, alo);
    __64_div_u32(&a, d, &q);
    sprintf(note, "got %08X%08X want %08X%08X",
            (unsigned int)q.u32[0], (unsigned int)q.u32[1],
            (unsigned int)qhi, (unsigned int)qlo);
    check(what, is64(&q, qhi, qlo), note);
}

static void divmod_vec(const char *what, unsigned int ahi, unsigned int alo,
                       unsigned int d, unsigned int qhi, unsigned int qlo,
                       unsigned int rhi, unsigned int rlo)
{
    __64 a;
    __64 q;
    __64 r;
    int  good;
    char note[64];

    put64(&a, ahi, alo);
    __64_divmod_u32(&a, d, &q, &r);
    good = is64(&q, qhi, qlo) && is64(&r, rhi, rlo);

    /* Observed on the case line, expected only on failure: both halves on
       one line would run past 120 columns and SYSPRINT would truncate the
       half that matters. */
    sprintf(note, "got %08X%08X r %08X%08X",
            (unsigned int)q.u32[0], (unsigned int)q.u32[1],
            (unsigned int)r.u32[0], (unsigned int)r.u32[1]);
    check(what, good, note);
    if (!good) {
        printf("  %-46s      want %08X%08X r %08X%08X\n", "",
               (unsigned int)qhi, (unsigned int)qlo,
               (unsigned int)rhi, (unsigned int)rlo);
    }
}

int main(int argc, char **argv)
{
    clock64_t   c1;
    mclock64_t  m1;
    uclock64_t  u1;
    time64_t    t0;
    time64_t    t1;
    time64_t    t2;
    time64_t    tmp;
    time64_t    scaled;
    struct tm  *tm;
    int         year;
    int         delta;
    char        note[96];

    (void)argc;
    (void)argv;

    printf("TSTTM64 - libc370 #49, clock64() returns seconds\n\n");

    /* ============================================================== */
    printf("(A) UNIT CONTRACT - rewritten by the step B fix\n");
    /* ============================================================== */

    /* (1) clock64() and time64() are now the same value: clock64() counts
           seconds and time64() passes it straight through, exactly as
           utime64() does for uclock64() and mtime64() for mclock64().
           Before the fix this case read clock64() == mclock64(). */
    c1 = clock64();
    t1 = time64(NULL);
    tmp.u64 = c1;
    delta = absdiff(&tmp, &t1);
    sprintf(note, "|clock64() - time64()| = %d s", delta);
    check("(1) clock64() == time64() (both seconds)",
          delta >= 0 && delta <= 1, note);

    /* (2) ... and clock64() is now 1000x SMALLER than mclock64(), where it
           used to be equal to it.  clock64() truncates the sub-second part,
           so scaling it back up lands up to 999 ms short of the millisecond
           tier.  Before the fix this case read clock64()/1000 == time64(). */
    c1 = clock64();
    m1 = mclock64();
    tmp.u64 = c1;
    __64_mul_u32(&tmp, 1000, &scaled);
    t0.u64 = m1;
    delta = absdiff(&scaled, &t0);
    sprintf(note, "|clock64()*1000 - mclock64()| = %d ms", delta);
    check("(2) clock64()*1000 == mclock64() (s vs ms)",
          delta >= 0 && delta <= 1000, note);

    /* ============================================================== */
    printf("\n(B) INVARIANT - must stay green across step B\n");
    /* ============================================================== */

    /* (3) THE strongest guard.  time64() feeds gmtime64(), which takes
           seconds; a millisecond value read as seconds lands in year
           58 296, and a seconds/1000 value collapses to 1970. */
    t1 = time64(NULL);
    tm = gmtime64(&t1);
    year = tm ? tm->tm_year + 1900 : -1;
    sprintf(note, "gmtime64(time64()) year = %d", year);
    check("(3) time64() -> gmtime64() lands in this era",
          year >= 2000 && year <= 2100, note);

    /* (4) the same statement without the calendar: seconds since 1970 fit
           in 32 bits until 2106, so the high word MUST be zero.  A
           millisecond value does not fit and would light this up. */
    t1 = time64(NULL);
    sprintf(note, "time64() = %08X%08X", (unsigned int)t1.u32[0], (unsigned int)t1.u32[1]);
    check("(4) time64() has a seconds-since-1970 size",
          t1.u32[0] == 0 && t1.u32[1] >= 0x386D4380U
                         && t1.u32[1] <= 0xF4865700U, note);

    /* (5) the three tiers agree with each other, whatever the absolute
           unit turns out to be: us/1000 is ms, ms/1000 is s.  These hold
           before AND after the fix - only clock64() moves, not this. */
    u1 = uclock64();
    m1 = mclock64();
    tmp.u64 = u1;
    __64_div_u32(&tmp, 1000, &scaled);
    t0.u64 = m1;
    delta = absdiff(&scaled, &t0);
    sprintf(note, "|uclock64()/1000 - mclock64()| = %d ms", delta);
    check("(5a) uclock64()/1000 == mclock64()",
          delta >= 0 && delta <= 1000, note);

    m1 = mclock64();
    t1 = time64(NULL);
    tmp.u64 = m1;
    __64_div_u32(&tmp, 1000, &scaled);
    delta = absdiff(&scaled, &t1);
    sprintf(note, "|mclock64()/1000 - time64()| = %d s", delta);
    check("(5b) mclock64()/1000 == time64()",
          delta >= 0 && delta <= 2, note);

    /* (6) monotonicity: the clock does not run backwards. */
    t0 = time64(NULL);
    t1 = time64(NULL);
    t2 = time64(NULL);
    sprintf(note, "%08X %08X %08X", (unsigned int)t0.u32[1], (unsigned int)t1.u32[1],
            (unsigned int)t2.u32[1]);
    check("(6) time64() is non-decreasing",
          __64_cmp(&t1, &t0) != __64_SMALLER &&
          __64_cmp(&t2, &t1) != __64_SMALLER, note);

    /* (7) the same for the microsecond tier, which has the resolution to
           show a difference at all. */
    u1 = uclock64();
    t0.u64 = u1;
    u1 = uclock64();
    t1.u64 = u1;
    sprintf(note, "second read is %d us later", absdiff(&t1, &t0));
    check("(7) uclock64() is non-decreasing",
          __64_cmp(&t1, &t0) != __64_SMALLER, note);

    /* (8) difftime64() over two reads of the same tier stays sane.  Its
           correctness presumes seconds too: it converts only the LOW 32
           bits of the difference (src/time64/tm64dtim.c:26). */
    t0 = time64(NULL);
    t1 = time64(NULL);
    sprintf(note, "difftime64 = %d s", (int)difftime64(t1, t0));
    check("(8) difftime64(now, now) is ~0",
          difftime64(t1, t0) >= 0.0 && difftime64(t1, t0) <= 2.0, note);

    /* (9) THE THREAD-MANAGER GUARD.  This is dispatch_work()'s arithmetic,
           verbatim, over a real two-second wait.  1..3 is seconds; ~2000
           is milliseconds; 0 is seconds/1000.  See the header. */
    t0 = time64(NULL);
    __asm__("STIMER WAIT,BINTVL==F'200'   2.00 seconds");
    t1 = time64(NULL);
    __64_sub(&t1, &t0, &tmp);
    delta = __64_to_i32(&tmp);
    sprintf(note, "delta over a 2 s wait = %d (ms would be ~2000)", delta);
    check("(9) time64() counts SECONDS (dispatch_work)",
          delta >= 1 && delta <= 3, note);

    /* ============================================================== */
    printf("\n(C) ARITHMETIC - the divisor layer the fix stands on\n");
    /* ============================================================== */

    /* The reporter's measured moment expressed in microseconds,
       1777488851576000, divided both ways.  These are the two divisors
       step B chooses between. */
    div_vec("(10) 1777488851576000 / 1000",
            0x0006509DU, 0xDF9724C0U, 1000, 0x0000019DU, 0xDA977278U);
    div_vec("(11) 1777488851576000 / 1000000",
            0x0006509DU, 0xDF9724C0U, 1000000, 0x00000000U, 0x69F253D3U);

    divmod_vec("(12) ... /1000000 with remainder",
               0x0006509DU, 0xDF9724C0U, 1000000,
               0x00000000U, 0x69F253D3U, 0x00000000U, 0x0008CA00U);

    /* boundaries: zero, just under, and exactly one */
    div_vec("(13) 0 / 1000", 0, 0, 1000, 0, 0);
    divmod_vec("(14) 999 / 1000 = 0 rem 999",
               0, 999, 1000, 0, 0, 0, 999);
    div_vec("(15a) 1000 / 1000 = 1", 0, 1000, 1000, 0, 1);
    div_vec("(15b) 1000000 / 1000000 = 1", 0, 1000000, 1000000, 0, 1);

    /* a value with the top halfword populated, still below the 2^63 point
       where __64_div's own overflow guard takes over */
    divmod_vec("(16a) (2^62+12345) / 1000",
               0x40000000U, 0x00003039U, 1000,
               0x0010624DU, 0xD2F1AA08U, 0x00000000U, 0x000000F9U);
    divmod_vec("(16b) (2^62+12345) / 1000000",
               0x40000000U, 0x00003039U, 1000000,
               0x00000431U, 0xBDE82D7BU, 0x00000000U, 0x00061B79U);

    printf("\nTSTTM64 %s\n", bad ? "FAILED" : "PASSED");
    if (bad) {
        printf("  A class (B) failure is a finding about the library or the\n");
        printf("  system, not an expectation to adjust.  Check tm64clck.c\n");
        printf("  and tm64time.c before touching this file.\n");
    }

    return bad ? 8 : 0;
}
