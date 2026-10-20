/*
 * tst75sel.c - libc370 #396: select()'s timeout and ioctlsocket(FIONBIO);
 * #161: the errno of a failed select().
 *
 * #includes the real @@75selx.c and @@75ioct.c; __75(), the one function
 * that reaches the X'75' instruction, is a fake.  For select it answers
 * "nothing ready" (-2) to every poll, so the number of polls is the number
 * of 0.08-second waits plus one.  For ioctl it records the value register
 * and fails for socket 99, as the emulator does for a socket that is not
 * one (hyperion tcpip.c, case 15 / check_not_sock).
 *
 * #161: a select run the emulator fails (poll answers -1) means a socket in
 * it is gone.  The socket number's error word (GETERRORS, code 2) is then
 * not this call's - it may be the word of whoever got the number next - so
 * select() sets ENOTSOCK without reading it.  The fake answers GETERRORS
 * with 9, so a select() that still reads the word reports errno 9.  Every
 * call after the failed poll must still be a select call (code 17), or the
 * Finish subcode never reaches the emulator and the run's state stays.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address -Wno-trigraphs \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tst75sel.c && ./t
 *
 * RED, 2026-10-18, against main: 0.5 s gave 7812 waits (625 s), and
 * 0.1, 1 and 2.5 s were wrong too; FIONBIO passed the pointer (r9 nonzero
 * for &zero), wrote 0 over the caller's value, and returned 0 for socket 99.
 *
 * RED, 2026-10-20, against main (#161): a failed select() gave errno 9 (the
 * error word), read it once, and sent the four calls after it as code 2,
 * so Finish was never issued.
 *
 * RC: 0 = every check passed, 1 = a check failed.
 */
#include <stdio.h>
#include <string.h>

/* dyn75.h spells these with trigraphs, which gnu99 does not translate */
#define BOR |
#define OR  ||
#include "../../src/net/dyn75/@@75selx.c"
#include "../../src/net/dyn75/@@75ioct.c"

int *__errno(void) { static int e; return &e; }
unsigned arraycount(void *varray) { (void)varray; return 0; }

static int      polls;
static unsigned last_r9;
static int      poll_fails;                 /* the poll answers -1        */
static int      errfetch;                   /* GETERRORS calls            */
static int      finished;                   /* select Finish (subcode 8)  */

int __75(PL75 *pl)
{
    unsigned fn = pl->r7 & 0xFFFF;

    if (fn == 17) {                         /* select */
        if (pl->r8 == 4) {
            polls++;
            pl->r4 = poll_fails ? (unsigned)-1 : (unsigned)-2;
        }
        if (pl->r8 == 8) finished++;
        return 0;
    }
    if (fn == 15) {                         /* ioctl */
        last_r9 = pl->r9;
        pl->r4 = ((pl->r7 >> 16) == 99) ? (unsigned)-1 : 0;
        return 0;
    }
    if (fn == 2) {                          /* get error */
        errfetch++;
        pl->r4 = 9;
        return 0;
    }
    return 0;
}

static int run = 0, failed = 0;

static void check(int cond, const char *what)
{
    run++;
    if (!cond) {
        failed++;
        printf("FAIL: %s\n", what);
    }
}

static int waits(long sec, long usec)
{
    timeval t;
    fd_set  r;

    t.tv_sec = sec;
    t.tv_usec = usec;
    r.fds_bits[0] = 2;
    polls = 0;
    selectex(5, &r, NULL, NULL, &t, NULL);
    return polls - 1;
}

int main(void)
{
    unsigned v;
    int      n;

    /* select(): 0.08-second waits, rounded up */
    n = waits(0, 500000);
    printf("0.5 s: %d waits\n", n);
    check(n == 7, "0.5 s is 7 waits (0.56 s)");
    check(waits(0, 100000) == 2, "0.1 s is 2 waits");
    check(waits(1, 0) == 13, "1 s is 13 waits");
    check(waits(0, 1) == 1, "1 usec is 1 wait");
    check(waits(0, 0) == 0, "0 is a poll");
    check(waits(2, 500000) == 32, "2.5 s is 32 waits");

    /* select() failed by the emulator (#161) */
    {
        timeval t;
        fd_set  r;

        t.tv_sec = 1;
        t.tv_usec = 0;
        r.fds_bits[0] = 2;
        poll_fails = 1;
        errfetch = 0;
        finished = 0;
        errno = 0;
        n = selectex(5, &r, NULL, NULL, &t, NULL);
        poll_fails = 0;
        printf("failed select: rc %d errno %d, %d error reads, %d finish\n",
               n, errno, errfetch, finished);
        check(n == -1, "failed select returns -1");
        check(errno == ENOTSOCK, "failed select sets ENOTSOCK");
        check(errfetch == 0, "failed select does not read the error word");
        check(finished == 1, "failed select still issues Finish");
    }

    /* ioctlsocket(FIONBIO) */
    v = 0;
    check(ioctlsocket(3, FIONBIO, &v) == 0 && last_r9 == 0,
          "FIONBIO &0: blocking, value 0 passed");
    v = 1;
    check(ioctlsocket(3, FIONBIO, &v) == 0 && last_r9 == 1 && v == 1,
          "FIONBIO &1: non-blocking, the caller's value kept");
    check(ioctlsocket(3, FIONBIO, NULL) == 0 && last_r9 == 0,
          "FIONBIO NULL: blocking");
    v = 1;
    check(ioctlsocket(99, FIONBIO, &v) == -1 && errno == 9 && v == 1,
          "FIONBIO on a non-socket: -1, errno set, value kept");

    printf("tst75sel: %d checks, %d failed\n", run, failed);
    return failed ? 1 : 0;
}
