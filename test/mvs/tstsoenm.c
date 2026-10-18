/*
 * tstsoenm.c - libc370 #601 on MVS: a program's sockets are closed when
 * it ends.  The emulator's socket table is global, so a socket a program
 * leaves open stayed bound after it.
 *
 *   CASE=1  open a listening socket on PORT and end without closesocket()
 *   CASE=2  bind PORT: works once the socket of CASE=1 was closed
 *   CASE=3  LINK (under try(), __linkt) the module MOD, which opens a
 *           listening socket on PORT and abends S0C1 (CASE=4); then bind
 *           PORT here: works once the dead module's socket was closed
 *   CASE=5  guard: create a socket, LINK the module MOD (CASE=6), which
 *           binds that socket - the caller's - to PORT and returns; the
 *           caller's socket must still be open: listen() works, and a
 *           second socket cannot bind PORT
 *
 * Every program is built twice, against this tree and against the
 * installed library, with ports of their own (the red ones stay bound).
 *
 * BUILD: see jcl/tstsoenm.jcl for the module names and -D values; all
 * with -O1 -Wall -Werror, the green ones with -Iinclude -L build/sdk.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <mvs/socket.h>
#include <mvs/wto.h>
#include <mvs/link.h>

#ifndef CASE
#define CASE 1
#endif
#ifndef PORT
#define PORT 24601
#endif

/* the module CASE=3 and CASE=5 LINK: the green or the red build of it */
#ifdef RED
#define SUF "R"
#else
#define SUF "G"
#endif
#if CASE == 3
#define MOD "TSTSOD" SUF
#else
#define MOD "TSTSOF" SUF
#endif

static int bind_to(int s, int port)
{
    struct sockaddr_in sin;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = (unsigned short)port;
    return bind(s, &sin, sizeof(sin));
}

__attribute__((unused))
static int verdict(int ok, const char *what)
{
    wtof("TSTSOENM %s: %s (port %d)", ok ? "PASS" : "FAIL", what, PORT);
    return ok ? 0 : 8;
}

/* LINK MOD under try() with the PARM text */
__attribute__((unused))
static int linkmod(const char *text)
{
    struct { unsigned short len; char txt[16]; } parm;
    unsigned    plist;
    int         prc = 0;

    parm.len = (unsigned short)strlen(text);
    memcpy(parm.txt, text, parm.len);
    plist = (unsigned)&parm | 0x80000000;
    return __linkt(MOD, NULL, &plist, &prc);
}

int main(int argc, char **argv)
{
#if CASE == 1
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int rc = bind_to(s, PORT);

    listen(s, 1);
    wtof("TSTSOENM socket %d bound to %d (rc %d), ending without close",
         s, PORT, rc);
    return rc == 0 ? 0 : 8;
#elif CASE == 2
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int rc = bind_to(s, PORT);

    closesocket(s);
    return verdict(rc == 0, "2 the port of a program that ended is free");
#elif CASE == 3
    int abend = linkmod("X");
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int rc;

    wtof("TSTSOENM %s ended with %08X", MOD, abend);
    rc = bind_to(s, PORT);
    closesocket(s);
    return verdict(abend != 0 && rc == 0,
                   "3 the port of a module that abended under try() is free");
#elif CASE == 4
    int s = socket(AF_INET, SOCK_STREAM, 0);

    bind_to(s, PORT);
    listen(s, 1);
    __asm__("DC\tH'0'          abend S0C1");
    return 0;
#elif CASE == 5
    int  s = socket(AF_INET, SOCK_STREAM, 0);
    int  s2, rc1, rc2;
    char num[12];
    int  n = s, i = 0, j;
    char tmp[12];

    do { tmp[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    for (j = 0; j < i; j++) num[j] = tmp[i - 1 - j];
    num[i] = 0;
    linkmod(num);
    rc1 = listen(s, 1);
    s2 = socket(AF_INET, SOCK_STREAM, 0);
    rc2 = bind_to(s2, PORT);
    wtof("TSTSOENM caller socket %d: listen rc %d, second bind rc %d",
         s, rc1, rc2);
    closesocket(s2);
    closesocket(s);
    return verdict(rc1 == 0 && rc2 != 0,
                   "5 a socket the LINKed module bound stays the caller's");
#elif CASE == 6
    int s = argc > 1 ? atoi(argv[1]) : -1;
    int rc = bind_to(s, PORT);

    wtof("TSTSOENM module bound the caller's socket %d: rc %d", s, rc);
    return 0;
#endif
}
