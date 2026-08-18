/*
 * tst75snd.c - libc370 #120: send() (src/dyn75/@@75send.c) must honour the
 * X'75' retry code -2, and must not stop on a short write.
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
 * The same emulator change makes a SHORT write reachable for the first time:
 * MSG_DONTWAIT sends what fits and reports it.  The pre-fix send() was a
 * single-shot call, so it passed that partial count up.  ftpd_data_send()
 * loops and survives that; the control-socket sends (ftpd#ses.c:115 and
 * friends) discard the return value entirely and would silently truncate a
 * response line.  The fix therefore loops on the remainder, exactly as
 * @@75recv.c loops on the bytes still wanted.
 *
 * THIS TEST LINKS AND EXECUTES THE REAL @@75send.c.  What it substitutes is
 * __75() - the one function that reaches the X'75' instruction - by a fake
 * emulator driven by a scripted list of return codes.
 *
 * ====================================================================
 * WHY THE FAKE MUTATES THE PARAMETER LIST
 * --------------------------------------------------------------------
 * This is the whole point of the test, and the trap the fix has to clear.
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
 * it moved.  A retry that reuses it asks the emulator to send zero bytes from
 * a pointer already past the buffer - a silent no-op that would loop forever
 * against a real -2.  @@75recv.c re-does its XC clear and re-sets r6/r7/r8/r9
 * inside its loop for exactly this reason; it is not a stylistic habit.
 *
 * fake__75() therefore reproduces that write-back (r0, r1, r2, r3, r5, r14)
 * before returning, and records the (offset, length) of every request.  A fix
 * that forgot to rebuild the list would show up here as a second request of
 * length 0 at offset len, not as a passing test.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The loop and the retry: which requests reach the emulator, in what order,
 *   with what offset and length, and what send() returns for each scripted
 *   reply.  Nothing below touches a socket.
 *
 * - Not the STIMER.  The recipe's -D'__asm__(...)=' removes the XC clear and
 *   the STIMER WAIT together (neither assembles on the host, and a real 0.08 s
 *   wait per retry would make the suite crawl).  Removing the XC is load-
 *   bearing rather than a loss: with it gone, ONLY the explicit C assignments
 *   survive, so a fix that hoisted them out of the loop fails here.  The wait
 *   itself is by inspection of @@75send.s - it is the same one line, with the
 *   same R0/R1 clobber, that @@75recv.c has shipped for years.
 *
 * - Not that -2 actually arrives.  That needs the patched emulator and a peer
 *   that stops reading; the end-to-end gate is mvslovers/httpd ->
 *   docs/hercules-x75-send-stall-repro.md, driven against ftpd.
 *
 * - Not the 4096-byte chunking @@75recv.c documents.  It is deliberately NOT
 *   mirrored here: x75.c's copy loop is direction-symmetric (same 255-byte
 *   segmentation, same effective_addr2 += i), lar_tcpip() sets len_in from R1
 *   with no cap, and neither x75.c nor tcpip.c contains a 4096 anywhere.
 *   Whatever the recv comment observed, its cause is not in the guest/host
 *   copy, and adding round trips to the send path for it would cost an
 *   emulator-side malloc/free per chunk for a hazard nobody has measured.
 *
 * - The retry is UNBOUNDED, and this test pins that (case 2).  A budget that
 *   gave up with -1/EWOULDBLOCK would re-create the reported bug rather than
 *   fix it: ftpd_data_send() treats rc <= 0 as fatal, so a bound would only
 *   move the point at which a slow client kills a download - and a blocking
 *   socket reporting EWOULDBLOCK is a contract no POSIX caller expects.  The
 *   stall budget belongs in the consumer, which knows about quiesce and
 *   shutdown; httpd already has one (SEND_STALL_MAX in httpsend.c).
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
 *     ./t                                             # 47/47, rc 0
 *
 * Five warnings are expected and are all host artefacts: two libc370 stdio
 * prototypes that disagree with the host's builtins, one inline-asm operand
 * width in clibstr.h, the `unused parameter 'flags'` send() has always had,
 * and `cast to smaller integer type` on  pl.r5 = (unsigned) p  - which is the
 * 24-bit target's native pointer width and the reason the fake below works in
 * offsets rather than dereferencing r5.  The target build (cc370, -Wall
 * -Werror) is clean.
 *
 * RED, against the pre-fix source - same driver, same script:
 *
 *     git show <pre-fix-rev>:src/dyn75/@@75send.c > /tmp/old75send.c
 *     ... and #include that instead.
 *
 *     FAIL 1: -2 then success: rc                    got -2, want 11
 *     FAIL 1: -2 then success: requests              got 1, want 2
 *     FAIL 1: -2 then success: retry length          got 0, want 11
 *     FAIL 1: -2 then success: bytes on the wire     got 0, want 11
 *     FAIL 2: five -2 in a row: rc                   got -2, want 11
 *     FAIL 2: five -2 in a row: requests             got 1, want 6
 *     FAIL 3: short/-2/remainder: rc                 got 4, want 11
 *     FAIL 3: short/-2/remainder: requests           got 1, want 3
 *     FAIL 5: -1 after partial: requests             got 1, want 2
 *     ...
 *     25/47 checks passed
 *
 * The pre-fix file hands the retry code to its caller on the first reply and
 * never reissues - which is the bug, stated as a number.  Cases 4 and 8 pass
 * in RED too, and are meant to: the error path and the ordinary
 * everything-in-one-go path must come out of this fix unchanged.
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

#define MAXREQ  32

/* one scripted reply, in EZASOKET terms: what lands in R4 */
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

static void
script_reset(const char *payload, const int *replies, int n)
{
    int i;

    base    = payload;
    base32  = (unsigned)(unsigned long)payload;

    for (i = 0; i < n; i++) script[i] = replies[i];
    nscript = n;

    nreplies = nreq = nerrfetch = 0;
    wirelen  = 0;
    sockerr  = 0;
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
               "(retry loop not converging)\n", nreq + 1);
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

/* every request the emulator saw, as "off/len off/len ..." */
static const char *
requests(void)
{
    static char txt[MAXREQ * 16];
    char        *p = txt;
    int         i;

    *p = 0;
    for (i = 0; i < nreq; i++)
        p += sprintf(p, "%s%u/%u", i ? " " : "", req_off[i], req_len[i]);
    return txt;
}

static void
show(const char *title)
{
    printf("     %-24s requests: [%s]  wire: %u byte(s)\n",
           title, requests(), wirelen);
}

#define PAYLOAD "HELLO WORLD"           /* 11 bytes, no terminator sent */
#define PLEN    ((int)(sizeof PAYLOAD - 1))

int main(void)
{
    static const char payload[] = PAYLOAD;
    int rc;

    /* ---------------------------------------------------------------
    ** 1. the reported case: one -2, then the socket drains.
    **    send() must reissue the SAME request - offset 0, all 11 bytes -
    **    and report the full length, not the retry code.
    */
    {
        static const int s[] = { -2, PLEN };
        script_reset(payload, s, 2);

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
    }

    /* ---------------------------------------------------------------
    ** 2. a peer that keeps the buffer full for a while.  The retry is
    **    unbounded by design (see the header): five in a row is not a
    **    failure, it is the contract.
    */
    {
        static const int s[] = { -2, -2, -2, -2, -2, PLEN };
        int i;
        script_reset(payload, s, 6);

        rc = send(4, payload, PLEN, 0);
        show("2: five -2 in a row");

        CHECK_EQ(rc,      PLEN, "2: five -2 in a row: rc");
        CHECK_EQ(nreq,       6, "2: five -2 in a row: requests");
        CHECK_EQ(wirelen, PLEN, "2: five -2 in a row: bytes on the wire");
        for (i = 0; i < nreq; i++) {
            /* nothing went out, so every reissue repeats the whole buffer */
            if (req_off[i] != 0 || req_len[i] != (unsigned) PLEN) {
                fails++;
                printf("FAIL %-52s request %d is %u/%u, want 0/%d\n",
                       "2: five -2 in a row: every retry is 0/11",
                       i, req_off[i], req_len[i], PLEN);
                break;
            }
        }
        checks++;
    }

    /* ---------------------------------------------------------------
    ** 3. short write, then -2, then the remainder.  This is the shape
    **    MSG_DONTWAIT newly makes reachable, and the one the pre-fix
    **    single-shot call passed straight up to a caller that discards it.
    */
    {
        static const int s[] = { 4, -2, 7 };
        script_reset(payload, s, 3);

        rc = send(4, payload, PLEN, 0);
        show("3: short, -2, remainder");

        CHECK_EQ(rc,       PLEN, "3: short/-2/remainder: rc");
        CHECK_EQ(nreq,        3, "3: short/-2/remainder: requests");
        CHECK_EQ(req_off[0],  0, "3: short/-2/remainder: req 1 offset");
        CHECK_EQ(req_len[0], PLEN, "3: short/-2/remainder: req 1 length");
        CHECK_EQ(req_off[1],  4, "3: short/-2/remainder: req 2 offset");
        CHECK_EQ(req_len[1],  7, "3: short/-2/remainder: req 2 length");
        CHECK_EQ(req_off[2],  4, "3: short/-2/remainder: req 3 offset");
        CHECK_EQ(req_len[2],  7, "3: short/-2/remainder: req 3 length");
        CHECK_EQ(wirelen,  PLEN, "3: short/-2/remainder: bytes on the wire");
        CHECK_EQ(memcmp(wire, payload, PLEN), 0,
                                 "3: short/-2/remainder: wire content");
    }

    /* ---------------------------------------------------------------
    ** 4. a dead socket, nothing sent: -1 reaches the caller with errno
    **    from the emulator's function code 2.  This is the path httpd
    **    takes on EWOULDBLOCK (it sets FIONBIO, so it never sees -2),
    **    and it has to be exactly what it was before the fix.
    */
    {
        static const int s[] = { -1 };
        script_reset(payload, s, 1);
        sockerr   = EWOULDBLOCK;
        the_errno = 0;

        rc = send(4, payload, PLEN, 0);
        show("4: -1, nothing sent");

        CHECK_EQ(rc,          -1, "4: -1 nothing sent: rc");
        CHECK_EQ(nreq,         1, "4: -1 nothing sent: requests");
        CHECK_EQ(wirelen,      0, "4: -1 nothing sent: bytes on the wire");
        CHECK_EQ(nerrfetch,    1, "4: -1 nothing sent: errno fetched once");
        CHECK_EQ(the_errno, EWOULDBLOCK,
                                  "4: -1 nothing sent: errno value");
    }

    /* ---------------------------------------------------------------
    ** 5. the error arrives after some bytes did go out.  POSIX reports
    **    the count; the error is rediscovered on the next call.  errno is
    **    still set - the same thing @@75recv.c does, and unspecified on a
    **    non-negative return.
    */
    {
        static const int s[] = { 5, -1 };
        script_reset(payload, s, 2);
        sockerr   = ECONNRESET;
        the_errno = 0;

        rc = send(4, payload, PLEN, 0);
        show("5: -1 after a partial");

        CHECK_EQ(rc,          5, "5: -1 after partial: rc is the count");
        CHECK_EQ(nreq,        2, "5: -1 after partial: requests");
        CHECK_EQ(req_off[1],  5, "5: -1 after partial: req 2 offset");
        CHECK_EQ(req_len[1],  6, "5: -1 after partial: req 2 length");
        CHECK_EQ(wirelen,     5, "5: -1 after partial: bytes on the wire");
        CHECK_EQ(memcmp(wire, payload, 5), 0,
                                 "5: -1 after partial: wire content");
        CHECK_EQ(the_errno, ECONNRESET,
                                 "5: -1 after partial: errno value");
    }

    /* ---------------------------------------------------------------
    ** 6. a zero-progress reply that is not -2 and not -1 stops the loop.
    **    0 from send() is not a state the emulator produces for a non-empty
    **    request, but the guard has to be there or the loop spins.
    */
    {
        static const int s[] = { 3, 0 };
        script_reset(payload, s, 2);

        rc = send(4, payload, PLEN, 0);
        show("6: 0 after a partial");

        CHECK_EQ(rc,       3, "6: 0 after partial: rc is the count");
        CHECK_EQ(nreq,     2, "6: 0 after partial: requests");
        CHECK_EQ(wirelen,  3, "6: 0 after partial: bytes on the wire");
    }

    /* ---------------------------------------------------------------
    ** 7. nothing to send: no X'75' at all, and no uninitialised PL75 read.
    **    The loop gets this for free, the pre-fix single-shot call did not.
    */
    {
        static const int s[] = { 0 };
        script_reset(payload, s, 1);
        the_errno = 0;

        rc = send(4, payload, 0, 0);
        show("7: len 0");

        CHECK_EQ(rc,        0, "7: len 0: rc");
        CHECK_EQ(nreq,      0, "7: len 0: requests");
        CHECK_EQ(nerrfetch, 0, "7: len 0: no errno fetch");
        CHECK_EQ(the_errno, 0, "7: len 0: errno untouched");
    }

    /* ---------------------------------------------------------------
    ** 8. the whole buffer in one go, the ordinary case: one request, one
    **    reply, no retry, no errno fetch.  A fix that reissued after a
    **    complete send would show up here.
    */
    {
        static const int s[] = { PLEN };
        script_reset(payload, s, 1);
        the_errno = 0;

        rc = send(4, payload, PLEN, 0);
        show("8: straight through");

        CHECK_EQ(rc,       PLEN, "8: straight through: rc");
        CHECK_EQ(nreq,        1, "8: straight through: requests");
        CHECK_EQ(req_off[0],  0, "8: straight through: offset");
        CHECK_EQ(req_len[0], PLEN, "8: straight through: length");
        CHECK_EQ(wirelen,  PLEN, "8: straight through: bytes on the wire");
        CHECK_EQ(nerrfetch,   0, "8: straight through: no errno fetch");
    }

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    return fails ? 1 : 0;
}
