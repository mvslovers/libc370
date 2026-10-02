#ifndef NETDB_H
#define NETDB_H
#include <sys/_cc370.h>
/* netdb.h - POSIX <netdb.h>: host lookup.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

struct hostent {
    char    *h_name;
    char    **h_aliases;
    int     h_addrtype;
    int     h_length;
    char    **h_addr_list;
};

struct hostent * gethostbyname(const char *name)                                asm("@@75GHBN");

struct hostent * gethostbyaddr(void *addr)                                      asm("@@75GHBA");

#endif /* NETDB_H */
