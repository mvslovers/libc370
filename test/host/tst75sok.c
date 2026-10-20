/*
 * tst75sok.c - libc370 #161: the errno of a failed socket().
 *
 * #includes the real @@75sock.c; __75(), the one function that reaches the
 * X'75' instruction, is a fake.  SOCKET (code 5) fails, and GETERROR (code
 * 3) answers with the value under test.
 *
 * GETERROR reads one word for the whole emulator (hyperion tcpip.c,
 * CerrGen), written by a failing socket(), gethostbyname() or
 * gethostbyaddr() in any address space, and read with a second X'75'
 * instruction.  A task switch between the two lets another program's error
 * through: a lookup's code, or 0.  socket() keeps the word only if
 * socket() can fail that way, and reports ENOBUFS otherwise.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-trigraphs \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tst75sok.c && ./t
 *
 * RED, 2026-10-20, against main: 61 (ECONNREFUSED), 0, 65 and -1
 * came back as errno unchanged.
 *
 * RC: 0 = every check passed, 1 = a check failed.
 */
#include <stdio.h>
#include <string.h>

/* dyn75.h spells these with trigraphs, which gnu99 does not translate */
#define BOR |
#define OR  ||
#include "../../src/net/dyn75/@@75sock.c"

int *__errno(void) { static int e; return &e; }

static CLIBGRT  grt;
CLIBGRT *__grtget(void) { return &grt; }
int __75init(void) { return 0; }
int __soadd(int ss, void *name, void *peer)
{
    (void)ss; (void)name; (void)peer;
    return 0;
}

static int      general;                    /* what GETERROR answers      */

int __75(PL75 *pl)
{
    unsigned fn = pl->r7 & 0xFFFF;

    if (fn == 5) pl->r4 = (unsigned)-1;     /* socket fails               */
    if (fn == 3) pl->r4 = (unsigned)general;
    return 0;
}

static int run = 0, failed = 0;

static void check(int general_word, int expect)
{
    int s;

    general = general_word;
    errno = -1;
    s = socket(AF_INET, SOCK_STREAM, 0);
    run++;
    if (s != -1 || errno != expect) {
        failed++;
        printf("FAIL: word %d gave socket %d errno %d, expected -1 errno %d\n",
               general_word, s, errno, expect);
    }
}

int main(void)
{
    grt.grtflag1 = GRTFLAG1_SOCKINIT;

    /* what socket() itself can report: kept */
    check(EMFILE, EMFILE);
    check(ENFILE, ENFILE);
    check(ENOBUFS, ENOBUFS);
    check(ENOMEM, ENOMEM);
    check(EACCES, EACCES);
    check(EAFNOSUPPORT, EAFNOSUPPORT);
    check(EPROTONOSUPPORT, EPROTONOSUPPORT);
    check(EPROTOTYPE, EPROTOTYPE);
    check(EINVAL, EINVAL);

    /* a lookup's code, an unset word, anything else: ENOBUFS */
    check(ECONNREFUSED, ENOBUFS);
    check(0, ENOBUFS);
    check(65, ENOBUFS);
    check(-1, ENOBUFS);

    printf("tst75sok: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
