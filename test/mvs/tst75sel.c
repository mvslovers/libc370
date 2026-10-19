/*
 * tst75sel.c - libc370 #144 probe (MVS target, batch): does select() over
 * more than one socket report every socket that is ready?
 *
 * Not a libc370 defect.  It measures the Hercules X'75' emulation (tcpip.c,
 * SELECT subcode 4), which runs the host select() as
 *
 *     select (Ccom_han [aux2 - 1] + 1, ro, wo, eo, &timeout);
 *
 * aux2 is the guest's maxsock, so nfds is the HOST descriptor of the guest's
 * highest socket NUMBER, plus one.  Socket numbers come from the emulator's
 * slot table, host descriptors from the kernel; a socket whose descriptor is
 * at or above that nfds is not examined.  Linux writes back only the words
 * of each set below nfds, so such a socket comes back either cleared (same
 * word: a silent MISS) or with its input bit untouched (later word: reported
 * ready whether it is or not, a FALSE READY).  Both are checked.
 *
 * HOW THE CONDITION IS CREATED, FROM THE GUEST ALONE
 * --------------------------------------------------
 * Every X'75' socket(), accept() and close() takes or frees a slot and a
 * descriptor as a pair, both lowest-free, so guest-only churn keeps slot
 * order and descriptor order in step - that is phase A, the control, and it
 * must show nothing.  The two orders come apart only when the Hercules
 * process takes a descriptor WITHOUT a slot.  The guest can make it do that:
 * connect to a port Hercules itself serves (its HTTP server, 8888, or its
 * TN3270 console, 3270) and Hercules accept()s that connection on a
 * descriptor of its own.  Phase B:
 *
 *     P = socket()            slot p,   fd x
 *     H = socket()            slot p+1, fd x+1
 *     close(P)                slot p and fd x are free
 *     connect(H, Hercules)    Hercules accepts on fd x  (no slot)
 *     C = socket()            slot p,   fd x+2   <- low slot, high fd
 *     C connected to our own listener, its peer sends one byte;
 *     H gets Hercules' answer (HTTP reply, or telnet negotiation).
 *
 * Both C and H are readable.  select() over {C, H} gets maxsock = p+2, the
 * emulator computes nfds = fd(H)+1 = x+2, and C (fd x+2) is not examined.
 * The guest sees the slot numbers, never the descriptors, so it checks what
 * it can: that C really got the lower slot.  Controls in the same attempt:
 * select over {C} alone and over {H} alone, each its own maxsock, must
 * report the socket.
 *
 * Phase B also counts on nothing else in the guest (mvsMF answering the job
 * poll, say) taking or freeing a slot between "close(P)" and "C = socket()".
 * That is why it runs several attempts; an attempt whose slot layout came out
 * wrong is reported as such and not counted.
 *
 * READINESS IS CROSS-CHECKED WITH FIONREAD, a different X'75' function code
 * (15) that does not go through SELECT.  Every socket is confirmed readable
 * BEFORE select() runs, so a miss cannot be data that had not arrived yet.
 * The returned fd_set is compared whatever select() returned (0 included).
 *
 * BUILD (host, from the repository root, after make):
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tst75sel.c \
 *           -o TST75SEL -flinker-output=iebcopy
 *     ld370 --pack TST75SEL=TST75SEL.iebcopy -o out -xmit \
 *           --dsn IBMUSER.LIBC370.SLSCR
 *
 * RUN: jcl/recv75sel.jcl (RECEIVE from IBMUSER.LIBC370.SLXMIT), then
 * jcl/tst75sel.jcl.  Every result line is a WTO, so it is in the job log:
 *     TST75SEL A r=.. n=.. miss=.. false=..        phase A, per round
 *     TST75SEL B#. C=.. H=.. port=.. pair: rc=.. C:.. H:..   phase B
 *     TST75SEL RESULT ...                          the verdict
 * where C:fion/isset is FIONREAD before select and the bit after it.
 *
 * RC: 0 = no anomaly, and phase B's condition was created at least once;
 *     4 = an anomaly (miss or false ready) - the emulator defect reproduced;
 *     8 = setup failure, or phase B's condition never created: no verdict.
 */
#include <stdio.h>
#include <string.h>
#include <mvs/wto.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>

#define PORTBASE   27600            /* first port tried for the listener   */
#define PORTTRIES     10
#define NPAIR          4            /* phase A: connected pairs            */
#define AROUNDS        8            /* phase A: rounds                     */
#define BTRIES         4            /* phase B: attempts                   */
#define WAITMAX      125            /* x 0.08 s = 10 s for data to arrive  */

static int  lsock = -1;
static unsigned lport;
static int  anomalies;              /* misses + false readies, all phases  */

static const unsigned herc_port[2] = { 8888, 3270 };

/* "GET / HTTP/1.0" CR LF CR LF, in ASCII: the bytes go to Hercules as is */
static const unsigned char http_get[] = {
    0x47, 0x45, 0x54, 0x20, 0x2F, 0x20, 0x48, 0x54, 0x54, 0x50,
    0x2F, 0x31, 0x2E, 0x30, 0x0D, 0x0A, 0x0D, 0x0A
};

static void snooze(unsigned hundredths)
{
    unsigned t = hundredths;

    __asm__("STIMER WAIT,BINTVL=(%0)" : : "r" (&t) : "0", "1", "14", "15");
}

static int fionread(int s)
{
    int n = 0;

    if (ioctlsocket(s, FIONREAD, &n) < 0) return -1;
    return n;
}

/* wait until at least 'want' bytes are queued on s */
static int wait_for(int s, int want)
{
    int tries;

    for (tries = 0; tries < WAITMAX; tries++) {
        if (fionread(s) >= want) return 0;
        snooze(8);
    }
    return -1;
}

static int sock_to(unsigned port)
{
    struct sockaddr_in addr;
    int s;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)port);
    addr.sin_addr.s_addr = 0x7F000001;
    if (connect(s, &addr, sizeof(addr)) < 0) {
        closesocket(s);
        return -1;
    }
    return s;
}

/* a connected pair over our own listener: *c the client, *s the server end */
static int make_pair(int *c, int *s)
{
    struct sockaddr_in addr;
    int len = sizeof(addr);

    *s = -1;
    *c = sock_to(lport);
    if (*c < 0) return -1;
    *s = accept(lsock, &addr, &len);
    if (*s < 0) {
        closesocket(*c);
        *c = -1;
        return -1;
    }
    return 0;
}

/*
 * select() for read over n sockets, then compare each one's bit with the
 * FIONREAD taken before.  fion[] is filled here; returns misses + false
 * readies and the select() rc through *rc.
 */
static int sel_check(const int *s, int n, int *fion, int *isset, int *rc)
{
    fd_set   r;
    timeval  t;
    int      maxsock = 0;
    int      i;
    int      bad = 0;

    for (i = 0; i < n; i++) {
        fion[i] = fionread(s[i]);
        if (s[i] + 1 > maxsock) maxsock = s[i] + 1;
    }

    FD_ZERO(&r);
    for (i = 0; i < n; i++) FD_SET(s[i], &r);

    t.tv_sec  = 0;
    t.tv_usec = 200000;
    *rc = select(maxsock, &r, NULL, NULL, &t);

    for (i = 0; i < n; i++) {
        isset[i] = FD_ISSET(s[i], &r) ? 1 : 0;
        if (fion[i] > 0 && !isset[i]) bad++;
        if (fion[i] <= 0 && isset[i]) bad++;
    }
    return bad;
}

/*
 * Phase A, the control: NPAIR pairs over our own listener.  Each round
 * closes and reopens a varying subset of the clients, gives a byte to all
 * of them (even rounds) or every other one (odd rounds), and selects over
 * all clients.  Slot and descriptor move as pairs here, so nothing may show.
 */
static int phase_a(void)
{
    int  c[NPAIR], sv[NPAIR], fion[NPAIR], isset[NPAIR];
    int  r = -1, i, rc, bad, miss, fls;
    int  setup = 0;
    char one = 'x';

    for (i = 0; i < NPAIR; i++) c[i] = sv[i] = -1;
    for (i = 0; i < NPAIR; i++) {
        if (make_pair(&c[i], &sv[i]) < 0) { setup = 1; goto out; }
    }

    for (r = 0; r < AROUNDS; r++) {
        for (i = 0; i < NPAIR; i++) {
            if (((r + 1) >> (i % 3)) & 1) {
                closesocket(c[i]);
                closesocket(sv[i]);
                if (make_pair(&c[i], &sv[i]) < 0) { setup = 1; goto out; }
            }
        }
        for (i = 0; i < NPAIR; i++) {
            if ((r & 1) && (i & 1)) continue;
            if (send(sv[i], &one, 1, 0) != 1) { setup = 1; goto out; }
        }
        for (i = 0; i < NPAIR; i++) {
            if ((r & 1) && (i & 1)) continue;
            if (wait_for(c[i], 1) < 0) { setup = 1; goto out; }
        }

        bad  = sel_check(c, NPAIR, fion, isset, &rc);
        miss = fls = 0;
        for (i = 0; i < NPAIR; i++) {
            if (fion[i] > 0 && !isset[i]) miss++;
            if (fion[i] <= 0 && isset[i]) fls++;
        }
        anomalies += bad;

        wtof("TST75SEL A r=%d s=%d,%d,%d,%d rc=%d miss=%d false=%d",
             r, c[0], c[1], c[2], c[3], rc, miss, fls);
        if (bad) {
            for (i = 0; i < NPAIR; i++)
                wtof("TST75SEL A   s=%d fion=%d isset=%d",
                     c[i], fion[i], isset[i]);
        }

        /* drain, so every round starts empty */
        for (i = 0; i < NPAIR; i++) {
            if (fion[i] > 0) recv(c[i], &one, 1, 0);
        }
    }

out:
    for (i = 0; i < NPAIR; i++) {
        if (c[i]  >= 0) closesocket(c[i]);
        if (sv[i] >= 0) closesocket(sv[i]);
    }
    if (setup) wtof("TST75SEL A setup failed in round %d", r);
    return setup;
}

/*
 * One phase B attempt.  Returns 1 if the slot layout came out as intended
 * (C below H), 0 if not, -1 on a setup failure.
 */
static int phase_b(int k)
{
    int      p = -1, h = -1, c = -1, sv = -1;
    int      s[2], fion[2], isset[2];
    int      fion1[1], isset1[1];
    int      rc, rc1, rc2, bad, b1, b2;
    int      hix, made = -1;
    unsigned port = 0;
    char     one = 'x';

    p = socket(AF_INET, SOCK_STREAM, 0);
    h = socket(AF_INET, SOCK_STREAM, 0);
    if (p < 0 || h < 0) goto out;

    closesocket(p);                 /* slot p and its descriptor free */

    {
        struct sockaddr_in addr;
        for (hix = 0; hix < 2 && !port; hix++) {
            memset(&addr, 0, sizeof(addr));
            addr.sin_family      = AF_INET;
            addr.sin_port        = htons((unsigned short)herc_port[hix]);
            addr.sin_addr.s_addr = 0x7F000001;
            if (connect(h, &addr, sizeof(addr)) >= 0) port = herc_port[hix];
        }
    }
    if (!port) {
        wtof("TST75SEL B#%d no Hercules port reachable (8888, 3270)", k);
        goto out;
    }

    snooze(50);                     /* let Hercules accept() on fd x  */

    if (make_pair(&c, &sv) < 0) goto out;

    if (port == 8888 && send(h, (void *)http_get, sizeof(http_get), 0)
        != (int)sizeof(http_get)) goto out;
    if (send(sv, &one, 1, 0) != 1) goto out;
    if (wait_for(c, 1) < 0 || wait_for(h, 1) < 0) {
        wtof("TST75SEL B#%d data did not arrive C=%d H=%d", k,
             fionread(c), fionread(h));
        goto out;
    }

    made = (c == p && c < h) ? 1 : 0;

    s[0] = c;
    s[1] = h;
    bad  = sel_check(s, 2, fion, isset, &rc);
    b1   = sel_check(&c, 1, fion1, isset1, &rc1);
    b2   = sel_check(&h, 1, fion1, isset1, &rc2);

    wtof("TST75SEL B#%d P=%d C=%d H=%d port=%u layout=%s", k, p, c, h,
         port, made ? "C<H" : "NOT-AS-PLANNED");
    wtof("TST75SEL B#%d {C,H} rc=%d C:%d/%d H:%d/%d bad=%d", k, rc,
         fion[0], isset[0], fion[1], isset[1], bad);
    wtof("TST75SEL B#%d ctl {C} rc=%d bad=%d {H} rc=%d bad=%d", k,
         rc1, b1, rc2, b2);

    anomalies += bad + b1 + b2;

out:
    if (c  >= 0) closesocket(c);
    if (sv >= 0) closesocket(sv);
    if (h  >= 0) closesocket(h);
    if (made < 0) wtof("TST75SEL B#%d setup failed", k);
    return made;
}

int main(void)
{
    struct sockaddr_in addr;
    unsigned i;
    int  k, m;
    int  setup = 0;
    int  created = 0;

    printf("TST75SEL - libc370 #144 probe, see the job log\n");

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) {
        wtof("TST75SEL RESULT setup: no listener socket - RC 8");
        return 8;
    }
    for (i = 0; i < PORTTRIES; i++) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)(PORTBASE + i));
        addr.sin_addr.s_addr = 0x7F000001;
        if (bind(lsock, &addr, sizeof(addr)) >= 0) {
            lport = PORTBASE + i;
            break;
        }
    }
    if (!lport || listen(lsock, 10) < 0) {
        closesocket(lsock);
        wtof("TST75SEL RESULT setup: no loopback port - RC 8");
        return 8;
    }
    wtof("TST75SEL listener s=%d on 127.0.0.1:%u", lsock, lport);

    setup |= phase_a();

    for (k = 0; k < BTRIES; k++) {
        m = phase_b(k);
        if (m < 0) setup = 1;
        if (m > 0) created++;
    }

    closesocket(lsock);

    if (anomalies) {
        wtof("TST75SEL RESULT DEFECT: %d anomalies, B layout %d/%d - RC 4",
             anomalies, created, BTRIES);
        return 4;
    }
    if (setup || !created) {
        wtof("TST75SEL RESULT NO VERDICT: setup=%d, B layout %d/%d - RC 8",
             setup, created, BTRIES);
        return 8;
    }
    wtof("TST75SEL RESULT CLEAN: 0 anomalies, B layout %d/%d - RC 0",
         created, BTRIES);
    return 0;
}
