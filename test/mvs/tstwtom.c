/*
 * tstwtom.c - libc370 #404 on MVS: wto() leaves the caller's text alone,
 * wtodump() and wtodumpf() stay inside their buffers.
 *
 *   1 wto() of a 300-character text: several messages, the text unchanged
 *     afterwards (the old wto() moved its rest to the front, in place)
 *   2 wtodump() with chunks 36, 0 and -5: the dump comes out (the old one
 *     ran past its 80-byte lines; 0 and below without bound)
 *   3 wtodumpf() with a 199-character title (the old one ran past its
 *     80-byte title on the stack)
 *
 * CASE=0 runs all three (this tree); the red build against the installed
 * library runs one case per step, CASE=1..3.  wtorf() needs an operator
 * reply and is tested on the host only (test/host/tstwto.c).
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -DCASE=0 -Iinclude -L build/sdk \
 *         test/mvs/tstwtom.c -o $S/TSTWTOM -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -DCASE=1 test/mvs/tstwtom.c \
 *         -o $S/TSTWTOR1 -flinker-output=iebcopy        (2 and 3 alike)
 *     cd $S && ld370 --pack TSTWTOM=TSTWTOM.iebcopy \
 *         TSTWTOR1=TSTWTOR1.iebcopy ... -o wt -xmit \
 *         --dsn IBMUSER.LIBC370.WTSCR
 *
 * then jcl/recvwtom.jcl and jcl/tstwtom.jcl.
 *
 * RED, 2026-10-18, against 2.18.1: case 1 FAIL (RC 8), cases 2 and 3
 * S0C4.  GREEN: 3 of 3.
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
    wtof("TSTWTOM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

#if CASE == 0 || CASE == 1
static char text[] =
    "ALPHA BRAVO CHARLIE DELTA ECHO FOXTROT GOLF HOTEL INDIA JULIETT KILO "
    "LIMA MIKE NOVEMBER OSCAR PAPA QUEBEC ROMEO SIERRA TANGO UNIFORM VICTOR "
    "WHISKEY XRAY YANKEE ZULU ALPHA BRAVO CHARLIE DELTA ECHO FOXTROT GOLF "
    "HOTEL INDIA JULIETT KILO LIMA MIKE NOVEMBER OSCAR PAPA QUEBEC ROMEO END";
#endif

int main(void)
{
    char    area[100];
    char    title[200];
    int     i;

    for (i = 0; i < (int)sizeof(area); i++) area[i] = (char)i;
    for (i = 0; i < (int)sizeof(title) - 1; i++) title[i] = 'T';
    title[sizeof(title) - 1] = 0;

#if CASE == 0 || CASE == 1
    {
        char    copy[sizeof(text)];

        memcpy(copy, text, sizeof(text));
        wto(text);
        want(memcmp(copy, text, sizeof(text)) == 0,
             "1 wto() of 300 characters leaves the text unchanged");
    }
#endif
#if CASE == 0 || CASE == 2
    wtodump("CHUNK36", area, sizeof(area), 36);
    wtodump("CHUNK0", area, sizeof(area), 0);
    wtodump("CHUNKM5", area, sizeof(area), -5);
    want(1, "2 wtodump() with chunks 36, 0, -5 came back");
#endif
#if CASE == 0 || CASE == 3
    wtodumpf(area, 16, "%s", title);
    want(1, "3 wtodumpf() with a 199-character title came back");
#endif
    wtof("TSTWTOM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
