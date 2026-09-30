#ifndef ARPA_INET_H
#define ARPA_INET_H
/* arpa/inet.h - POSIX <arpa/inet.h>: address conversion.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

#include <netinet/in.h>

int inet_aton(const char *cp, in_addr_t *inp)	asm("@@INATON");

#endif /* ARPA_INET_H */
