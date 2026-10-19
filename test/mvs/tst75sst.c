/*
 * tst75sst.c - libc370 #160 probe (MVS target, batch): after a page fault on
 * the guest buffer of a SEND, the X'75' copy must resume in the HOST buffer
 * where it stopped, not at its start.  The SEND counterpart of tst75rst.c.
 *
 * ISSUE #160: X'75' moves data in 256-byte segments and the instruction is
 * restartable.  On SEND the guest buffer is the source.  A page translation
 * exception on it is nullifying, the instruction runs again from the top,
 * and the guest side resumes where it stopped (R1 and the base register were
 * advanced).  An unfixed emulator recomputes the host pointer from map32[R2]
 * on every entry, so the remaining bytes land at the START of the host
 * buffer: the bytes that already went across are overwritten by the tail,
 * and the end of the host buffer is never written at all.  The peer receives
 * the right length with the wrong content - from byte 0 on.
 *
 * A copy of 256 bytes or less is immune by construction: a single segment
 * either faults having moved nothing, where resuming from the start is
 * correct, or it completes.
 *
 * WHAT THIS PROBE DOES
 * --------------------
 * Every round fills a send buffer with a pattern, releases with PGRLSE
 * (SVC 112) the one page that starts 'bound' bytes into it, and sends the
 * whole buffer through send() - the library function, so the red step
 * measures the library as released and the green step the library under
 * test - over a loopback pair the probe owns both ends of.  The pages below
 * the boundary were just written and are resident, so the segments below it
 * complete and the segment at the boundary faults.  The receiving end takes
 * the bytes with recv(), which caps at 256 since #154 and so cannot add a
 * fault of its own, and compares them with the send buffer as it reads AFTER
 * the send (a released page reads back as zeros once it is touched again).
 *
 * The caller's loop is the usual one: send() again from where the last call
 * stopped until everything is out, any return <= 0 being fatal - exactly what
 * a caller has to do with a send() that may return a short count.
 *
 * Boundary 0 is the control: the first segment faults having moved nothing,
 * which is correct on a fixed AND on an unfixed emulator.  It must pass
 * either way.  Bound 600 is not a multiple of 256: the segment that straddles
 * the page boundary faults before it copies a byte (both pages are resolved
 * before the move), so the replay starts at 512 there.
 *
 * - The byte pattern is i % 251, a period that does not divide 256, so a
 *   replay shifted by a multiple of 256 cannot carry the right values.
 * - PGRLSE is verified to work on this system before any socket is opened,
 *   exactly as in tst75rst.c; without that a PGRLSE that quietly did nothing
 *   would look like a fixed emulator.
 *
 * WHAT A PASS DOES AND DOES NOT PROVE
 * -----------------------------------
 * A pass has two causes: the emulator resumes correctly, or the copy never
 * faulted after a completed segment.  The guest cannot tell them apart.  Only
 * the emulator-side trace separates them: mvslovers/hyperion branch
 * diag/x75-restart-trace logs every restart, and a SEND restart reads
 *
 *   X75 restart 7 (3 after a completed segment): dir=0 left=488 done=512 ...
 *
 * Read this job's RC together with that log, never alone.
 *
 * BUILD (from the repository root, after make; S a scratch directory, R a
 * directory holding a libc.a built from the release being compared against):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tst75sst.c \
 *         -o $S/TST75SSG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -Iinclude -L $R test/mvs/tst75sst.c \
 *         -o $S/TST75SSR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TST75SSG=TST75SSG.iebcopy \
 *         TST75SSR=TST75SSR.iebcopy -o sst -xmit --dsn IBMUSER.LIBC370.SSTSCR
 *
 * then jcl/recv75sst.jcl and jcl/tst75sst.jcl.
 *
 * RC: 0 = every round arrived intact, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mvs/wto.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>

#define PAGE        4096            /* MVS page size                        */
#define SEG          256            /* the X'75' copy segment               */
#define MAXLEN     32000            /* largest single send() measured       */
#define ROUNDS         5            /* forced faults per case               */
#define PORTBASE   27600            /* first port tried for the loopback    */
#define PORTTRIES     10
#define WAITMAX      250            /* x 0.08s = 20s for the data to queue  */

#define PATTERN(i)  ((unsigned char)((i) % 251))

struct sstcase {
    unsigned    len;                /* bytes handed to one send()           */
    unsigned    bound;              /* offset of the released page          */
};

static const struct sstcase cases[] = {
    {  1024,     0 },               /* control: faults on segment 0         */
    {   300,   256 },               /* one completed segment, then a fault  */
    {  1000,   512 },
    {  1000,   600 },               /* boundary inside a segment            */
    {  4096,  2048 },
    { 32000, 16384 },
};
#define NCASE   (sizeof(cases) / sizeof(cases[0]))

static unsigned char *arena;        /* send buffers, page boundaries inside */
static unsigned char *rbuf;         /* what the receiving end got           */
static unsigned       hi_bias;      /* PGRLSE high address, see main()      */

static int  check(const char *what, int ok);
static int  pgrlse(void *lo, void *hi);
static void snooze(void);
static int  send_all(int s, const unsigned char *buf, unsigned len,
                     int *first, unsigned *calls);
static int  recv_all(int s, unsigned char *buf, unsigned len);
static int  connect_pair(int lsock, unsigned port, int *cs, int *ss);
static int  run_round(int lsock, unsigned port, const struct sstcase *c,
                      unsigned round);

int main(void)
{
    unsigned char       *page1;
    struct sockaddr_in  addr;
    int                 lsock;
    unsigned            port = 0;
    unsigned            i, r;
    int                 bad  = 0;
    int                 failed_rounds = 0;
    int                 ok_in, ok_ex;

    printf("TST75SST - libc370 #160 probe\n\n");

    arena = (unsigned char *)malloc(MAXLEN + 3 * PAGE);
    rbuf  = (unsigned char *)malloc(MAXLEN);
    if (!arena || !rbuf) {
        printf("*** FAIL  no storage\n");
        wtof("TST75SST FAILED - no storage");
        return 8;
    }

    /* Does PGRLSE release on this system, and which high address does it
       want?  Fill, release, read back zeros - see tst75rst.c. */
    page1 = (unsigned char *)(((unsigned)arena + PAGE - 1)
                              & ~(unsigned)(PAGE - 1)) + PAGE;
    memset(page1, 0xFF, PAGE);
    pgrlse(page1, page1 + PAGE - 1);
    ok_in = (page1[0] == 0x00 && page1[SEG] == 0x00);
    ok_ex = 0;
    if (!ok_in) {
        memset(page1, 0xFF, PAGE);
        pgrlse(page1, page1 + PAGE);
        ok_ex = (page1[0] == 0x00 && page1[SEG] == 0x00);
    }
    hi_bias = ok_in ? (PAGE - 1) : (ok_ex ? PAGE : 0);
    wtof("TST75SST pgrlse incl=%c excl=%c bias=%u",
         ok_in ? 'Y' : 'N', ok_ex ? 'Y' : 'N', hi_bias);

    if (!check("PGRLSE releases a page (reads back zero)", hi_bias != 0)) {
        /* nothing below could fault: no verdict either way */
        wtof("TST75SST FAILED - pgrlse no-op, cases not run");
        return 8;
    }

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) {
        wtof("TST75SST FAILED - no listener");
        return 8;
    }

    for (i = 0; i < PORTTRIES && port == 0; i++) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)(PORTBASE + i));
        addr.sin_addr.s_addr = 0x7F000001;      /* 127.0.0.1 */
        if (bind(lsock, &addr, sizeof(addr)) >= 0) port = PORTBASE + i;
    }

    if (port == 0 || listen(lsock, 5) < 0) {
        closesocket(lsock);
        wtof("TST75SST FAILED - no port");
        return 8;
    }

    printf("  listening on 127.0.0.1:%u\n\n", port);

    for (i = 0; i < NCASE; i++) {
        printf("  len %5u, released page at %5u\n",
               cases[i].len, cases[i].bound);
        for (r = 0; r < ROUNDS; r++) {
            if (run_round(lsock, port, &cases[i], r) != 0) failed_rounds++;
        }
    }

    closesocket(lsock);

    bad += failed_rounds;

    printf("\n  A pass here means the copy resumed correctly OR never faulted.\n");
    printf("  Only the emulator trace tells those apart.\n");
    printf("\nTST75SST %s\n", bad ? "FAILED" : "PASSED");

    if (bad) wtof("TST75SST FAILED (%d of %u rounds)", failed_rounds,
                  (unsigned)(NCASE * ROUNDS));
    else     wtof("TST75SST PASSED (%u rounds)", (unsigned)(NCASE * ROUNDS));

    return bad ? 8 : 0;
}

/*
 * One measurement: fill, release the page at c->bound, send, receive,
 * compare.  Returns 0 when the bytes arrived intact.
 */
static int run_round(int lsock, unsigned port, const struct sstcase *c,
                     unsigned round)
{
    unsigned char   *base;
    unsigned char   *buf;
    unsigned char   *rel;
    unsigned        i;
    unsigned        done;
    unsigned        calls = 0;
    unsigned        first_bad;
    int             first = 0;
    int             cs, ss;
    int             replay;

    /* the released page starts exactly at buf + bound */
    base = (unsigned char *)(((unsigned)arena + PAGE - 1)
                             & ~(unsigned)(PAGE - 1));
    rel  = base + (c->bound / PAGE + 1) * PAGE;
    buf  = rel - c->bound;

    if (connect_pair(lsock, port, &cs, &ss) < 0) {
        printf("    *** FAIL  round %u: no connected pair\n", round);
        wtof("TST75SST len=%u b=%u r=%u no pair", c->len, c->bound, round);
        return 1;
    }

    for (i = 0; i < c->len; i++) buf[i] = PATTERN(i);

    /* nothing may touch the released page before send() reads it */
    pgrlse(rel, rel + hi_bias);

    if (send_all(cs, buf, c->len, &first, &calls) < 0
        || recv_all(ss, rbuf, c->len) < 0) {
        printf("    *** FAIL  round %u: transfer incomplete (errno %d)\n",
               round, errno);
        wtof("TST75SST len=%u b=%u r=%u transfer incomplete first=%d",
             c->len, c->bound, round, first);
        closesocket(cs);
        closesocket(ss);
        return 1;
    }

    closesocket(cs);
    closesocket(ss);

    /* the send buffer as it reads now is what had to arrive */
    for (first_bad = 0; first_bad < c->len; first_bad++) {
        if (rbuf[first_bad] != buf[first_bad]) break;
    }

    if (first_bad == c->len) {
        printf("    ok    round %u: %u bytes intact, first send() %d,"
               " %u calls\n", round, c->len, first, calls);
        if (round == 0) {
            wtof("TST75SST len=%u b=%u intact first=%d calls=%u",
                 c->len, c->bound, first, calls);
        }
        return 0;
    }

    /* the red signature: the host buffer starts with the guest buffer from
       the first segment that had not completed when the copy faulted */
    done   = (c->bound / SEG) * SEG;
    replay = 1;
    for (i = 0; done + i < c->len; i++) {
        if (rbuf[i] != buf[done + i]) {
            replay = 0;
            break;
        }
    }

    printf("    *** FAIL  round %u: first bad byte at %u, first send() %d,"
           " %u calls; tail of the buffer from %u moved to 0: %s\n",
           round, first_bad, first, calls, done, replay ? "yes" : "no");
    wtof("TST75SST len=%u b=%u r=%u first_bad=%u replay_from=%u:%c",
         c->len, c->bound, round, first_bad, done, replay ? 'Y' : 'N');
    return 1;
}

/*
 * The usual caller loop: send the rest again until everything is out, any
 * return <= 0 is fatal.  Records the first call's return and the call count.
 */
static int send_all(int s, const unsigned char *buf, unsigned len,
                    int *first, unsigned *calls)
{
    unsigned    done = 0;
    int         rc;

    while (done < len) {
        rc = send(s, buf + done, (int)(len - done), 0);
        if (*calls == 0) *first = rc;
        (*calls)++;
        if (rc <= 0) return -1;
        done += (unsigned)rc;
    }

    return 0;
}

/* Wait until 'len' bytes are queued, then take them with recv(). */
static int recv_all(int s, unsigned char *buf, unsigned len)
{
    int n;
    int tries;

    for (tries = 0; tries < WAITMAX; tries++) {
        n = 0;
        if (ioctlsocket(s, FIONREAD, &n) < 0) return -1;
        if (n >= 0 && (unsigned)n >= len) break;
        snooze();
    }

    if (tries == WAITMAX) return -1;

    return recv(s, buf, (int)len, 0) == (int)len ? 0 : -1;
}

static int connect_pair(int lsock, unsigned port, int *cs, int *ss)
{
    struct sockaddr_in  addr;
    int                 addrlen;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)port);
    addr.sin_addr.s_addr = 0x7F000001;

    *cs = socket(AF_INET, SOCK_STREAM, 0);
    if (*cs < 0) return -1;
    if (connect(*cs, &addr, sizeof(addr)) < 0) {
        closesocket(*cs);
        return -1;
    }

    addrlen = sizeof(addr);
    *ss = accept(lsock, &addr, &addrlen);
    if (*ss < 0) {
        closesocket(*cs);
        return -1;
    }

    return 0;
}

/*
 * PGRLSE (SVC 112): R0 = low address, R1 = high address, R15 = rc.  It
 * rounds inward; main() finds which high address this system wants.
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

static void snooze(void)
{
    __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds"
            : : : "0", "1", "14", "15", "memory");
}

static int check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok;
}
