/* @@INADDR.C - inet_addr(): inet_aton() as a value (#51) */
#include <arpa/inet.h>

in_addr_t inet_addr(const char *cp)
{
    struct in_addr  a;

    return inet_aton(cp, &a) ? a.s_addr : INADDR_NONE;
}
