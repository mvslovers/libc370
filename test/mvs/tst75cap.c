/*
 * tst75cap.c - libc370 #154 red/green for the CAP (MVS target, batch).
 *
 * tst75rst.c measures the EMULATOR: it goes through __75() directly so that
 * one measurement is one pair of X'75' instructions.  This probe measures the
 * GUEST-SIDE FIX instead - the chunk cap in @@75recv.c - and it does so by
 * running the pre-fix loop and the post-fix loop against the same forced
 * fault, in the same job, on the same stand.
 *
 * THE DEFECT, IN ONE PARAGRAPH
 * ----------------------------
 * X'75' copies in 256-byte segments and the instruction is restartable.  A
 * page translation exception on the guest buffer is nullifying, so MVS
 * resolves the page and the instruction runs again from the top.  The guest
 * side resumes correctly - R1 holds the bytes remaining and the base register
 * was advanced before the exception.  The host side has nothing to resume
 * from: upstream x75.c recomputes its pointer from map32[R2] on every entry
 * and R2 is a slot index that never advances, so the remaining bytes come
 * from the START of the host buffer.  The tail of the read is a replay of its
 * head.  One segment is atomic against that exception, so 256 or less either
 * faults having moved nothing - where resuming from the start is correct - or
 * completes.  The defect needs one COMPLETED segment before the fault.
 *
 * WHAT THIS PROBE ADDS
 * --------------------
 * A 4096-byte receive buffer is placed so a page boundary falls at offset
 * 2560, and the page beyond it is released with PGRLSE immediately before the
 * receive.  2560 is not an arbitrary offset: it is the first bad byte in
 * mvslovers/ftpd#122, a 4577-byte ASCII upload that arrived as
 *
 *     orig[0:2560] + orig[0:1536] + orig[4096:4577]
 *
 * so a red case here reproduces that report's fingerprint exactly - same
 * boundary, same 1536-byte replay, same 4096-byte window.
 *
 * Four cases, each a fresh connection over a loopback pair the program owns
 * both ends of:
 *
 *   1  control       cap 4096, boundary 0     the first segment faults having
 *                                             copied nothing; correct on a
 *                                             fixed AND an unfixed emulator,
 *                                             so it must pass either way
 *   2  pre-fix       cap 4096, boundary 2560  RED on an unfixed emulator
 *   3  post-fix      cap  256, boundary 2560  GREEN
 *   4  linked libc   recv(),   boundary 2560  whichever the archive holds
 *
 * Cases 2 and 3 are the loop out of @@75recv.c with the cap as a parameter,
 * so they differ in exactly the one line the fix changes and in nothing else.
 * Case 4 exercises the real function: build this TU once against the sysroot
 * and once against a libc.a from the fix branch and the pair separates them.
 *
 * WHAT A PASS DOES AND DOES NOT PROVE
 * -----------------------------------
 * Case 3 green has two possible causes - the cap kept every copy inside one
 * segment, or the receive never faulted at all - and the guest cannot tell
 * them apart.  It is case 2 RED on the same stand, same buffer, same released
 * page, that makes the pair mean anything: it shows the fault is live here and
 * that 4096 is exposed to it.  If case 2 is green as well, the emulator
 * carries the host-side fix (SDL-Hercules-390/hyperion 4675e7e1) and this run
 * says NOTHING about the cap.  The program prints that verdict itself rather
 * than leaving it to be read off an RC.
 *
 * DETAILS THAT DECIDE WHETHER THE PROBE MEASURES ANYTHING - all inherited
 * from tst75rst.c, which explains each at length:
 *
 * - the byte pattern is i % 251, so a replay restarting at a multiple of 256
 *   cannot hide behind a period that divides 256
 * - the peer sends in 256-byte chunks, because send() has the mirror defect
 *   and no cap at all
 * - the part of the buffer below the boundary is written just before the
 *   receive, so the leading segments certainly complete
 * - PGRLSE is verified to release on this system before any socket is opened;
 *   a PGRLSE that quietly did nothing would look exactly like a fixed emulator
 *
 * BUILD (host), both halves:
 *     cc370 -Iinclude test/mvs/tst75cap.c -flinker-output=iebcopy -o TST75CAP
 *     cc370 -Iinclude -L build/sdk test/mvs/tst75cap.c \
 *           -flinker-output=iebcopy -o TST75CPN
 *
 * RUN: see jcl/tst75cap.jcl.  Built by hand - libc370 is the cc370 sysroot,
 * not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = every expectation met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibwto.h>
#include <socket.h>
#include <__75.h>

#define PAGE        4096            /* MVS page size                        */
#define SEG          256            /* the X'75' copy segment               */
#define BUFLEN      4096            /* bytes per measured receive           */
#define BOUND       2560            /* ftpd#122's first bad byte            */
#define PORTBASE   27600            /* first port tried for the loopback    */
#define PORTTRIES     10
#define WAITMAX      250            /* x 0.08s = 20s for the data to queue  */

#define PATTERN(i)  ((unsigned char)((i) % 251))

/* how the measured receive is issued */
#define M_CAP4096   0               /* the loop as @@75recv.c had it        */
#define M_CAP256    1               /* the loop as the fix leaves it        */
#define M_LIBC      2               /* recv() out of the linked archive     */

/* Over-allocated by a page so a page boundary can be found inside it, and
   malloc'ed rather than static: program statics live in the load module's own
   storage, which PGRLSE need not accept. */
static unsigned char *arena;

/* PGRLSE's high address convention on this system, probed rather than
   assumed: PAGE-1 if the last byte of the page works, PAGE if it wants the
   first byte of the next one.  0 means neither did. */
static unsigned hi_bias;

static int  check(const char *what, int ok);
static int  pgrlse(void *lo, void *hi);
static int  release_page(unsigned char *page);
static void snooze(void);
static int  recv_capped(int s, void *vbuf, int len, int cap);
static int  measure(int mode, int s, void *buf, int len);
static int  send_all(int s, const unsigned char *buf, unsigned len);
static int  wait_for(int s, unsigned want);
static int  run_case(int lsock, unsigned port, int mode, unsigned bound,
                     const char *label, unsigned *first_bad, int *replay);

int main(void)
{
    unsigned char       *base;
    unsigned char       *page1;
    struct sockaddr_in  addr;
    int                 lsock = -1;
    unsigned            port  = 0;
    unsigned            i;
    int                 bad   = 0;
    int                 rc_in, rc_ex;
    int                 ok_in, ok_ex;
    unsigned            fb_ctl, fb_pre, fb_post, fb_libc;
    int                 rp_ctl, rp_pre, rp_post, rp_libc;
    int                 r_ctl, r_pre, r_post, r_libc;

    /* primes the stdio buffers before anything is measured */
    printf("TST75CAP - libc370 #154, the recv() chunk cap\n\n");

    arena = (unsigned char *)malloc(5 * PAGE);
    if (!arena) {
        printf("*** FAIL  no storage for the arena\n");
        wtof("TST75CAP FAILED - no arena");
        return 8;
    }

    base  = (unsigned char *)(((unsigned)arena + PAGE - 1)
                              & ~(unsigned)(PAGE - 1));
    page1 = base + PAGE;

    /* Does PGRLSE release on this system, and which high address does it
       want?  Fill, release, read back: a released page reads as zeros, so if
       it still reads 0xFF nothing was released and every case below would
       pass for the wrong reason. */
    memset(page1, 0xFF, PAGE);
    rc_in = pgrlse(page1, page1 + PAGE - 1);       /* last byte of the page  */
    ok_in = (page1[0] == 0x00 && page1[SEG] == 0x00);

    rc_ex = -1;
    ok_ex = 0;
    if (!ok_in) {
        memset(page1, 0xFF, PAGE);
        rc_ex = pgrlse(page1, page1 + PAGE);       /* first byte beyond it   */
        ok_ex = (page1[0] == 0x00 && page1[SEG] == 0x00);
    }

    hi_bias = ok_in ? (PAGE - 1) : (ok_ex ? PAGE : 0);

    printf("  PGRLSE  hi=page+%u: rc=%d %s / hi=page+%u: rc=%d %s\n",
           PAGE - 1, rc_in, ok_in ? "released" : "no-op",
           PAGE, rc_ex, ok_ex ? "released" : "no-op");
    wtof("TST75CAP pgrlse incl rc=%d ok=%c excl rc=%d ok=%c bias=%u",
         rc_in, ok_in ? 'Y' : 'N', rc_ex, ok_ex ? 'Y' : 'N', hi_bias);

    bad += check("PGRLSE releases a page (reads back zero)", hi_bias != 0);

    if (hi_bias == 0) {
        printf("\n  PGRLSE released nothing - the probe cannot force a fault,\n");
        printf("  so the receive cases are NOT run.  No verdict either way.\n");
        printf("\nTST75CAP FAILED\n");
        wtof("TST75CAP FAILED - pgrlse no-op, cases not run");
        return 8;
    }

    /* one listener for the whole run; a fresh connected pair per case */
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    bad  += check("socket() for the listener", lsock >= 0);

    if (lsock < 0) {
        printf("\nTST75CAP FAILED - no listener\n");
        return 8;
    }

    for (i = 0; i < PORTTRIES; i++) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)(PORTBASE + i));
        addr.sin_addr.s_addr = 0x7F000001;      /* 127.0.0.1 */

        if (bind(lsock, &addr, sizeof(addr)) >= 0) {
            port = PORTBASE + i;
            break;
        }
    }

    bad += check("bind() on a loopback port", port != 0);
    bad += check("listen()", port != 0 && listen(lsock, 5) >= 0);

    if (port == 0) {
        closesocket(lsock);
        printf("\nTST75CAP FAILED - no port\n");
        return 8;
    }

    printf("  listening on 127.0.0.1:%u\n", port);
    printf("  buffer %u bytes, page boundary at %u (%u whole segments below)\n\n",
           BUFLEN, BOUND, BOUND / SEG);

    r_ctl  = run_case(lsock, port, M_CAP4096, 0,
                      "control    cap 4096, boundary    0", &fb_ctl,  &rp_ctl);
    r_pre  = run_case(lsock, port, M_CAP4096, BOUND,
                      "pre-fix    cap 4096, boundary 2560", &fb_pre,  &rp_pre);
    r_post = run_case(lsock, port, M_CAP256,  BOUND,
                      "post-fix   cap  256, boundary 2560", &fb_post, &rp_post);
    r_libc = run_case(lsock, port, M_LIBC,    BOUND,
                      "linked     recv(),   boundary 2560", &fb_libc, &rp_libc);

    closesocket(lsock);

    /* --------------------------------------------------------------
    ** The verdict.  The control must be clean on any emulator; the
    ** pre-fix case is what decides whether this stand can show the
    ** defect at all, and only if it can does the post-fix case mean
    ** anything.
    ** ----------------------------------------------------------- */
    printf("\n  verdict\n");

    if (r_ctl != 0) {
        printf("    the control case did not pass.  Something other than\n");
        printf("    this defect is wrong; the other cases mean nothing.\n");
        bad++;
    } else if (r_pre < 0) {
        /* run_case() could not take the measurement at all - a failed
           connect, a short send, a receive that did not return BUFLEN.  That
           is not a red case, and reporting it as one would put a first bad
           byte of 0 in front of a reader as if it were the defect. */
        printf("    the pre-fix case could not be MEASURED (connect, send or\n");
        printf("    receive failed).  No verdict either way - fix the probe's\n");
        printf("    own plumbing and re-run.\n");
        bad++;
    } else if (r_pre == 0) {
        printf("    cap 4096 is CLEAN on this stand, so the emulator carries\n");
        printf("    the host-side fix (hyperion 4675e7e1) or the receive never\n");
        printf("    faulted.  This run says NOTHING about the cap - it is not\n");
        printf("    a pass for the fix and not a failure of it.\n");
        printf("    Re-run against an emulator without 4675e7e1.\n");
    } else {
        printf("    cap 4096 REPRODUCED the defect (first bad byte %u%s, tail\n",
               fb_pre, rp_pre ? " a clean replay from the buffer head" : "");
        printf("    %s).  The fault is live on this stand.\n",
               rp_pre ? "matches ftpd#122" : "corrupt but not a clean replay");

        if (r_post < 0) {
            printf("    cap 256 could not be MEASURED - no verdict on the fix.\n");
            bad++;
        } else if (r_post == 0) {
            printf("    cap 256 is CLEAN against the same fault: the fix holds.\n");
        } else {
            printf("    cap 256 is ALSO corrupt (first bad byte %u) - the fix\n",
                   fb_post);
            printf("    does NOT hold and 256 is not the safe bound.\n");
            bad++;
        }

        if (r_libc < 0) {
            printf("    linked recv() could not be MEASURED - this run does not\n");
            printf("    say which libc the module was built against.\n");
            bad++;
        } else {
            printf("    linked recv() is %s",
                   r_libc == 0 ? "CLEAN" : "corrupt");
            if (r_libc != 0) printf(" (first bad byte %u)", fb_libc);
            printf(" - this build links the %s libc.\n",
                   r_libc == 0 ? "FIXED" : "pre-fix");
        }
    }

    printf("\nTST75CAP %s\n", bad ? "FAILED" : "PASSED");

    if (bad) wtof("TST75CAP FAILED (%d checks)", bad);
    else     wtof("TST75CAP PASSED ctl=%d pre=%d post=%d libc=%d",
                  r_ctl, r_pre, r_post, r_libc);

    return bad ? 8 : 0;
}

/*
 * One measurement.  Places a page boundary at 'bound' bytes into a BUFLEN
 * receive buffer, releases the page beyond it, and receives BUFLEN bytes the
 * way 'mode' says.
 *
 * Returns 0 if the pattern came through intact, 1 if it did not, -1 if the
 * measurement could not be taken at all.  *first_bad and *replay describe the
 * corruption when there is one.
 */
static int run_case(int lsock, unsigned port, int mode, unsigned bound,
                    const char *label, unsigned *first_bad, int *replay)
{
    static unsigned char    out[BUFLEN];
    unsigned char           *base;
    unsigned char           *page1;
    unsigned char           *buf;
    struct sockaddr_in      addr;
    int                     addrlen;
    int                     csock = -1;
    int                     ssock = -1;
    unsigned                i;
    int                     got;
    int                     clean;

    *first_bad = 0;
    *replay    = 0;

    base  = (unsigned char *)(((unsigned)arena + PAGE - 1)
                              & ~(unsigned)(PAGE - 1));
    page1 = base + PAGE;
    buf   = page1 - bound;          /* boundary lands at buf + bound */

    printf("  %s\n", label);

    for (i = 0; i < BUFLEN; i++) {
        out[i] = PATTERN(i);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)port);
    addr.sin_addr.s_addr = 0x7F000001;

    csock = socket(AF_INET, SOCK_STREAM, 0);
    if (csock < 0 || connect(csock, &addr, sizeof(addr)) < 0) {
        printf("    *** FAIL  connect() to the listener\n");
        if (csock >= 0) closesocket(csock);
        return -1;
    }

    addrlen = sizeof(addr);
    ssock   = accept(lsock, &addr, &addrlen);
    if (ssock < 0) {
        printf("    *** FAIL  accept()\n");
        closesocket(csock);
        return -1;
    }

    /* 256 bytes per send: the send path has the mirror defect and no cap, and
       at one segment per call it cannot reach it */
    if (send_all(csock, out, BUFLEN) < 0) {
        printf("    *** FAIL  send() of the pattern\n");
        closesocket(csock);
        closesocket(ssock);
        return -1;
    }

    /* wait until the whole message is queued, so a cap-4096 receive takes it
       all in one pair of instructions and the fault lands inside that copy */
    if (wait_for(ssock, BUFLEN) < 0) {
        printf("    *** FAIL  only part of the message arrived\n");
        closesocket(csock);
        closesocket(ssock);
        return -1;
    }

    /* the part of the buffer below the boundary must be resident, so that the
       leading segments certainly complete before the fault */
    for (i = 0; i < bound; i++) {
        buf[i] = 0;
    }

    /* release the page the buffer runs into, then receive at once - nothing
       may touch that page in between or it becomes resident again */
    release_page(page1);

    got = measure(mode, ssock, buf, BUFLEN);

    closesocket(csock);
    closesocket(ssock);

    if (got != BUFLEN) {
        printf("    *** FAIL  receive returned %d, expected %u\n",
               got, BUFLEN);
        return -1;
    }

    clean = 1;
    for (i = 0; i < BUFLEN; i++) {
        if (buf[i] != PATTERN(i)) {
            *first_bad = i;
            clean      = 0;
            break;
        }
    }

    if (clean) {
        printf("    clean   %u bytes, pattern intact\n", (unsigned)got);
        wtof("TST75CAP m=%d b=%u first_bad=none", mode, bound);
        return 0;
    }

    printf("    CORRUPT first bad byte at %u", *first_bad);
    if (*first_bad % SEG == 0) printf(" (a multiple of %u)", SEG);
    else                       printf(" (NOT a multiple of %u)", SEG);
    printf("\n");

    /* a replay from the start of the host buffer puts PATTERN(k) at
       first_bad + k, which is the signature that names the cause */
    *replay = 1;
    for (i = 0; *first_bad + i < BUFLEN; i++) {
        if (buf[*first_bad + i] != PATTERN(i)) {
            *replay = 0;
            break;
        }
    }

    printf("            the tail %s\n",
           *replay ? "is the host buffer replayed from its start"
                   : "is not a clean replay - some other corruption");

    /* SYSOUT sits in the QSAM buffer until fclose and an abend discards it;
       the WTO is in the job log the moment it is issued */
    wtof("TST75CAP m=%d b=%u first_bad=%u mult256=%c replay=%c",
         mode, bound, *first_bad,
         (*first_bad % SEG == 0) ? 'Y' : 'N', *replay ? 'Y' : 'N');

    return 1;
}

/*
 * Issue the measured receive the way 'mode' asks.
 */
static int measure(int mode, int s, void *buf, int len)
{
    switch (mode) {
    case M_CAP4096: return recv_capped(s, buf, len, 4096);
    case M_CAP256:  return recv_capped(s, buf, len,  256);
    default:        return recv(s, buf, len, 0);
    }
}

/*
 * The loop out of src/dyn75/@@75recv.c with the cap as a parameter, so that
 * the pre-fix and post-fix cases differ in exactly the line the fix changes.
 * Kept deliberately verbatim otherwise - including the -2 wait - so a
 * divergence here cannot be mistaken for the defect.
 */
static int recv_capped(int s, void *vbuf, int len, int cap)
{
    int     rc  = 0;
    int     chunk;
    char    *buf;
    int     read;
    PL75    pl;

    for (read = 0; read < len; read += rc) {
        buf = ((char *)vbuf) + read;

        chunk = len - read;
        if (chunk > cap) chunk = cap;

        __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl));
        pl.r6   = (unsigned) buf;
        pl.r7   = (unsigned) 11;    /* function code for recv() */
        pl.r8   = (unsigned) s;
        pl.r9   = (unsigned) chunk;

        __75(&pl);

        rc = (int) pl.r4;
        if (rc == -2) {
            snooze();
            rc = 0;
            continue;
        }

        if (rc <= 0) break;
    }

    if (read) rc = read;

    return rc;
}

/*
 * Send the whole buffer one segment at a time.  Retries the X'75' wait code
 * -2, which SEND can return on a blocking socket since hyperion 1a599b0d.
 */
static int send_all(int s, const unsigned char *buf, unsigned len)
{
    unsigned    done = 0;
    unsigned    chunk;
    int         rc = -1;
    int         tries;

    while (done < len) {
        chunk = len - done;
        if (chunk > SEG) chunk = SEG;

        for (tries = 0; tries < WAITMAX; tries++) {
            rc = send(s, buf + done, (int)chunk, 0);
            if (rc != -2) break;
            snooze();
        }

        if (rc <= 0) return -1;
        done += (unsigned)rc;
    }

    return 0;
}

/*
 * Wait until 'want' bytes are queued on the socket.  FIONREAD is a different
 * X'75' function code and does not go through the copy path being measured.
 */
static int wait_for(int s, unsigned want)
{
    int n;
    int tries;

    for (tries = 0; tries < WAITMAX; tries++) {
        n = 0;
        if (ioctlsocket(s, FIONREAD, &n) < 0) return -1;
        if (n >= 0 && (unsigned)n >= want) return 0;
        snooze();
    }

    return -1;
}

/*
 * PGRLSE (SVC 112) releases whole pages: R0 = low address, R1 = high address,
 * return code in R15.  It rounds INWARD, so whether the high address is the
 * last byte of the page or the first byte beyond it decides between releasing
 * one page and releasing nothing at all.  main() probes which, rather than
 * assuming; a silent no-op here would make every case pass for the wrong
 * reason.
 */
static int pgrlse(void *lo, void *hi)
{
    int rc = -1;

    __asm__("\n"
"*\n"
"* release pages via SVC 112 (PGRLSE): R0 = low, R1 = high, R15 = rc\n"
"*\n"
"         LR\t0,%1\n\t"
"         LR\t1,%2\n\t"
"         SVC\t112\n\t"
"         ST\t15,%0"
        : "=m" (rc) : "r" (lo), "r" (hi) : "0", "1", "14", "15", "memory");

    return rc;
}

/* Release exactly the one page starting at 'page', using whichever high
   address main() found this system to want. */
static int release_page(unsigned char *page)
{
    return pgrlse(page, page + hi_bias);
}

static void snooze(void)
{
    __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds" : : : "0", "1");
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
