#ifndef MVS_SOCKET_H
#define MVS_SOCKET_H
#include <sys/_cc370.h>
/* mvs/socket.h - libc370's non-POSIX socket calls and socket table,
** plus every POSIX socket header, for code written against 1.x socket.h.
**
** libc370 2.0 splits this out of socket.h (#256).
*/

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifndef FIONBIO
#define FIONBIO  1 /* set/clear non-blocking i/o */
#endif

#ifndef FIONREAD
#define FIONREAD 2 /* get # bytes to read */
#endif

struct clientid {
    int     domain;
    char    name[8];
    char    subtaskname[8];
    char    reserved[20];
};

unsigned getaddrbyname(const char *name)                                        asm("@@75GABN");

int closesocket(int ss)                                                         asm("@@75CLOS");

int ioctlsocket(int ss, int cmd, void *argp)                                    asm("@@75IOCT");

int selectex(int, fd_set *, fd_set *, fd_set *, timeval *, unsigned **)         asm("@@75SELX");

/* ---- from 1.x clibsock.h ------------------------------------------------ */
typedef struct clibsock CLIBSOCK;

/* the CLIBSOCK structure is used to hold information about open
** sockets created by the DYN75 interface.
**
** the name and peer fields represent sockaddr_in structures.
*/
struct clibsock {
    char    eye[8];                 /* 00 eye catcher "CLIBSOCK"        */
#define CLIBSOCK_EYE    "CLIBSOCK"  /* ...                              */
    int     socket;                 /* 08 socket number                 */
    int     unused;                 /* 0C unused                        */
    char    name[16];               /* 10 sockaddr_in for local side    */
    char    peer[16];               /* 20 sockaddr_in for remote side   */
};                                  /* 30 (48 bytes)                    */

extern int  __soadd(int ss, void *name, void *peer);
extern int  __soupd(int ss, void *name, void *peer);
extern int  __sodel(int ss);

/* __sofind() - returns index of found socket or 0 if not found */
extern int  __sofind(int ss, CLIBSOCK **s);

/* __sosnam() - get socket name (local side of socket) */
extern int  __sosnam(int ss, void *name);

/* __sopnam() - get peer name (remote side of socket) */
extern int  __sopnam(int ss, void *peer);

#endif /* MVS_SOCKET_H */
