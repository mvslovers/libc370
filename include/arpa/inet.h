#ifndef ARPA_INET_H
#define ARPA_INET_H
#include <sys/_cc370.h>
/* arpa/inet.h - POSIX <arpa/inet.h>: address conversion.
**
** libc370 2.0 splits this out of socket.h (#256) and brings it to POSIX and
** BSD (#51): in_addr_t is an integer, inet_aton() returns 1 for an address,
** and inet_addr(), inet_ntoa(), inet_pton(), inet_ntop() are new.  None of
** them uses scanf or printf, so they cost a load module a few hundred bytes.
*/

#include <netinet/in.h>
#include <sys/socket.h>                 /* socklen_t, AF_INET */

/* inet_aton() - an address in one of the forms "a.b.c.d", "a.b.c", "a.b",
**               "a" -- each part decimal, octal (0...) or hex (0x...) -- into
**               *inp, which may be NULL to only check.  Returns 1 when cp is
**               an address, 0 when it is not (BSD; 1.x returned 0 and -1). */
int inet_aton(const char *cp, struct in_addr *inp)          asm("@@INATON");

/* inet_addr() - the same forms, as an address in network byte order;
**               INADDR_NONE when cp is not one.  "255.255.255.255" is
**               INADDR_NONE as well: inet_aton() tells the two apart. */
in_addr_t inet_addr(const char *cp)                         asm("@@INADDR");

/* inet_pton() - AF_INET only: exactly "a.b.c.d", four decimal parts of 0 to
**               255 without leading zeros, into *(struct in_addr *)dst.
**               1 done, 0 not an address, -1 with errno EAFNOSUPPORT. */
int inet_pton(int af, const char *src, void *dst)           asm("@@INPTON");

/* inet_ntop() - AF_INET only: *(const struct in_addr *)src as "a.b.c.d" into
**               dst of size bytes (INET_ADDRSTRLEN is always enough).
**               dst, or NULL with errno EAFNOSUPPORT or ENOSPC. */
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size)
                                                            asm("@@INNTOP");

/* inet_ntoa() - in as "a.b.c.d", in one buffer per process that the next
**               call overwrites: not for concurrent threads, where
**               inet_ntop() is the form to use.  NULL when the runtime has
**               no process anchor (__wsaget()). */
char *inet_ntoa(struct in_addr in)                          asm("@@INNTOA");

#endif /* ARPA_INET_H */
