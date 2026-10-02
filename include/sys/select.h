#ifndef SYS_SELECT_H
#define SYS_SELECT_H
#include <sys/_cc370.h>
/* sys/select.h - POSIX <sys/select.h>: fd_set and select().
**
** libc370 2.0 splits this out of socket.h (#256).
*/

typedef long fd_mask;

#define NBBY       8
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif
#define NFDBITS (sizeof(fd_mask) * NBBY) /* bits per mask */
#define howmany(x, y) (((x) + ((y) - 1)) / (y))

typedef struct fd_set {
    fd_mask    fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define FD_SET(n, p) \
    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p) \
    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p) \
    ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))

#define	FD_ZERO(p) \
    __asm__("XC\t0(0,%1),0(%1)      *** executed ***\n\t" \
    "EX\t%0,*-6            clear memory" \
    : : "r" (sizeof(*(p))), "r" ((p)))

typedef struct timeval
{
    long    tv_sec;
    long    tv_usec;
} timeval;

int select(int msock, fd_set *r, fd_set *w, fd_set *e, timeval * t)             asm("@@75SELE");

#endif /* SYS_SELECT_H */
