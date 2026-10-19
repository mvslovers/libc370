/*
 * tst75prt.c - libc370 #165 probe (MVS target, batch): does a PARTIAL X'75'
 * RECV consume the bytes it returned, or are they delivered again by the
 * next call?
 *
 * ISSUE #165: a corruption report against another guest (mvs_nfsd socktest)
 * traced a 636-byte message as
 *
 *     0: at buf+0    requested 636  returned 256   <-- PARTIAL
 *     1: at buf+256  requested 380  returned 380
 *     first bad at offset 256, tail carries the message from byte 0
 *
 * and read it as "a partial RECV does not consume what it returned".  If so,
 * every guest that loops on partial reads - @@75recv.c among them - is
 * corrupted.  The #154 restart defect cannot explain it: under that defect
 * the first bad byte would be at 512, not 256.
 *
 * WHAT THIS PROBE DOES
 * --------------------
 * It owns both ends of a loopback pair (as tst75rst.c does) and forces the
 * partial read instead of waiting for one:
 *
 *   1. the peer sends the first P bytes of a 636-byte i % 251 pattern;
 *   2. FIONREAD until P bytes are queued, then ONE __75() RECV for 636 -
 *      the host recv() can only return what is queued, so the count is short;
 *   3. FIONREAD again: if the RECV consumed what it returned this is 0;
 *   4. the peer sends the remaining 636-P bytes;
 *   5. FIONREAD until they are queued, then ONE __75() RECV for 636-got;
 *   6. FIONREAD again (must be 0) and compare the whole buffer to the
 *      pattern.  A replay puts PATTERN(0..) at the continuation offset.
 *
 * Split points: 256 (the reported case) and points that are not multiples
 * of 256 (1, 100, 255, 257, 380, 500, 635).  A replay at a split that is not
 * a multiple of 256 cannot be the #154 restart defect.  Each split runs
 * FORCEDREP times.  Then RACE rounds send all 636 bytes and receive at once
 * with no waiting, checking whatever partial reads happen by themselves.
 *
 * DETAILS
 * -------
 * - RECV goes through __75() directly, so one call is one pair of X'75'
 *   instructions; recv() would loop in 256-byte pieces.
 * - Pattern i % 251: no period divides a 256 split, so a replay cannot look
 *   right by accident.
 * - The peer sends in pieces of at most 256 bytes (the send side has the
 *   mirror of #154 and is immune at one segment).
 * - The receive buffer is written immediately before each RECV, so it is
 *   resident and the #154 restart path is not entered.
 * - The same guest is on both ends and the host stack is loopback; the
 *   reported case had an external sender.  A clean result here says the
 *   partial-RECV path itself consumes correctly, not that their setup cannot
 *   fail.
 *
 * BUILD (host, from the repository root, after make):
 *     cc370 -O1 -Wall -Werror -Iinclude -I. -L build/sdk \
 *         test/mvs/tst75prt.c -o TST75PRT -flinker-output=iebcopy
 *     ld370 --pack TST75PRT=TST75PRT.iebcopy -o probe -xmit --dsn <PDS>
 *
 * RUN: jcl/tst75prt.jcl.
 *
 * OUTPUT: one WTO per forced round
 *   TST75PRT r=.. P=.. c1 fion=.. req=636 got=.. after=.. c2 fion=.. req=..
 *            got=.. after=.. OK|REPLAY|CORRUPT|LEFTOVER
 * and the totals
 *   TST75PRT SUMMARY forced=.. short=.. replay=.. corrupt=.. leftover=..
 *   TST75PRT SUMMARY race=.. race_short=.. race_bad=..
 *
 * RC: 0 = short counts happened and every continuation was correct;
 *     4 = a short count never happened - the condition was not created, no
 *         verdict (this is NOT a pass);
 *     8 = at least one replay, corruption or leftover byte count.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mvs/socket.h>
#include "src/net/dyn75/dyn75.h"
#include "src/net/dyn75/x75.h"

#define MSGLEN       636            /* the reported message length          */
#define SEG          256
#define FORCEDREP      3            /* rounds per split point               */
#define RACEROUNDS    20
#define PORTBASE   27650
#define PORTTRIES     10
#define WAITMAX      250            /* x 0.08 s                             */
#define MAXCALLS      20

#define PATTERN(i)  ((unsigned char)((i) % 251))

static const unsigned split[] = { 256, 1, 100, 255, 257, 380, 500, 635 };
#define NSPLIT (sizeof(split) / sizeof(split[0]))

static unsigned char out[MSGLEN];
static unsigned char buf[MSGLEN];

static int  recv75(int s, void *b, unsigned len);
static int  send_all(int s, const unsigned char *b, unsigned len);
static int  fionread(int s);
static int  wait_for(int s, unsigned want);
static int  check(unsigned len, unsigned *first_bad, int *replay);
static void snooze(void);

int main(void)
{
    struct sockaddr_in  addr;
    int     lsock, csock = -1, ssock = -1;
    int     addrlen;
    unsigned port = 0;
    unsigned i;
    unsigned r;
    int     forced = 0, shortc = 0, replay = 0, corrupt = 0, leftover = 0;
    int     race = 0, race_short = 0, race_bad = 0, setup_bad = 0;

    for (i = 0; i < MSGLEN; i++) out[i] = PATTERN(i);

    lsock = socket(AF_INET, SOCK_STREAM, 0);
    for (i = 0; lsock >= 0 && i < PORTTRIES; i++) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)(PORTBASE + i));
        addr.sin_addr.s_addr = 0x7F000001;
        if (bind(lsock, &addr, sizeof(addr)) >= 0) {
            port = PORTBASE + i;
            break;
        }
    }
    if (port == 0 || listen(lsock, 5) < 0) {
        wtof("TST75PRT FAILED - no listener");
        return 12;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((unsigned short)port);
    addr.sin_addr.s_addr = 0x7F000001;
    csock = socket(AF_INET, SOCK_STREAM, 0);
    if (csock < 0 || connect(csock, &addr, sizeof(addr)) < 0) {
        wtof("TST75PRT FAILED - connect");
        return 12;
    }
    addrlen = sizeof(addr);
    ssock = accept(lsock, &addr, &addrlen);
    if (ssock < 0) {
        wtof("TST75PRT FAILED - accept");
        return 12;
    }

    wtof("TST75PRT start port=%u peer=%d recv=%d msglen=%d", port, csock,
         ssock, MSGLEN);

    /* forced partial reads */
    for (r = 0; r < NSPLIT * FORCEDREP; r++) {
        unsigned P = split[r % NSPLIT];
        int      f0, f1, f2, f3;
        int      g1, g2, g;
        unsigned have, first_bad = 0;
        int      calls = 2;
        int      rep;
        const char *verdict;

        if (send_all(csock, out, P) < 0 || wait_for(ssock, P) < 0) {
            wtof("TST75PRT r=%u P=%u setup failed (first part)", r, P);
            setup_bad++;
            break;
        }
        f0 = fionread(ssock);
        memset(buf, 0xEE, sizeof(buf));
        g1 = recv75(ssock, buf, MSGLEN);
        f1 = fionread(ssock);

        have = (g1 > 0) ? (unsigned)g1 : 0;

        if (send_all(csock, out + P, MSGLEN - P) < 0
            || wait_for(ssock, MSGLEN - have) < 0) {
            wtof("TST75PRT r=%u P=%u setup failed (rest) g1=%d f1=%d",
                 r, P, g1, f1);
            setup_bad++;
            break;
        }
        f2 = fionread(ssock);
        memset(buf + have, 0xEE, MSGLEN - have);
        g2 = recv75(ssock, buf + have, MSGLEN - have);
        f3 = fionread(ssock);
        if (g2 > 0) have += (unsigned)g2;

        /* collect anything still outstanding, so the next round starts clean
           and a short second call is not misread as corruption */
        while (have < MSGLEN && calls < MAXCALLS) {
            if (wait_for(ssock, 1) < 0) break;
            g = recv75(ssock, buf + have, MSGLEN - have);
            calls++;
            if (g <= 0) break;
            have += (unsigned)g;
        }

        forced++;
        if (g1 > 0 && (unsigned)g1 < MSGLEN) shortc++;

        rep = 0;
        if (have != MSGLEN) {
            verdict = "INCOMPLETE";
            corrupt++;
        } else if (check(MSGLEN, &first_bad, &rep) == 0) {
            verdict = "OK";
        } else if (rep) {
            verdict = "REPLAY";
            replay++;
        } else {
            verdict = "CORRUPT";
            corrupt++;
        }
        if (f1 != 0 && g1 == f0) {
            /* the RECV returned everything queued, yet bytes remain */
            leftover++;
        }
        if (f3 != 0 && have == MSGLEN) leftover++;

        wtof("TST75PRT r=%u P=%u c1 fion=%d req=%d got=%d after=%d c2 fion=%d"
             " req=%d got=%d after=%d calls=%d %s", r, P, f0, MSGLEN, g1, f1,
             f2, MSGLEN - (g1 > 0 ? g1 : 0), g2, f3, calls, verdict);
        if (strcmp(verdict, "OK") != 0 && have == MSGLEN) {
            wtof("TST75PRT r=%u first_bad=%u tail_from_0=%c", r, first_bad,
                 rep ? 'Y' : 'N');
        }
    }

    /* race: everything sent, then received at once with no waiting */
    for (r = 0; setup_bad == 0 && r < RACEROUNDS; r++) {
        unsigned have = 0, first_bad = 0;
        int      calls = 0, g, rep = 0, wasshort = 0, f;
        int      g0 = 0, f0 = 0;

        memset(buf, 0xEE, sizeof(buf));
        if (send_all(csock, out, MSGLEN) < 0) {
            setup_bad++;
            break;
        }
        while (have < MSGLEN && calls < MAXCALLS) {
            f = fionread(ssock);
            g = recv75(ssock, buf + have, MSGLEN - have);
            if (calls == 0) { g0 = g; f0 = f; }
            calls++;
            if (g == -2) { snooze(); continue; }
            if (g <= 0) break;
            if ((unsigned)g < MSGLEN - have) wasshort = 1;
            have += (unsigned)g;
        }
        race++;
        if (wasshort) race_short++;
        if (have != MSGLEN || check(MSGLEN, &first_bad, &rep) != 0) {
            race_bad++;
            wtof("TST75PRT race r=%u have=%u calls=%d first_bad=%u"
                 " tail_from_0=%c BAD", r, have, calls, first_bad,
                 rep ? 'Y' : 'N');
        } else if (wasshort || r == 0) {
            wtof("TST75PRT race r=%u calls=%d first fion=%d got=%d%s", r,
                 calls, f0, g0, wasshort ? " SHORT ok" : " ok");
        }
    }

    closesocket(csock);
    closesocket(ssock);
    closesocket(lsock);

    wtof("TST75PRT SUMMARY forced=%d short=%d replay=%d corrupt=%d"
         " leftover=%d setup_bad=%d", forced, shortc, replay, corrupt,
         leftover, setup_bad);
    wtof("TST75PRT SUMMARY race=%d race_short=%d race_bad=%d", race,
         race_short, race_bad);

    if (replay || corrupt || leftover || race_bad) {
        wtof("TST75PRT DEFECT SEEN - RC 8");
        return 8;
    }
    if (shortc == 0 || setup_bad) {
        wtof("TST75PRT SHORT COUNT NEVER HAPPENED / setup - no verdict, RC 4");
        return 4;
    }
    wtof("TST75PRT every partial RECV continued correctly - RC 0");
    return 0;
}

/* 0 = buffer is the pattern; else first bad offset, and whether the bytes
   from there on are the pattern restarted at 0 */
static int check(unsigned len, unsigned *first_bad, int *replay)
{
    unsigned i, fb;

    for (i = 0; i < len; i++) {
        if (buf[i] != PATTERN(i)) break;
    }
    if (i == len) return 0;
    fb = i;
    *first_bad = fb;
    *replay = 1;
    for (i = 0; fb + i < len; i++) {
        if (buf[fb + i] != PATTERN(i)) {
            *replay = 0;
            break;
        }
    }
    return 1;
}

/* one RECV, one pair of X'75' instructions */
static int recv75(int s, void *b, unsigned len)
{
    PL75    pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl) : "memory");
    pl.r6 = (unsigned) b;
    pl.r7 = 11;
    pl.r8 = (unsigned) s;
    pl.r9 = len;

    __75(&pl);

    return (int) pl.r4;
}

static int send_all(int s, const unsigned char *b, unsigned len)
{
    unsigned done = 0, chunk;
    int      rc = -1, tries;

    while (done < len) {
        chunk = len - done;
        if (chunk > SEG) chunk = SEG;
        for (tries = 0; tries < WAITMAX; tries++) {
            rc = send(s, b + done, (int)chunk, 0);
            if (rc != -2) break;
            snooze();
        }
        if (rc <= 0) return -1;
        done += (unsigned)rc;
    }
    return 0;
}

static int fionread(int s)
{
    int n = -1;

    if (ioctlsocket(s, FIONREAD, &n) < 0) return -1;
    return n;
}

static int wait_for(int s, unsigned want)
{
    int n, tries;

    for (tries = 0; tries < WAITMAX; tries++) {
        n = fionread(s);
        if (n < 0) return -1;
        if ((unsigned)n >= want) return 0;
        snooze();
    }
    return -1;
}

static void snooze(void)
{
    __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds"
            : : : "0", "1", "14", "15", "memory");
}
