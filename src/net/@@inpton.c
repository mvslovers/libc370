/* @@INPTON.C - inet_pton(): AF_INET only (#51)
**
** POSIX's strict form: exactly four decimal parts of 0 to 255, no leading
** zeros, nothing after the last.  dst is written only for an address.
*/
#include <errno.h>
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst)
{
    in_addr_t   addr    = 0;
    unsigned    v;
    int         n, len;

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    for (n = 0; n < 4; n++) {
        if (n) {
            if (*src != '.')
                return 0;
            src++;
        }
        for (v = 0, len = 0; *src >= '0' && *src <= '9'; src++, len++) {
            if (len && v == 0)
                return 0;               /* a leading zero */
            v = v * 10 + (unsigned) (*src - '0');
            if (v > 255)
                return 0;
        }
        if (!len)
            return 0;
        addr = (addr << 8) | v;
    }
    if (*src)
        return 0;
    ((struct in_addr *) dst)->s_addr = addr;
    return 1;
}
