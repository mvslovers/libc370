#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H
/* sys/socket.h - POSIX <sys/socket.h>: sockets, over the DYN75 interface.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

#include <netinet/in.h>

/* socklen_t - a length of socket data (POSIX: at least 32 bits) */
typedef unsigned int    socklen_t;

struct sockaddr {
    unsigned short    sa_family;    /* address family, AF_xxx */
    char              sa_data[14];  /* 14 bytes of protocol address */
};

struct linger {
    int     onoff;
    int     linger;
};

/* Supported address families. */
#define AF_UNSPEC       0
#define AF_UNIX         1   /* Unix domain sockets          */
#define AF_INET         2   /* Internet IP Protocol         */
#define AF_AX25         3   /* Amateur Radio AX.25          */
#define AF_IPX          4   /* Novell IPX                   */
#define AF_APPLETALK    5   /* Appletalk DDP                */
#define AF_NETROM       6   /* Amateur radio NetROM         */
#define AF_BRIDGE       7   /* Multiprotocol bridge         */
#define AF_AAL5         8   /* Reserved for Werner's ATM    */
#define AF_X25          9   /* Reserved for X.25 project    */
#define AF_INET6        10  /* IP version 6                 */
#define AF_MAX          12  /* For now..                    */

#define SOCK_STREAM     1   /* virtual circuit              */
#define SOCK_DGRAM      2   /* datagram                     */
#define SOCK_RAW        3   /* raw socket                   */
#define SOCK_RDM        4   /* reliably-delivered message   */
#define SOCK_SEQPACKET  5   /* sequenced packet stream      */

#define SOMAXCONN 256

int socket(int af, int type, int protocol)                                      asm("@@75SOCK");

int bind(int ss, struct sockaddr_in *add, int length)                           asm("@@75BIND");

int connect(int ss, struct sockaddr_in *addr, int length)                       asm("@@75CONN");

int listen(int ss, int backlog)                                                 asm("@@75LIST");

int accept(int ss, struct sockaddr_in *addr, int *length)                       asm("@@75ACCE");

int send(int ss, const void *buf, int len, int flags)                           asm("@@75SEND");

int recv(int ss, void *buf, int len, int flags)                                 asm("@@75RECV");

int getsockname(int ss, struct sockaddr *addr, int *addrlen)                    asm("@@75SNAM");

int getpeername(int ss, struct sockaddr *addr, int *addrlen)                    asm("@@75PNAM");

#endif /* SYS_SOCKET_H */
