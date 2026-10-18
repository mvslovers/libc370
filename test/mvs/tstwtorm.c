/*
 * tstwtorm.c - libc370 #404 on MVS: wtor() terminates the reply within
 * replymax, and wtorf() with a text that ends in a newline issues the
 * WTOR.  Needs an operator - run it where the console may be answered.
 *
 *   A wtorf(buf, 4, ...) answered with ABC: buf is "ABC", a null in
 *     buf[3], the bytes behind replymax untouched.  The old wtor() asked
 *     for 4 characters and stored "ABC" with no null behind it.  A longer
 *     answer never arrives: MVS refuses a reply longer than the WTOR asked
 *     for and keeps the WTOR outstanding.
 *   B wtorf(buf, 8, "...\n...REPLY OK\n"): the last line is a WTOR and
 *     buf is "OK".  The old wtorf() wrote both lines as messages and
 *     returned without a reply.
 *   C (CASE=3 only) wtorf(buf, 1, ...): no reply characters fit; the new
 *     wtor() stores the null and issues the message without waiting.
 *
 * Every WTOR text starts "TSTWTORM <case>" so a driver can find and
 * answer it (D R,L, then R nn,...): A ABC, B OK, C (old) Z.
 *
 * BUILD as test/mvs/tstwtom.c, CASE=0 and CASE=3 against build/sdk
 * (TSTWRG, TSTWRGC) and against the installed library (TSTWRR, TSTWRRC);
 * then jcl/recvwtorm.jcl and jcl/tstwtorm.jcl.
 *
 * RED, 2026-10-18, against 2.18.1: A got C1 C2 C3 E7 ("ABC", then the
 * fill byte), B issued no WTOR, C waited for a reply.  GREEN: 3 of 3.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <string.h>
#include <mvs/wto.h>

#ifndef CASE
#define CASE 0
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTWTORM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

static void fill(char *buf, int n)
{
    int i;

    for (i = 0; i < n; i++) buf[i] = 'X';
}

static int untouched(const char *buf, int from, int n)
{
    int i;

    for (i = from; i < n; i++) if (buf[i] != 'X') return 0;
    return 1;
}

int main(void)
{
    char    buf[12];

#if CASE == 0
    fill(buf, sizeof(buf));
    wtorf(buf, 4, "TSTWTORM A REPLY ABC");
    want(memcmp(buf, "ABC", 4) == 0 && untouched(buf, 4, sizeof(buf)),
         "A reply cut to 3 characters and a null, rest untouched");
    if (!(memcmp(buf, "ABC", 4) == 0))
        wtof("TSTWTORM A got %02X %02X %02X %02X %02X", buf[0] & 0xFF,
             buf[1] & 0xFF, buf[2] & 0xFF, buf[3] & 0xFF, buf[4] & 0xFF);

    fill(buf, sizeof(buf));
    wtorf(buf, 8, "TSTWTORM B FIRST LINE\nTSTWTORM B REPLY OK\n");
    want(strcmp(buf, "OK") == 0 && untouched(buf, 8, sizeof(buf)),
         "B trailing newline: the last line was the WTOR");
#endif
#if CASE == 3
    fill(buf, sizeof(buf));
    wtorf(buf, 1, "TSTWTORM C REPLY Z");
    want(buf[0] == 0 && untouched(buf, 1, sizeof(buf)),
         "C replymax 1: a null and no wait");
#endif
    wtof("TSTWTORM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
