#ifndef SRC_INTERNAL_DYN75_H
#define SRC_INTERNAL_DYN75_H
/* src/internal/dyn75.h - internal: the DYN75 (X'75') socket implementation.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#ifndef OR
#define OR ??!??!
#endif

#ifndef BOR
#define BOR ??!
#endif

typedef struct __75vect     __75VECT;
extern __75VECT *__75vect;  /* global pointer to vector     */

unsigned __75gabn(const char *name);

struct hostent * __75ghbn(const char *name);

int __75sock(int af, int type, int protocol);

int __75bind(int ss, struct sockaddr_in * name, int length);

int __75conn(int ss, struct sockaddr_in *name, int length);

int __75list(int ss, int backlog);

int __75acce(int ss, struct sockaddr_in *name, int *length);

int __75send(int ss, const void *buf, int len, int flags);

int __75recv(int ss, void *buf, int len, int flags);

int __75clos(int ss);

int __75ioct(int ss, int cmd, void *argp);

int __75snam(int ss, struct sockaddr *addr, int *addrlen);

int __75sele(int maxsock, fd_set *r, fd_set *w, fd_set *e, timeval * t);

int __75selx(int, fd_set *, fd_set *, fd_set *, timeval *, unsigned **);

int __75pnam(int ss, struct sockaddr *addr, int *addrlen);

struct hostent * __75ghba(void *addr);

struct __75vect {
    int     inited;         /* 00 API initialized flag (deprecated)     */
    int     error;          /* 04 last error code           */
    fd_set  *inuse;         /* 08 sockets in use       (deprecated)     */
    unsigned (*getaddrbyname)(const char *name);
                            /* 0C get IP address by name    */

    struct hostent * (*gethostbyname)(const char *name);
                            /* 10 get host by name          */
    int     (*socket)(int af, int type, int protocol);
                            /* 14 allocate a socket         */
    int     (*bind)(int ss, struct sockaddr_in *add, int length);
                            /* 18 bind socket to name       */
    int     (*connect)(int ss, struct sockaddr_in *addr, int length);
                            /* 1C connect socket to name    */

    int     (*listen)(int ss, int backlog);
                            /* 20 listen for connection     */
    int     (*accept)(int ss, struct sockaddr_in *addr, int *length);
                            /* 24 accept socket connection  */
    int     (*send)(int ss, const void *buf, int len, int flags);
                            /* 28 send data to socket       */
    int     (*recv)(int ss, void *buf, int len, int flags);
                            /* 2C recv data from socket     */

    int     (*closesocket)(int ss);
                            /* 30 close socket              */
    int     (*ioctlsocket)(int ss, int cmd, void *argp);
                            /* 34 ioctl for socket          */
    int     (*getsockname)(int ss, struct sockaddr *addr, int *addrlen);
                            /* 38 getsockname()             */
    int     (*select)(int msock, fd_set *r, fd_set *w, fd_set *e, timeval * t);
                            /* 3C select()                  */
    int     (*selectex)(int, fd_set *, fd_set *, fd_set *, timeval *, unsigned **);
                            /* 40 selectex()                */
    int     (*getpeername)(int ss, struct sockaddr *addr, int *addrlen);
                            /* 44 getpeername()             */
    struct hostent * (*gethostbyaddr)(void *addr);
                            /* 48 gethostbyaddr()           */
};

extern int __75init(void);  /* initialize API */

#if 0
#ifndef __75_H
/* public calls via __75vect vector */
#define getaddrbyname(name) \
    ((__75vect->getaddrbyname)(name))

#define gethostbyname(name) \
    ((__75vect->gethostbyname)(name))

#define socket(af,type,protocol) \
    ((__75vect->socket)((af),(type),(protocol)))

#define bind(ss,name,length) \
    ((__75vect->bind)((ss),(name),(length)))

#define connect(ss,name,length) \
    ((__75vect->connect)((ss),(name),(length)))

#define listen(ss,backlog) \
    ((__75vect->listen)((ss),(backlog)))

#define accept(ss,name,length) \
    ((__75vect->accept)((ss),(name),(length)))

#define send(ss,buf,len,flags) \
    ((__75vect->send)((ss),(buf),(len),(flags)))

#define recv(ss,buf,len,flags) \
    ((__75vect->recv)((ss),(buf),(len),(flags)))

#define closesocket(ss) \
    ((__75vect->closesocket)((ss)))

#define ioctlsocket(ss,cmd,argp) \
    ((__75vect->ioctlsocket)((ss),(cmd),(argp)))

#define getsockname(ss,addr,addrlen) \
    ((__75vect->getsockname)((ss),(addr),(addrlen)))

#define select(maxsock,r,w,e,t) \
    ((__75vect->select)((maxsock),(r),(w),(e),(t)))

#define selectex(maxsock,r,w,e,t,ecblist) \
    ((__75vect->selectex)((maxsock),(r),(w),(e),(t),(ecblist)))

#define getpeername(ss,addr,addrlen) \
    ((__75vect->getpeername)((ss),(addr),(addrlen)))

#define gethostbyaddr(addr) \
    ((__75vect->gethostbyaddr)(addr))

#endif  /* ifndef __75_H */
#endif  /* ifdef 0 */

#endif /* SRC_INTERNAL_DYN75_H */
