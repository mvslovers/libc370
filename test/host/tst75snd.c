/*
 * tst75snd.c - libc370 #120: send() (src/dyn75/@@75send.c) must honour the
 * X'75' retry code -2, wait for it on a bounded budget, and change nothing
 * else about what it returns.
 *
 * ISSUE #120: Hercules' X'75' emulation used to call a BLOCKING host send()
 * on the emulated CPU thread.  A client that stopped reading filled the host
 * send buffer, the guest send() blocked, the CPU stopped making progress, and
 * the Hercules watchdog killed the whole emulator by design (impl.c CRASH()).
 * mvslovers/hyperion 1a599b0d made that send() non-blocking and gave it the
 * return contract RECV and ACCEPT have always had:
 *
 *     non-blocking guest socket (FIONBIO)  ->  -1 with EWOULDBLOCK
 *     blocking guest socket (the default)  ->  -2, "wait and reissue"
 *
 * -2 is not a new code - tcpip.c has returned it from RECV and ACCEPT since
 * forever, and @@75recv.c has always had the matching STIMER wait.  It is
 * simply now reachable from SEND as well, and @@75send.c had no branch for
 * it: it tested only for -1 and handed everything else back as a byte count.
 * A consumer on a blocking socket got -2 where it expected a length.  ftpd is
 * the one that shows it: ftpd_data_send() (src/ftpd#dat.c:314) treats rc <= 0
 * as fatal, so a client slower than the server aborted the transfer.  httpd
 * is not affected - it sets FIONBIO on every accepted socket and therefore
 * takes the -1/EWOULDBLOCK path its own send layer already handles.
 *
 * THIS TEST LINKS AND EXECUTES THE REAL @@75send.c.  What it substitutes is
 * __75() - the one function that reaches the X'75' instruction - by a fake
 * emulator driven by a scripted list of return codes.
 *
 * ====================================================================
 * WHY THE FAKE MUTATES THE PARAMETER LIST
 * --------------------------------------------------------------------
 * This is the trap the fix has to clear, and case 1 is what catches it.
 *
 * @@75.s ends the sequence with  STM 0,15,0(11)  - it stores ALL SIXTEEN
 * registers back into the caller's PL75.  During a SEND, x75.c's guest-to-host
 * copy loop runs
 *
 *     (regs->GR_L(b2)) += i;      // b2 = 5, the buffer register
 *     (regs->GR_L(1))  -= i;      // the byte counter
 *
 * to completion, and the second X'75' then sets R1 = len_out, which is 0 for
 * a SEND.  So the list comes BACK with r1 = 0 and r5 advanced past the bytes
 * it moved.  A retry that reused it would ask the emulator to send zero bytes
 * from a pointer already past the buffer - a silent no-op that would burn the
 * whole stall budget against a peer that had in fact drained.  @@75recv.c
 * re-does its XC clear and re-sets r6/r7/r8/r9 inside its loop for exactly
 * this reason; it is not a stylistic habit.
 *
 * fake__75() therefore reproduces that write-back (r0, r1, r2, r3, r5, r14)
 * before returning, and records the (offset, length) of every request.  A fix
 * that forgot to rebuild the list would show up here as a second request of
 * length 0 at offset len, not as a passing test.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The retry hangs off the -2 branch and NOTHING ELSE.  Case 4 is the guard:
 *   a short write is a byte count, returned as one, after exactly ONE X'75'.
 *   send() must not accumulate over partial sends - that would change its
 *   semantics for every caller, and httpd builds its state machine on the
 *   partial return (src/httpfile.c).  -2 needs no accumulation anyway: it
 *   always means zero bytes went out, so the identical buffer is reissued.
 *
 * - The budget, both sides of it (cases 3a and 3b): 100 waits of 0.10 s, then
 *   -1 with EWOULDBLOCK.  Same policy as httpd's own send layer
 *   (SEND_STALL_MAX / SEND_STALL_PAUSE, httpd/include/httpd.h), so a stalled
 *   peer is torn down on one number across the ecosystem rather than three.
 *   The errno on that path is set in C, not fetched: Cerr[] holds nothing for
 *   a -2, so there is no emulator errno to ask for, and case 3b pins that no
 *   function-code-2 call is made.
 *
 * - Nothing else moves.  Cases 4 to 7 are the pre-fix behaviour, unchanged:
 *   an unpatched Hercules can never return -2 from SEND (its case 10 yields
 *   only -1 or a count), so on such a system the new branch is dead code and
 *   this file says so in numbers - every one of those cases passes against
 *   the pre-fix source too.
 *
 * - Not the STIMER.  The recipe's -D'__asm__(...)=' removes the XC clear and
 *   the STIMER WAIT together (neither assembles on the host, and a real
 *   budget of 100 x 0.10 s would put 10 s of sleep into the suite).  Removing
 *   the XC is load-bearing rather than a loss: with it gone, ONLY the explicit
 *   C assignments survive, so a fix that hoisted them out of the loop fails
 *   here.  The wait itself is by inspection of @@75send.s - the same one line,
 *   with the same R0/R1 clobber, that @@75recv.c has shipped for years, with
 *   BINTVL=F'10' for the 100 ms this policy asks for (BINTVL counts
 *   hundredths).  Nothing live crosses it: SVC 47 preserves R2-R13, and R12
 *   is reloaded from the page table behind it.
 *
 * - Not that -2 actually arrives.  That needs the patched emulator and a peer
 *   that stops reading; the end-to-end gate is mvslovers/httpd ->
 *   docs/hercules-x75-send-stall-repro.md, driven against ftpd.
 *
 * - Not the 4096-byte chunking @@75recv.c documents.  It is deliberately NOT
 *   mirrored: x75.c's copy loop is direction-symmetric (same 255-byte
 *   segmentation, same effective_addr2 += i), lar_tcpip() sets len_in from R1
 *   with no cap, and grep finds no 4096, 4095 or 0x1000 in x75.c, tcpip.c or
 *   their headers.  Whatever the recv comment observed, its cause is not in
 *   the guest/host copy.
 *
 * ====================================================================
 * BUILD AND RUN
 * --------------------------------------------------------------------
 *   -D'__asm__(...)='   drop the inline asm (see above)
 *   -D'asm(...)='       drop the  asm("@@75SEND")  symbol labels socket.h
 *                       puts on every declaration
 *   -D__32BIT__         what libc370's stddef.h keys size_t off
 *   -Wno-trigraphs      socket.h's OR/BOR are spelled with ??!
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-trigraphs \
 *        -D'__asm__(...)=' -D'asm(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tst75snd.c
 *     ./t                                             # 39/39, rc 0
 *
 * Four warnings are expected and are all host artefacts: two libc370 stdio
 * prototypes that disagree with the host's builtins, one inline-asm operand
 * width in clibstr.h, and the `unused parameter 'flags'` send() has always
 * had.  The target build (cc370, -Wall) is clean.
 *
 * RED, against the pre-fix source - same driver, same script.  The pre-fix
 * file has no SEND_STALL_MAX, so the driver gets the number on the command
 * line instead:
 *
 *     git show <pre-fix-rev>:src/dyn75/@@75send.c > /tmp/old75send.c
 *     ... and #include that instead, adding -DSEND_STALL_MAX=100
 *
 *     FAIL 1: -2 then success: rc                    got -2, want 11
 *     FAIL 1: -2 then success: requests              got 1, want 2
 *     FAIL 1: -2 then success: retry length          got 0, want 11
 *     FAIL 1: -2 then success: bytes on the wire     got 0, want 11
 *     FAIL 2: five -2 in a row: rc                   got -2, want 11
 *     FAIL 2: five -2 in a row: requests             got 1, want 6
 *     FAIL 3a: 100 -2 then success: rc               got -2, want 11
 *     FAIL 3a: 100 -2 then success: requests         got 1, want 101
 *     FAIL 3b: budget exhausted: rc                  got -2, want -1
 *     FAIL 3b: budget exhausted: requests            got 1, want 101
 *     FAIL 3b: budget exhausted: errno               got 0, want 35
 *     ...
 *     25/39 checks passed
 *
 * The pre-fix file hands the retry code to its caller on the first reply and
 * never reissues - which is the bug, stated as a number.  Every check in
 * cases 4 to 7 passes in RED as well, and is meant to: that is the behaviour
 * this fix must not touch, and the reason a system with an unpatched Hercules
 * sees no change at all.
 *
 * ====================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/dyn75/@@75send.c"

/* ---- the fake emulator ------------------------------------------------
 * Defined AFTER the translation unit so the prototype the library's own
 * header declares is the one that has to match.
 */

/* one more than the 101 attempts the budget allows, so an off-by-one in
** either direction is a diagnosable overrun rather than an array overflow */
#define MAXREQ  (SEND_STALL_MAX + 8)

/* the scripted replies, in EZASOKET terms: what lands in R4 */
static int      script[MAXREQ];
static int      nscript;
static int      nreplies;               /* replies consumed so far      */

/* what the emulator was asked to do, request by request */
static unsigned req_off[MAXREQ];        /* offset into the payload      */
static unsigned req_len[MAXREQ];        /* byte count it was given      */
static int      nreq;

/* the bytes that reached the "host", in the order they got there */
static char     wire[256];
static unsigned wirelen;

static int      sockerr;                /* Cerr[] for function code 2   */
static int      nerrfetch;              /* how often it was asked for   */

/* the payload and its address as the 24-bit target sees it.  send() puts the
** buffer through `unsigned` (pl.r5), which is exact on MVS and truncating on
** a 64-bit host, so the fake never dereferences r5: it works in offsets from
** this base, and a difference of two truncated pointers is exact. */
static const char  *base;
static unsigned     base32;

/* fill the script with `n` copies of `reply`, then `tail` (skipped if the
** script is already full).  Returns the number of replies scripted. */
static int
script_reset(const char *payload, int reply, int n, int tail, int use_tail)
{
    int i;

    base    = payload;
    base32  = (unsigned)(unsigned long)payload;

    if (n > MAXREQ) n = MAXREQ;
    for (i = 0; i < n; i++) script[i] = reply;
    if (use_tail && n < MAXREQ) script[n++] = tail;
    nscript = n;

    nreplies = nreq = nerrfetch = 0;
    wirelen  = 0;
    sockerr  = 0;
    return nscript;
}

int
__75(PL75 *pl)
{
    unsigned    off;
    unsigned    len;
    int         ret;

    if (pl->r7 == 2) {                  /* get error */
        nerrfetch++;
        pl->r1 = 0;                     /* len_out */
        pl->r4 = (unsigned) sockerr;
        return 0;
    }

    if (pl->r7 != 10) {                 /* nothing else may reach here */
        printf("FAIL fake__75: unexpected function code %u\n", pl->r7);
        exit(2);
    }

    if (nreplies >= nscript || nreq >= MAXREQ) {
        printf("FAIL fake__75: request %d past the end of the script "
               "(retry budget not honoured)\n", nreq + 1);
        exit(2);
    }

    off = pl->r5 - base32;
    len = pl->r1;

    req_off[nreq] = off;
    req_len[nreq] = len;
    nreq++;

    ret = script[nreplies++];

    /* the host got whatever send() accepted - which is the scripted count,
    ** and never more than it was offered */
    if (ret > 0) {
        if ((unsigned) ret > len) {
            printf("FAIL fake__75: script returns %d for a %u-byte request\n",
                   ret, len);
            exit(2);
        }
        if (wirelen + (unsigned) ret > sizeof wire) {
            printf("FAIL fake__75: wire overflow\n");
            exit(2);
        }
        memcpy(&wire[wirelen], base + off, (size_t) ret);
        wirelen += (unsigned) ret;
    }

    /* ---- the register write-back, as @@75.s performs it -----------------
    ** First X'75' (R3=0): lar_tcpip() hands out the buffer slot, the copy
    ** loop drains R1 and advances R5, then EZASOKET runs.  Second X'75'
    ** (R3=1): R1 = len_out (0 for a SEND), R2 = buffer_out_slot, R4 = ret_cd,
    ** and the dealloc pass clears R14.  STM 0,15,0(11) stores the lot.
    */
    pl->r0  = 1;
    pl->r5 += len;                      /* advanced past the bytes copied  */
    pl->r1  = 0;                        /* len_out                         */
    pl->r2  = 0xDEAD;                   /* buffer_out_slot: never set for  */
                                        /* a SEND, so it is stale here too */
    pl->r3  = 1;
    pl->r14 = 0;
    pl->r15 = 0;
    pl->r4  = (unsigned) ret;

    return 0;
}

/* errno, as <errno.h> reaches it */
static int the_errno;
int *__errno(void) { return &the_errno; }

/* ---- harness ---------------------------------------------------------- */

static int checks = 0, fails = 0;

#define CHECK_EQ(got, want, what)                                       \
    do {                                                                \
        long g_ = (long)(got), w_ = (long)(want);                       \
        checks++;                                                       \
        if (g_ != w_) {                                                 \
            fails++;                                                    \
            printf("FAIL %-52s got %ld, want %ld\n", (what), g_, w_);   \
        } else {                                                        \
            printf("ok   %-52s %ld\n", (what), g_);                     \
        }                                                               \
    } while (0)

/* every request must repeat the whole buffer from the start: -2 means
** nothing went out, and the parameter list is rebuilt for each attempt */
static void
check_all_requests_whole(int len, const char *what)
{
    int i;

    checks++;
    for (i = 0; i < nreq; i++) {
        if (req_off[i] != 0 || req_len[i] != (unsigned) len) {
            fails++;
            printf("FAIL %-52s request %d is %u/%u, want 0/%d\n",
                   what, i + 1, req_off[i], req_len[i], len);
            return;
        }
    }
    printf("ok   %-52s %d request(s), all 0/%d\n", what, nreq, len);
}

static void
show(const char *title)
{
    int         i;
    int         shown = nreq < 4 ? nreq : 4;
    char        txt[80];
    char        *p = txt;

    *p = 0;
    for (i = 0; i < shown; i++)
        p += sprintf(p, "%s%u/%u", i ? " " : "", req_off[i], req_len[i]);
    if (shown < nreq) sprintf(p, " ... (%d total)", nreq);

    printf("     %-24s requests: [%s]  wire: %u byte(s)\n",
           title, txt, wirelen);
}

#define PAYLOAD "HELLO WORLD"           /* 11 bytes, no terminator sent */
#define PLEN    ((int)(sizeof PAYLOAD - 1))

int main(void)
{
    static const char payload[] = PAYLOAD;
    int rc;

    /* ---------------------------------------------------------------
    ** 1. the reported case: one -2, then the socket drains.  send() must
    **    reissue the SAME request - offset 0, all 11 bytes - and report the
    **    length, not the retry code.
    */
    script_reset(payload, -2, 1, PLEN, 1);
    the_errno = 0;
    rc = send(4, payload, PLEN, 0);
    show("1: -2 then success");

    CHECK_EQ(rc,       PLEN, "1: -2 then success: rc");
    CHECK_EQ(nreq,        2, "1: -2 then success: requests");
    CHECK_EQ(req_off[1],  0, "1: -2 then success: retry offset");
    CHECK_EQ(req_len[1], PLEN, "1: -2 then success: retry length");
    CHECK_EQ(wirelen,  PLEN, "1: -2 then success: bytes on the wire");
    CHECK_EQ(memcmp(wire, payload, PLEN), 0,
                             "1: -2 then success: wire content");
    CHECK_EQ(nerrfetch,   0, "1: -2 then success: no errno fetch");
    CHECK_EQ(the_errno,   0, "1: -2 then success: errno untouched");

    /* ---------------------------------------------------------------
    ** 2. a peer that keeps the buffer full for a moment.
    */
    script_reset(payload, -2, 5, PLEN, 1);
    rc = send(4, payload, PLEN, 0);
    show("2: five -2 in a row");

    CHECK_EQ(rc,      PLEN, "2: five -2 in a row: rc");
    CHECK_EQ(nreq,       6, "2: five -2 in a row: requests");
    CHECK_EQ(wirelen, PLEN, "2: five -2 in a row: bytes on the wire");
    check_all_requests_whole(PLEN, "2: five -2 in a row: every attempt whole");

    /* ---------------------------------------------------------------
    ** 3a. the last attempt the budget allows still succeeds: 100 waits,
    **     then the 101st X'75' goes through.
    */
    script_reset(payload, -2, SEND_STALL_MAX, PLEN, 1);
    the_errno = 0;
    rc = send(4, payload, PLEN, 0);
    show("3a: 100 -2 then success");

    CHECK_EQ(rc,      PLEN, "3a: 100 -2 then success: rc");
    CHECK_EQ(nreq, SEND_STALL_MAX + 1,
                            "3a: 100 -2 then success: requests");
    CHECK_EQ(wirelen, PLEN, "3a: 100 -2 then success: bytes on the wire");
    CHECK_EQ(the_errno,  0, "3a: 100 -2 then success: errno untouched");

    /* ---------------------------------------------------------------
    ** 3b. the peer never drains.  The budget runs out on the 101st -2 and
    **     send() reports -1/EWOULDBLOCK so the caller can drop the session.
    **     The errno is set in C: Cerr[] holds nothing for a -2, so no
    **     function-code-2 call may be made.
    */
    script_reset(payload, -2, SEND_STALL_MAX + 1, 0, 0);
    the_errno = 0;
    sockerr   = ENOTSOCK;               /* would be wrong if fetched */
    rc = send(4, payload, PLEN, 0);
    show("3b: budget exhausted");

    CHECK_EQ(rc,          -1, "3b: budget exhausted: rc");
    CHECK_EQ(nreq, SEND_STALL_MAX + 1,
                              "3b: budget exhausted: requests");
    CHECK_EQ(the_errno, EWOULDBLOCK,
                              "3b: budget exhausted: errno");
    CHECK_EQ(nerrfetch,    0, "3b: budget exhausted: no errno fetch");
    CHECK_EQ(wirelen,      0, "3b: budget exhausted: bytes on the wire");
    check_all_requests_whole(PLEN, "3b: budget exhausted: every attempt whole");

    /* ---------------------------------------------------------------
    ** 4. THE GUARD.  A short write is a byte count and comes straight back
    **    after ONE X'75'.  send() must not loop over the remainder: httpd
    **    builds its state machine on the partial return, and -2 - the only
    **    thing that retries - never needs accumulation because it always
    **    means zero bytes went out.
    */
    script_reset(payload, 4, 1, 0, 0);
    the_errno = 0;
    rc = send(4, payload, PLEN, 0);
    show("4: short write");

    CHECK_EQ(rc,          4, "4: short write: rc is the partial count");
    CHECK_EQ(nreq,        1, "4: short write: one X'75', no continuation");
    CHECK_EQ(req_len[0], PLEN, "4: short write: it was offered everything");
    CHECK_EQ(wirelen,     4, "4: short write: bytes on the wire");
    CHECK_EQ(nerrfetch,   0, "4: short write: no errno fetch");

    /* ---------------------------------------------------------------
    ** 5. a dead socket: -1 reaches the caller with errno from the
    **    emulator's function code 2.  This is the path httpd takes on
    **    EWOULDBLOCK (it sets FIONBIO, so it never sees -2).
    */
    script_reset(payload, -1, 1, 0, 0);
    sockerr   = ECONNRESET;
    the_errno = 0;
    rc = send(4, payload, PLEN, 0);
    show("5: -1 from the emulator");

    CHECK_EQ(rc,          -1, "5: -1: rc");
    CHECK_EQ(nreq,         1, "5: -1: requests");
    CHECK_EQ(nerrfetch,    1, "5: -1: errno fetched once");
    CHECK_EQ(the_errno, ECONNRESET, "5: -1: errno value");

    /* ---------------------------------------------------------------
    ** 6. the ordinary case: one request, one reply, no retry, no errno.
    **    A fix that reissued after a complete send would show up here.
    */
    script_reset(payload, PLEN, 1, 0, 0);
    the_errno = 0;
    rc = send(4, payload, PLEN, 0);
    show("6: straight through");

    CHECK_EQ(rc,       PLEN, "6: straight through: rc");
    CHECK_EQ(nreq,        1, "6: straight through: requests");
    CHECK_EQ(wirelen,  PLEN, "6: straight through: bytes on the wire");
    CHECK_EQ(nerrfetch,   0, "6: straight through: no errno fetch");

    /* ---------------------------------------------------------------
    ** 7. len 0 still issues its X'75', exactly as before.  The fix is
    **    additive: it adds a branch, it does not add a short circuit.
    */
    script_reset(payload, 0, 1, 0, 0);
    the_errno = 0;
    rc = send(4, payload, 0, 0);
    show("7: len 0");

    CHECK_EQ(rc,         0, "7: len 0: rc");
    CHECK_EQ(nreq,       1, "7: len 0: still one X'75'");
    CHECK_EQ(req_len[0], 0, "7: len 0: length passed through");
    CHECK_EQ(the_errno,  0, "7: len 0: errno untouched");

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
