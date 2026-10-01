/* @@INNTOP.C - inet_ntop(): AF_INET only (#51)
**
** Formats the four bytes itself instead of through sprintf(), which would
** bring the whole printf engine into the caller's load module.
*/
#include <errno.h>
#include <arpa/inet.h>

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    char        tmp[INET_ADDRSTRLEN];
    char        *p      = tmp;
    in_addr_t   a;
    unsigned    b;
    int         shift;
    socklen_t   i;

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return 0;
    }
    a = ((const struct in_addr *) src)->s_addr;
    for (shift = 24; shift >= 0; shift -= 8) {
        b = (unsigned) (a >> shift) & 0xFF;
        if (b >= 100) *p++ = (char) ('0' + b / 100);
        if (b >= 10)  *p++ = (char) ('0' + b / 10 % 10);
        *p++ = (char) ('0' + b % 10);
        *p++ = shift ? '.' : '\0';
    }
    if ((socklen_t) (p - tmp) > size) {
        errno = ENOSPC;
        return 0;
    }
    for (i = 0; i < (socklen_t) (p - tmp); i++)
        dst[i] = tmp[i];
    return dst;
}
