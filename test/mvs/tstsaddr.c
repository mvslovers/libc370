/*
 * tstsaddr.c - libc370 #417: bind(), connect(), accept(), getsockname()
 * and getpeername() take a struct sockaddr_in * and a struct sockaddr *
 * alike.
 *
 * The check is the compile: every function is called once with the
 * address of a struct sockaddr_in as it is and once with the POSIX cast
 * (struct sockaddr *), and connect() also with a pointer to const.
 * Compiled with -Wall -Werror, a declaration that accepts only one of the
 * forms fails the build ("passing arg 2 of 'bind' from incompatible
 * pointer type").  Before #417, bind/connect/accept rejected the cast and
 * getsockname/getpeername rejected the uncast form.
 *
 * Build:   make build
 *          cc370 -S -Wall -Werror -Iinclude test/mvs/tstsaddr.c \
 *                -o tstsaddr.s
 *
 * The program never runs a call: the socket number is -1 unless a
 * caller passes an argument, so it can also be linked and run without a
 * network.  RC 0.
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stddef.h>

static int
both_forms(int s)
{
    struct sockaddr_in       sin;
    const struct sockaddr_in *csin = &sin;
    int                      len = sizeof(sin);
    int                      rc = 0;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    /* as libc370 programs have called them */
    rc |= bind(s, &sin, sizeof(sin));
    rc |= connect(s, &sin, sizeof(sin));
    rc |= accept(s, &sin, &len);
    rc |= getsockname(s, &sin, &len);
    rc |= getpeername(s, &sin, &len);

    /* as POSIX code calls them */
    rc |= bind(s, (struct sockaddr *)&sin, sizeof(sin));
    rc |= connect(s, (struct sockaddr *)&sin, sizeof(sin));
    rc |= accept(s, (struct sockaddr *)&sin, &len);
    rc |= getsockname(s, (struct sockaddr *)&sin, &len);
    rc |= getpeername(s, (struct sockaddr *)&sin, &len);

    /* connect() does not write the address */
    rc |= connect(s, csin, sizeof(sin));
    rc |= connect(s, (const struct sockaddr *)csin, sizeof(sin));

    /* the null pointer and a void * */
    rc |= accept(s, NULL, NULL);
    rc |= getsockname(s, 0, &len);
    rc |= getpeername(s, NULL, &len);
    rc |= connect(s, (void *)&sin, sizeof(sin));
    rc |= bind(s, (void *)&sin, sizeof(sin));

    return rc;
}

int
main(int argc, char **argv)
{
    if (argc > 1)
        return both_forms(-1) == 0 ? 1 : 0;
    return 0;
}
