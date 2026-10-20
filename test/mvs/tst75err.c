/*
 * tst75err.c - libc370 #161 probe (MVS target, batch): can the error a
 * program reads back through X'75' be somebody else's?
 *
 * ISSUE #161: the emulator keeps socket errors in two places, and they are
 * read by two different X'75' function codes (hyperion tcpip.c):
 *
 *   code 2  GETERRORS(s)  returns Cerr[s] - one word per socket SLOT.  Every
 *           per-socket call (connect, bind, recv, send, accept, ioctl, ...)
 *           writes it on failure.  libc370 reads it after every failing
 *           socket call except socket() itself.
 *   code 3  GETERROR      returns CerrGen - ONE word for the whole emulator,
 *           written only by failing SOCKET, GETHOSTBYNAME and GETHOSTBYADDR.
 *           libc370 reads it after a failing socket() (@@75sock.c).
 *
 * Socket slot numbers are handed out emulator-wide, lowest free first, and
 * CLOSE resets the slot's handle, open and blocking flags - but not Cerr[].
 * So there are three ways a guest can read an error that is not its own,
 * and this probe measures each one separately, plus what select() makes of
 * the second one (D):
 *
 *   A. CROSSED SLOT   error 1 on socket X, error 2 on socket Y, then read X.
 *                     If X reads error 2, Cerr is not per socket.  The source
 *                     predicts no crossing; this path is deterministic, so a
 *                     clean count here IS informative.
 *   B. STALE SLOT     a freshly created socket, before any call has failed on
 *                     it, reads back the error of the slot's previous holder.
 *                     The source predicts it does (CLOSE never clears Cerr),
 *                     and since slots are emulator-wide the previous holder
 *                     can be another address space.
 *   C. CROSSED GLOBAL a failed socket() records error G, the program waits,
 *                     then reads GETERROR.  Anything other than G was written
 *                     by somebody else in between.  In one address space
 *                     nothing else writes it (unless another task on the
 *                     system has a failing socket()/gethostbyname()/
 *                     gethostbyaddr() at that moment); the two-address-space
 *                     variant below provokes exactly that.
 *   D. SELECT ON A RE-USED NUMBER
 *                     select() - the library function - waits on socket B.
 *                     B got the number of socket A, closed after a refused
 *                     connect (Cerr 61).  A subtask closes B under the
 *                     waiting select and takes the number again with a raw
 *                     SOCKET (C); on odd rounds C then fails a bind of its
 *                     own (EADDRINUSE, 48).  The emulator fails select's next
 *                     poll (the run's state went with the CLOSE) and sets no
 *                     error word for it.  The errno select() reports:
 *                       38 ENOTSOCK   right: the socket is gone
 *                       61            A's stale word (slot never cleared)
 *                       48            C's word, another socket's error
 *                        0            nothing, on a -1 return
 *                     Only this part goes through the library, so only its
 *                     result differs between the G and R steps.
 *
 * MODES (PARM):
 *   (none)      single address space, ROUNDS rounds of A + B + C.
 *   GEN=n       C only, for two concurrent jobs: provoke global error n
 *               (1 = EPROTONOSUPPORT, 2 = EAFNOSUPPORT, 3 = EINVAL), wait
 *               WINDOW, read GETERROR; GENROUNDS rounds.  Run two jobs with
 *               different n at the same time (jcl/tst75erx.jcl); each counts
 *               the rounds in which it read the other job's error.
 *
 * HOW THE ERRORS ARE PROVOKED (all through __75() directly, one pair of
 * X'75' instructions each, so no libc370 wrapper reads the error back early):
 *   ECONNREFUSED (61)    connect() to a loopback port nobody listens on
 *   EADDRINUSE   (48)    bind() to the probe's own listening port
 *   ENOTCONN     (57)    getpeername() on a socket that is not connected
 *   EPROTONOSUPPORT (43) socket(AF_INET, SOCK_STREAM, 99)   - global
 *   EAFNOSUPPORT (47)    socket(99, SOCK_STREAM, 0)          - global
 *   EINVAL       (22)    socket(AF_INET, 99, 0)              - global
 * A round counts only if the provoking call returned -1 and the immediate
 * read-back shows the expected code; otherwise it is reported as NOT
 * PROVOKED and takes no part in the counts.
 *
 * WHAT A NEGATIVE RESULT MEANS: for C, that the window was missed, not that
 * the defect is absent - CerrGen is one variable and the race is genuine.
 * For A and B the emulator code involved is deterministic, so their counts
 * stand as they are.
 *
 * BUILD (host, from the repository root, after make; S a scratch directory,
 * R a directory holding a libc.a built from the release being compared
 * against):
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *         test/mvs/tst75err.c -o $S/TST75ERG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L $R \
 *         test/mvs/tst75err.c -o $S/TST75ERR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TST75ERG=TST75ERG.iebcopy \
 *         TST75ERR=TST75ERR.iebcopy -o probe -xmit --dsn IBMUSER.LIBC370.XESCR
 *
 * RUN: jcl/recv75err.jcl, then jcl/tst75err.jcl (single address space,
 * steps GREEN = TST75ERG and RED = TST75ERR), jcl/tst75erx.jcl (two
 * concurrent jobs; check from IEF403I/IEF404I in both job logs that they
 * actually overlapped before reading a zero as anything).
 *
 * OUTPUT: one WTO per round (TST75ERR r=...: socket numbers X and Y, the
 * error provoked on each, fr = the fresh read before any failure, got = the
 * immediate read-back, post = X read again after Y's error, G = the global
 * error and what GETERROR returned; flags X = crossed, S = stale), one per
 * select round (TST75ERR sel r=...), then:
 *   TST75ERR SUMMARY rounds=.. provoked=.. crossed_slot=.. stale_slot=..
 *            crossed_global=..
 *   TST75ERR SUMMARY select rounds=.. failed=.. enotsock=.. stale=..
 *            newholder=.. zero=.. other=.. notcreated=..
 *
 * RC: 0 = nothing crossed, nothing stale; 4 = a condition could not be
 * created (a provocation did not give its expected code) and nothing was
 * seen; 8 = at least one read returned an error that was not the reader's
 * own (A, B or C), or a failed select() reported anything but ENOTSOCK (D).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/thread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>
#include "src/net/dyn75/dyn75.h"
#include "src/net/dyn75/x75.h"

#define ROUNDS        50
#define SELROUNDS     10
#define GENROUNDS    300
#define PORTBASE   27600            /* first port tried for the listener    */
#define PORTTRIES     10
#define DEADPORT   27690            /* first port tried as "nobody there"   */
#define NSLOT        256

#define K_REFUSED      0            /* per-socket error kinds               */
#define K_INUSE        1
#define K_NOTCONN      2
#define NKIND          3

static const int   kind_err[NKIND]  = { 61, 48, 57 };
static const char *kind_name[NKIND] = { "REFUSED", "INUSE", "NOTCONN" };

static const int   gen_err[NKIND]   = { 43, 47, 22 };
static const char *gen_name[NKIND]  = { "PROTO", "AF", "INVAL" };

static unsigned lport;              /* the probe's listening port           */
static unsigned dport;              /* a port with no listener              */
static int      last_err[NSLOT];
static unsigned char peer[64];      /* GETPEERNAME output, never used        */

struct selctx {                     /* D: what the subtask did              */
    int     b;                      /* the number to close                  */
    int     own;                    /* 1 = C provokes an error of its own   */
    int     c;                      /* what SOCKET handed out               */
    int     crc;                    /* the provocation's return code        */
};

static int  x75(unsigned r7, unsigned r8, unsigned r9);
static int  x75b(unsigned r7, unsigned r8, unsigned r9, void *r6);
static int  geterrs(int s);
static int  geterr(void);
static int  sock_fail(int g);
static int  provoke(int s, int kind);
static void snooze(unsigned ticks);
static int  single(void);
static int  general(int g);
static int  selcase(void);
static int  closer(void *a, void *b);

int main(int argc, char **argv)
{
    if (argc > 1 && strncmp(argv[1], "GEN=", 4) == 0) {
        int g = atoi(argv[1] + 4);
        if (g < 1 || g > NKIND) {
            wtof("TST75ERR bad PARM %s", argv[1]);
            return 12;
        }
        return general(g - 1);
    }
    return single();
}

/*
 * Single address space: A, B and C each round.
 */
static int single(void)
{
    struct sockaddr_in  addr;
    int     lsock;
    int     s;
    int     i;
    int     r;
    int     provoked = 0, crossed = 0, stale = 0, stale_prev = 0;
    int     gcrossed = 0, gprovoked = 0, notprov = 0;
    int     selbad;

    wtof("TST75ERR start single-AS rounds=%d", ROUNDS);

    for (i = 0; i < NSLOT; i++) last_err[i] = -1;

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) {
        wtof("TST75ERR FAILED - no listener socket");
        return 12;
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
    if (lport == 0 || listen(lsock, 5) < 0) {
        wtof("TST75ERR FAILED - no listening port");
        closesocket(lsock);
        return 12;
    }

    /* a port with nobody behind it: bind a socket to it, never listen,
       close it again - the bind proves the port was free */
    for (i = 0; i < PORTTRIES && dport == 0; i++) {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) break;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)(DEADPORT + i));
        addr.sin_addr.s_addr = 0x7F000001;
        if (bind(s, &addr, sizeof(addr)) >= 0) dport = DEADPORT + i;
        closesocket(s);
    }
    if (dport == 0) {
        wtof("TST75ERR FAILED - no free port for the refused connect");
        closesocket(lsock);
        return 12;
    }

    wtof("TST75ERR listener=%d port=%u deadport=%u", lsock, lport, dport);

    for (r = 0; r < ROUNDS; r++) {
        int kx = r % NKIND;
        int ky = (r + 1) % NKIND;
        int g  = r % NKIND;
        int x, y;
        int fx, fy;                 /* fresh read, before any failure       */
        int px, py;                 /* provoke() return codes               */
        int ex, ey;                 /* immediate read-back                  */
        int post;                   /* X read again after Y's error         */
        int gr, gget;
        int ok;
        char cx = '-', sx = '-', sy = '-', cg = '-';

        /* C: record a global error first; it must survive everything below,
           none of which writes CerrGen */
        gr = sock_fail(g);

        x  = socket(AF_INET, SOCK_STREAM, 0);
        y  = -1;
        if (x < 0 || x >= NSLOT) {
            wtof("TST75ERR r=%d socket() for X failed", r);
            notprov++;
            continue;
        }
        fx = geterrs(x);
        px = provoke(x, kx);
        ex = geterrs(x);

        y  = socket(AF_INET, SOCK_STREAM, 0);
        if (y < 0 || y >= NSLOT) {
            wtof("TST75ERR r=%d socket() for Y failed", r);
            closesocket(x);
            notprov++;
            continue;
        }
        fy = geterrs(y);
        py = provoke(y, ky);
        ey = geterrs(y);

        post = geterrs(x);
        gget = geterr();

        ok = (px == -1 && ex == kind_err[kx] && py == -1
              && ey == kind_err[ky]);

        if (ok) {
            provoked++;
            if (post != ex) { crossed++; cx = 'X'; }
        } else {
            notprov++;
        }

        /* B: a nonzero first read on a fresh socket is inherited; note
           whether it is exactly what this probe saw in that slot last */
        if (fx != 0) {
            stale++; sx = 'S';
            if (fx == last_err[x]) stale_prev++;
        }
        if (fy != 0) {
            stale++; sy = 'S';
            if (fy == last_err[y]) stale_prev++;
        }

        if (gr == -1) {
            gprovoked++;
            if (gget != gen_err[g]) { gcrossed++; cg = 'X'; }
        }

        /* short: a WTO is cut at about 125 characters */
        wtof("TST75ERR r=%d X=%d %s fr=%d got=%d post=%d %c%c"
             " Y=%d %s fr=%d got=%d %c G=%s got=%d %c",
             r, x, kind_name[kx], fx, ex, post, cx, sx,
             y, kind_name[ky], fy, ey, sy, gen_name[g], gget, cg);

        if (!ok) {
            wtof("TST75ERR r=%d NOT PROVOKED px=%d py=%d", r, px, py);
        }

        last_err[x] = post;
        last_err[y] = ey;
        closesocket(x);
        closesocket(y);
    }

    selbad = selcase();

    closesocket(lsock);

    wtof("TST75ERR SUMMARY rounds=%d provoked=%d notprov=%d crossed_slot=%d",
         ROUNDS, provoked, notprov, crossed);
    wtof("TST75ERR SUMMARY stale_slot=%d (=prev holder %d) gen_provoked=%d"
         " crossed_global=%d", stale, stale_prev, gprovoked, gcrossed);

    if (crossed || stale || gcrossed || selbad > 0) {
        wtof("TST75ERR DEFECT SEEN - RC 8");
        return 8;
    }
    if (notprov || gprovoked < ROUNDS || selbad < 0) {
        wtof("TST75ERR CONDITION NOT CREATED in every round - RC 4");
        return 4;
    }
    wtof("TST75ERR nothing crossed - RC 0");
    return 0;
}

/*
 * D: select() on a number that is closed and handed out again under it.
 * Returns the number of rounds with a wrong errno, or -1 if none was wrong
 * and some round could not be set up.
 */
static int selcase(void)
{
    struct sockaddr_in  addr;
    struct selctx       ctx;
    CTHDTASK            *task;
    fd_set              rset;
    timeval             tv;
    int     r;
    int     a, b;
    int     rc, err;
    int     failed = 0, enotsock = 0, stale = 0, newholder = 0, zero = 0;
    int     other = 0, notcreated = 0;
    const char *what;

    for (r = 0; r < SELROUNDS; r++) {
        /* A: a refused connect leaves 61 in the slot, then A goes */
        a = socket(AF_INET, SOCK_STREAM, 0);
        if (a < 0 || provoke(a, K_REFUSED) != -1) {
            wtof("TST75ERR sel r=%d A not set up (%d)", r, a);
            if (a >= 0) closesocket(a);
            notcreated++;
            continue;
        }
        closesocket(a);

        /* B: same number, listening, so nothing makes it readable */
        b = socket(AF_INET, SOCK_STREAM, 0);
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = 0x7F000001;
        if (b != a || bind(b, &addr, sizeof(addr)) < 0 || listen(b, 1) < 0) {
            wtof("TST75ERR sel r=%d B not set up (A=%d B=%d)", r, a, b);
            if (b >= 0) closesocket(b);
            notcreated++;
            continue;
        }

        ctx.b   = b;
        ctx.own = r & 1;
        ctx.c   = -1;
        ctx.crc = 0;
        task = cthread_create((void *)closer, &ctx, NULL);
        if (!task) {
            wtof("TST75ERR sel r=%d cthread_create failed", r);
            closesocket(b);
            notcreated++;
            continue;
        }

        FD_ZERO(&rset);
        FD_SET(b, &rset);
        tv.tv_sec  = 3;
        tv.tv_usec = 0;
        errno = 0;
        rc  = select(b + 1, &rset, NULL, NULL, &tv);
        err = errno;

        cthread_wait(&task->termecb);
        cthread_delete(&task);

        /* B's number now belongs to C; close what is there */
        if (ctx.c >= 0 && ctx.c != b) x75(12, (unsigned)ctx.c, 0);
        closesocket(b);

        if (ctx.c != b || rc != -1 || (ctx.own && ctx.crc != -1)) {
            what = "NOT CREATED";
            notcreated++;
        } else {
            failed++;
            if (err == ENOTSOCK)              { what = "ENOTSOCK"; enotsock++; }
            else if (err == kind_err[K_REFUSED]) { what = "STALE"; stale++; }
            else if (err == kind_err[K_INUSE])   { what = "NEWHOLDER"; newholder++; }
            else if (err == 0)                { what = "ZERO"; zero++; }
            else                              { what = "OTHER"; other++; }
        }
        wtof("TST75ERR sel r=%d B=%d C=%d own=%d crc=%d rc=%d errno=%d %s",
             r, b, ctx.c, ctx.own, ctx.crc, rc, err, what);
    }

    wtof("TST75ERR SUMMARY select rounds=%d failed=%d enotsock=%d stale=%d"
         " newholder=%d zero=%d other=%d notcreated=%d", SELROUNDS, failed,
         enotsock, stale, newholder, zero, other, notcreated);

    if (failed - enotsock) return failed - enotsock;
    return notcreated ? -1 : 0;
}

/* D's subtask: let select() start waiting, then close its socket and take
   the number again, raw, so no library state in this task is involved */
static int closer(void *a, void *b)
{
    struct selctx *ctx = a;

    (void)b;
    snooze(40);                     /* 0.4 s: select() is polling by now */
    x75(12, (unsigned)ctx->b, 0);
    ctx->c = x75(5, (AF_INET << 16) | SOCK_STREAM, 0);
    if (ctx->own && ctx->c >= 0) ctx->crc = provoke(ctx->c, K_INUSE);
    return 0;
}

/*
 * Global error only, for two concurrent jobs.  Provoke, wait, read.
 */
static int general(int g)
{
    int     s;
    int     r;
    int     gr;
    int     got;
    int     prov = 0, crossed = 0, other = 0, last = 0;
    int     seen[NKIND] = { 0, 0, 0 };
    int     k;

    /* a real socket first: initializes the interface (as socket() does) */
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s >= 0) closesocket(s);

    wtof("TST75ERR start GEN=%s rounds=%d time=%lu", gen_name[g],
         GENROUNDS, (unsigned long)time(NULL));

    for (r = 0; r < GENROUNDS; r++) {
        gr = sock_fail(g);
        if (gr != -1) {
            wtof("TST75ERR r=%d NOT PROVOKED rc=%d", r, gr);
            continue;
        }
        prov++;
        snooze(16);                 /* the window: 0.16 s */
        got = geterr();
        if (got != gen_err[g]) {
            crossed++;
            for (k = 0; k < NKIND; k++) {
                if (got == gen_err[k]) seen[k]++;
            }
            if (got != gen_err[0] && got != gen_err[1] && got != gen_err[2]) {
                other++;
            }
            if (crossed <= 20) {
                wtof("TST75ERR r=%d own=%d got=%d CROSSED", r,
                     gen_err[g], got);
            }
        }
        last = got;
    }

    wtof("TST75ERR end GEN=%s time=%lu last=%d", gen_name[g],
         (unsigned long)time(NULL), last);
    wtof("TST75ERR SUMMARY GEN=%s rounds=%d provoked=%d crossed_global=%d"
         " (%d=%d %d=%d %d=%d other=%d)", gen_name[g], GENROUNDS, prov,
         crossed, gen_err[0], seen[0], gen_err[1], seen[1],
         gen_err[2], seen[2], other);

    if (crossed) return 8;
    return prov == GENROUNDS ? 0 : 4;
}

/*
 * One X'75' pair with a freshly cleared parameter list - __75() writes the
 * registers back into it, so a reused list carries the last call's state.
 */
static int x75(unsigned r7, unsigned r8, unsigned r9)
{
    return x75b(r7, r8, r9, 0);
}

/* r6 = the guest buffer the second instruction copies host data into */
static int x75b(unsigned r7, unsigned r8, unsigned r9, void *r6)
{
    PL75    pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl) : "memory");
    pl.r7 = r7;
    pl.r8 = r8;
    pl.r9 = r9;
    pl.r6 = (unsigned) r6;

    __75(&pl);

    return (int) pl.r4;
}

static int geterrs(int s)
{
    return x75(2, (unsigned)s, 0);
}

static int geterr(void)
{
    return x75(3, 0, 0);
}

/* a socket() that must fail; returns -1, or a socket that is then closed */
static int sock_fail(int g)
{
    unsigned af = AF_INET, type = SOCK_STREAM, proto = 0;
    int      rc;

    if (g == 0) proto = 99;
    if (g == 1) af    = 99;
    if (g == 2) type  = 99;

    rc = x75(5, (af << 16) | type, proto);
    if (rc >= 0) {
        x75(12, (unsigned)rc, 0);   /* unexpectedly made one: close it */
    }
    return rc;
}

static int provoke(int s, int kind)
{
    unsigned r9;
    int      rc = 0;
    int      tries;

    switch (kind) {
    case K_REFUSED:
        r9 = ((AF_INET << 16) | dport) & 0x00FFFFFF;
        for (tries = 0; tries < 20; tries++) {
            rc = x75(((unsigned)s << 16) | 7, 0x7F000001, r9);
            if (rc != -2) break;
            snooze(8);
        }
        return rc;

    case K_INUSE:
        r9 = ((AF_INET << 16) | lport) & 0x00FFFFFF;
        return x75(((unsigned)s << 16) | 6, 0x7F000001, r9);

    case K_NOTCONN:
        /* GETPEERNAME hands back its 16-byte address buffer even when
           the call fails, so the second instruction needs a real target
           (with R6 = 0 it stores into page 0: S0C4) */
        return x75b(19, (unsigned)s, 0, peer);
    }
    return 0;
}

static void snooze(unsigned ticks)
{
    unsigned t;

    /* 0.08 s steps: the literal form of STIMER, as in tst75rst.c */
    for (t = 0; t < ticks; t += 8) {
        __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds"
                : : : "0", "1", "14", "15", "memory");
    }
}
