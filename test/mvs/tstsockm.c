/*
 * tstsockm.c - libc370 #396 on MVS: the socket layer.
 *
 *   1 FD_ZERO clears the fd_set and not the byte behind it (it cleared
 *     129 bytes)
 *   2 select() on a listening socket nobody connects to, timeout 0.1 s,
 *     returns 0 within 2 seconds (it waited about 125 s)
 *   3 ioctlsocket(FIONBIO) keeps the caller's value (it stored 0 over it)
 *     and returns -1 for a socket that is not one (it returned 0);
 *     non-blocking accept() still gives -1 at once
 *   4 bind() to port 0 leaves rand()'s sequence alone (it reseeded it)
 *   5 gethostbyaddr() writes nothing to stdout - read the step's
 *     SYSPRINT, which the old one filled with a __75ghba() line
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstsockm.c \
 *         -o $S/TSTSOCKG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror test/mvs/tstsockm.c \
 *         -o $S/TSTSOCKR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTSOCKG=TSTSOCKG.iebcopy \
 *         TSTSOCKR=TSTSOCKR.iebcopy -o so -xmit --dsn IBMUSER.LIBC370.SOSCR
 *
 * then jcl/recvsockm.jcl and jcl/tstsockm.jcl.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <mvs/socket.h>
#include <mvs/wto.h>

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTSOCKM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

int main(void)
{
    struct { fd_set set; unsigned char guard[4]; } z;
    struct sockaddr_in  sin;
    timeval             tv;
    fd_set              r;
    time_t              t0, t1;
    unsigned            v, addr;
    int                 s, rc, i, r1, r2, ok;

    /* 1 FD_ZERO */
    memset(&z, 0x55, sizeof(z));
    FD_ZERO(&z.set);
    ok = z.guard[0] == 0x55;
    for (i = 0; i < (int)sizeof(z.set); i++)
        if (((unsigned char *)&z.set)[i] != 0) ok = 0;
    want(ok, "1 FD_ZERO clears the set and not the byte behind it");

    /* a listening socket on a port of the system's choice */
    s = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = 0;

    /* 4 bind() to port 0 and rand() */
    srand(4711);
    r1 = rand();
    srand(4711);
    rc = bind(s, &sin, sizeof(sin));
    r2 = rand();
    wtof("TSTSOCKM socket %d, bind rc %d, port %d", s, rc, sin.sin_port);
    want(rc == 0 && r1 == r2, "4 bind() to port 0 leaves rand() alone");
    rc = listen(s, 1);

    /* 2 select() with a timeout of 0.1 s */
    FD_ZERO(&r);
    FD_SET(s, &r);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    time(&t0);
    rc = select(s + 1, &r, NULL, NULL, &tv);
    time(&t1);
    wtof("TSTSOCKM select rc %d after %d s", rc, (int)(t1 - t0));
    want(rc == 0 && t1 - t0 <= 2, "2 select() 0.1 s returns within 2 s");

    /* 3 FIONBIO */
    v = 1;
    rc = ioctlsocket(s, FIONBIO, &v);
    want(rc == 0 && v == 1, "3 FIONBIO keeps the caller's value");
    rc = accept(s, NULL, NULL);
    want(rc == -1, "3 non-blocking accept() returns -1 at once");
    v = 1;
    rc = ioctlsocket(999, FIONBIO, &v);
    want(rc == -1, "3 FIONBIO on a socket that is not one returns -1");

    /* 5 gethostbyaddr(): nothing on stdout */
    addr = 0x7F000001;
    gethostbyaddr(&addr);
    want(1, "5 gethostbyaddr() came back - SYSPRINT must be empty");

    closesocket(s);
    wtof("TSTSOCKM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
