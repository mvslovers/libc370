/*
 * tst75rst.c - libc370 #154 probe (MVS target, batch): after a page fault on
 * the guest buffer, the X'75' copy must resume in the HOST buffer where it
 * stopped, not at its start.
 *
 * ISSUE #154: X'75' moves data in 256-byte segments and the instruction is
 * restartable by design.  A page translation exception on the guest buffer is
 * nullifying, so the instruction runs again from the top: R0 says the native
 * call was already made, R1 says how much is left, and the base register was
 * advanced before the exception - so the GUEST side of the copy resumes
 * exactly where it stopped.  The host side has no such register.  Upstream
 * x75.c recomputes it from scratch on every entry
 *
 *     if (regs->GR_L(1) != 0) s = (unsigned char *)(map32[regs->GR_L(2)]);
 *
 * and R2 is a slot index into map32[] that never advances.  The remaining
 * bytes are therefore copied from the START of the host buffer to the already
 * advanced guest address: a 1024-byte message whose copy faults at offset 512
 * lands as M[0..511] followed by M[0..511] again.
 *
 * 256 or less is immune by construction - a single segment either faults
 * having copied nothing, where resuming from the start is correct, or it
 * completes.  The defect needs at least one COMPLETED segment, which is why
 * every cap above 256 held until it did not: 4096 in @@75recv.c since
 * cd43a70, then 2048 and finally one byte per recv() in mvsMF.
 *
 * WHAT THIS PROBE DOES DIFFERENTLY
 * --------------------------------
 * It does not wait for a page fault, it causes one.  A 1024-byte receive
 * buffer is placed so that a page boundary falls at a chosen multiple of 256
 * inside it, and the page beyond that boundary is released with PGRLSE
 * (SVC 112) immediately before the receive.  The segments below the boundary
 * complete, the segment at the boundary faults, and an unfixed emulator copies
 * the remainder from host[0].
 *
 * Four cases run.  Boundary 0 is the control: the very first segment faults
 * having copied nothing, which is correct behaviour on a fixed AND on an
 * unfixed emulator.  It must pass either way - if it fails, something other
 * than this defect is wrong and the other three cases mean nothing.
 *
 * DETAILS THAT DECIDE WHETHER THE PROBE MEASURES ANYTHING
 * -------------------------------------------------------
 * - The receive goes through __75() directly rather than recv(), so one
 *   measurement is one pair of X'75' instructions.  recv() caps at 4096 and
 *   loops, which would turn one measurement into several pairs and blur which
 *   one faulted.
 *
 * - The byte pattern is i % 251.  Any period dividing 256 would make a replay
 *   that restarts at a multiple of 256 invisible - the wrong bytes would carry
 *   the right values.  251 is the largest prime below 256.
 *
 * - The peer sends in 256-byte chunks.  send() has the mirror defect and no
 *   cap at all, so sending in one 1024-byte call could corrupt the data on its
 *   way out and be misread here as a receive-side failure.  At 256 the send
 *   side is immune by the same construction that makes the control case safe.
 *
 * - The page below the boundary is written to just before the receive, so it
 *   is certainly resident and the leading segments certainly complete.
 *
 * - PGRLSE is verified to work on this system before any socket is opened:
 *   a page is filled with 0xFF, released, and read back.  A released page
 *   reads as zeros.  Without that check a PGRLSE that quietly did nothing
 *   would look exactly like a fixed emulator.
 *
 * WHAT A PASS DOES AND DOES NOT PROVE
 * -----------------------------------
 * A pass has two possible causes: the emulator resumes correctly, or the
 * receive never faulted.  The guest cannot tell them apart - R0 comes back as
 * 1 either way, and there is no status bit that distinguishes a patched
 * emulator from an unpatched one.  Only the emulator-side trace separates
 * them: mvslovers/hyperion branch diag/x75-restart-trace logs every restart
 * with the bytes already moved,
 *
 *   X75 restart 7 (3 after a completed segment): dir=1 left=512 done=512 talk=4
 *
 * and the number that matters is the second one.  A run whose restarts all
 * report done=0 faulted on segment 0 every time and proves nothing.  Read this
 * job's RC together with that log, never alone.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tst75rst.c -flinker-output=iebcopy -o TST75RST
 *     ld370 --pack TST75RST.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tst75rst.jcl.  Built by hand - libc370 is the cc370 sysroot,
 * not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <socket.h>
#include <__75.h>

#define PAGE        4096            /* MVS page size                        */
#define SEG          256            /* the X'75' copy segment               */
#define BUFLEN      1024            /* bytes per measured receive           */
#define NCASE          4
#define PORTBASE   27500            /* first port tried for the loopback    */
#define PORTTRIES     10
#define WAITMAX      250            /* x 0.08s = 20s for the data to queue  */

#define PATTERN(i)  ((unsigned char)((i) % 251))

/* boundary offset inside the receive buffer; 0 is the control case */
static const unsigned boundary[NCASE] = { 0, 256, 512, 768 };

static unsigned char arena[4 * PAGE];

static int  check(const char *what, int ok);
static void pgrlse(void *lo, void *hi);
static void snooze(void);
static int  recv75(int s, void *buf, unsigned len);
static int  send_all(int s, const unsigned char *buf, unsigned len);
static int  wait_for(int s, unsigned want);
static int  run_case(int lsock, unsigned port, unsigned bound);

int main(void)
{
    unsigned char   *base;
    unsigned char   *page1;
    struct sockaddr_in  addr;
    int             lsock = -1;
    unsigned        port  = 0;
    unsigned        i;
    int             bad   = 0;
    int             rc;

    /* primes the stdio buffers before anything is measured */
    printf("TST75RST - libc370 #154 probe\n\n");

    base  = (unsigned char *)(((unsigned)arena + PAGE - 1)
                              & ~(unsigned)(PAGE - 1));
    page1 = base + PAGE;

    /* Does PGRLSE release on this system?  Fill, release, read back.  A
       released page reads as zeros; if it still reads 0xFF nothing was
       released and every case below would pass for the wrong reason. */
    memset(page1, 0xFF, PAGE);
    pgrlse(page1, page1 + PAGE - 1);
    bad += check("PGRLSE releases the page (reads back zero)",
                 page1[0] == 0x00 && page1[SEG] == 0x00);

    /* one listener for the whole run; a fresh connected pair per case */
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    bad  += check("socket() for the listener", lsock >= 0);

    if (lsock < 0) {
        printf("\nTST75RST FAILED - no listener\n");
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
        printf("\nTST75RST FAILED - no port\n");
        return 8;
    }

    printf("  listening on 127.0.0.1:%u\n\n", port);

    for (i = 0; i < NCASE; i++) {
        rc = run_case(lsock, port, boundary[i]);
        if (rc < 0) {
            bad++;
        } else {
            bad += rc;
        }
    }

    closesocket(lsock);

    printf("\n  A pass here means the copy resumed correctly OR never faulted.\n");
    printf("  Only the emulator trace tells those apart - read the count of\n");
    printf("  restarts 'after a completed segment' alongside this RC.\n");

    printf("\nTST75RST %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

/*
 * One measurement.  Places a page boundary at 'bound' bytes into a BUFLEN
 * receive buffer, releases the page beyond it, and receives BUFLEN bytes in a
 * single pair of X'75' instructions.
 */
static int run_case(int lsock, unsigned port, unsigned bound)
{
    static unsigned char    out[BUFLEN];
    unsigned char           *base;
    unsigned char           *page1;
    unsigned char           *buf;
    struct sockaddr_in      addr;
    int                     addrlen;
    int                     csock = -1;
    int                     ssock = -1;
    unsigned                first_bad = 0;
    unsigned                i;
    int                     got;
    int                     bad = 0;
    int                     clean;

    base  = (unsigned char *)(((unsigned)arena + PAGE - 1)
                              & ~(unsigned)(PAGE - 1));
    page1 = base + PAGE;
    buf   = page1 - bound;          /* boundary lands at buf + bound */

    printf("  boundary at %4u (%u whole segments below it)\n",
           bound, bound / SEG);

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

    /* wait until the whole message is queued, so one receive takes it all and
       the measurement is exactly one pair of instructions */
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
    pgrlse(page1, page1 + PAGE - 1);

    got = recv75(ssock, buf, BUFLEN);

    closesocket(csock);
    closesocket(ssock);

    if (got != BUFLEN) {
        printf("    *** FAIL  receive returned %d, expected %u\n", got, BUFLEN);
        return 1;
    }

    clean = 1;
    for (i = 0; i < BUFLEN; i++) {
        if (buf[i] != PATTERN(i)) {
            first_bad = i;
            clean     = 0;
            break;
        }
    }

    if (clean) {
        printf("    ok    %u bytes, pattern intact\n", (unsigned)got);
        return 0;
    }

    bad = 1;
    printf("    *** FAIL  first bad byte at %u", first_bad);

    if (first_bad % SEG == 0) {
        printf(" (a multiple of %u)", SEG);
    } else {
        printf(" (NOT a multiple of %u - not this defect)", SEG);
    }
    printf("\n");

    /* a replay from the start of the host buffer puts PATTERN(k) at
       first_bad + k, which is the signature that names the cause */
    clean = 1;
    for (i = 0; first_bad + i < BUFLEN; i++) {
        if (buf[first_bad + i] != PATTERN(i)) {
            clean = 0;
            break;
        }
    }

    if (clean) {
        printf("              the tail is the host buffer replayed from its"
               " start\n");
    } else {
        printf("              the tail is not a clean replay - some other"
               " corruption\n");
    }

    return bad;
}

/*
 * The measured receive: function code 11, straight through __75(), so the
 * whole transfer is one pair of X'75' instructions with no retry loop.
 */
static int recv75(int s, void *buf, unsigned len)
{
    PL75    pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl));

    pl.r6   = (unsigned) buf;       /* destination buffer       */
    pl.r7   = (unsigned) 11;        /* function code for recv() */
    pl.r8   = (unsigned) s;         /* socket number            */
    pl.r9   = (unsigned) len;       /* bytes wanted             */

    __75(&pl);

    return (int) pl.r4;
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
 * PGRLSE (SVC 112) releases whole pages: R0 = low address, R1 = high address.
 * It rounds INWARD, so a partial page at either end is left alone - both
 * addresses here are inside one page by construction.
 */
static void pgrlse(void *lo, void *hi)
{
    __asm__("\n"
"*\n"
"* release pages via SVC 112 (PGRLSE): R0 = low, R1 = high\n"
"*\n"
"         LR\t0,%0\n\t"
"         LR\t1,%1\n\t"
"         SVC\t112\n"
        : : "r" (lo), "r" (hi) : "0", "1", "14", "15", "memory");
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
