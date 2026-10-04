#import "../bookmaster/bookmaster.typ": *

= Sockets <sockets>

#idx("sockets")
#idx("TCP/IP")
The socket functions let a program open TCP connections, accept them and
exchange data with other hosts, with the calls a C programmer knows from
BSD and POSIX systems. They are declared in six headers:

#tab(caption: [Socket headers])[
  #table(columns: (1.35in, 1fr),
    [Header], [Provides],
    [#cmd("<sys/socket.h>")], [#cmd("socket()"), #cmd("bind()"), #cmd("listen()"),
      #cmd("accept()"), #cmd("connect()"), #cmd("send()"), #cmd("recv()"),
      #cmd("getsockname()"), #cmd("getpeername()")\; #cmd("struct sockaddr"),
      #cmd("socklen_t"), the #cmd("AF_") and #cmd("SOCK_") constants],
    [#cmd("<sys/select.h>")], [#cmd("select()"), #cmd("fd_set") and the
      #cmd("FD_") macros, #cmd("struct timeval")],
    [#cmd("<netinet/in.h>")], [#cmd("struct sockaddr_in"),
      #cmd("struct in_addr"), #cmd("in_addr_t"), the #cmd("IPPROTO_")
      constants, #cmd("INADDR_ANY"), #cmd("INADDR_NONE"), the byte-order
      macros],
    [#cmd("<arpa/inet.h>")], [#cmd("inet_aton()"), #cmd("inet_addr()"),
      #cmd("inet_pton()"), #cmd("inet_ntop()"), #cmd("inet_ntoa()")],
    [#cmd("<netdb.h>")], [#cmd("gethostbyname()"), #cmd("gethostbyaddr()"),
      #cmd("struct hostent")],
    [#cmd("<mvs/socket.h>")], [the calls that are not POSIX:
      #cmd("closesocket()"), #cmd("ioctlsocket()"), #cmd("selectex()"),
      #cmd("getaddrbyname()")\; #cmd("FIONBIO") and #cmd("FIONREAD")\; and
      the five headers above, for programs written against older versions of
      the library],
  )
] <sockets-headers-tab>

Every program that uses sockets calls #cmd("closesocket()"), so every such
program includes #cmd("<mvs/socket.h>").

== How the Calls Reach the Network <sockets-provider>

#idx("X'75' instruction")
#idx("Hercules", "TCPIP instruction")
MVS 3.8j has no TCP/IP of its own, and the library does not supply one. The
socket functions use the TCP/IP stack of the workstation that runs the
Hercules emulator. Each call is passed to the emulator through the TCPIP
instruction, operation code X'75', which exists only in Hercules: the
emulator performs the corresponding socket call on the host and returns the
result to the program. No TCP/IP software has to be installed on MVS, and no
started task has to be running.

The instruction is part of Hercules, and it is switched off unless the
emulator is told otherwise. The Hercules documentation describes three
conditions, all of which must be met:

- The emulator is built with the TCP/IP extension for the S/370
  architecture. The SDL Hyperion builds have it.
- The command #cmd("facility enable HERC_TCPIP_EXTENSION") is issued before
  MVS is IPLed. It makes the instruction available in supervisor state.
- The command #cmd("facility enable HERC_TCPIP_PROB_STATE") is issued
  after it, also before the IPL. It makes the instruction available in
  problem state, where C programs run.

Both commands are normally placed in the Hercules configuration file. Where
the instruction is not enabled, it is not an instruction at all, and the
first socket call of a program ends in a program check: an operation
exception (ABEND S0C1), or, with only the first facility enabled, a
privileged-operation exception (ABEND S0C2). The library has no way to test
for the instruction before it uses it.

#note[The TCPIP instruction gives every program that uses it the same
access to the network that the Hercules process has on the host. Socket
numbers are assigned by the emulator from one table that all programs on the
system share, so one program can use, or close, a socket that another
program opened. A server that must be protected from other programs on the
same system cannot rely on the socket layer for that.]

=== Socket Numbers

#idx("socket", "number")
A socket is identified by a small integer, as on other systems, but the
number is not a file descriptor: the functions of #cmd("<stdio.h>") cannot
use it, and there is no #cmd("close") or #cmd("read") for it. Numbers are
assigned from 1 to 1023\; 0 is never a socket. Because the table is shared
by the whole system, the first socket a program opens is not necessarily
number 1.

A socket stays open until #cmd("closesocket()") closes it. The library does
not close a program's sockets when the program ends: a socket left open
keeps its number, and a listening socket keeps its port, until some
program closes that number or Hercules is restarted. Close every socket
before the program ends, including on its error paths.

=== Addresses and Byte Order

#idx("byte order")
System/370 stores integers with the most significant byte first, which is
network byte order. The conversion macros #cmd("htonl()"), #cmd("htons()"),
#cmd("ntohl()") and #cmd("ntohs()") therefore return their argument unchanged,
and the address and port in a #cmd("struct sockaddr_in") are in network
order already. Use the macros anyway, so the program stays portable.

Only IPv4 addresses (#cmd("AF_INET")) are supported. The other #cmd("AF_")
constants in #cmd("<sys/socket.h>") are defined so that programs compile,
but no function can use them.

=== Character Data

#idx("EBCDIC", "socket data")
#idx("ASCII", "socket data")
#cmd("send()") and #cmd("recv()") move bytes, unchanged, in both directions.
The program runs in EBCDIC (code page 037), and nearly every peer on the
network expects ASCII or UTF-8, so text must be translated before it is
sent and after it is received. The library provides no translation function
for this\; the program supplies its own table. Binary data, such as the
32-bit integers of a protocol header, needs no translation.

Host names are the exception: #cmd("gethostbyname()") and
#cmd("getaddrbyname()") take the name in EBCDIC and the emulator translates
it, and #cmd("gethostbyaddr()") returns the name translated to EBCDIC.

=== Blocking

#idx("non-blocking socket")
A socket is created blocking. While a blocking call waits -- for a
connection, for data, or for room in the send buffer -- the library does not
leave the program waiting in the emulator. It repeats the call at short
intervals and waits between attempts with #cmd("STIMER"), so other work on
the system goes on. The interval is 0.08 seconds for #cmd("accept()"),
#cmd("recv()") and #cmd("select()"), 0.1 seconds for #cmd("send()") and one
second for #cmd("connect()"). The entries below say how each call behaves.

#cmd("ioctlsocket()") with #cmd("FIONBIO") makes a socket non-blocking. A call
on a non-blocking socket that cannot complete at once returns -1 and sets
#cmd("errno") to #cmd("EWOULDBLOCK") (for #cmd("connect()"),
#cmd("EINPROGRESS")).

=== Errors

#idx("errno", "socket functions")
A socket function that fails returns -1 and sets #cmd("errno") to a value
from #cmd("<errno.h>") (see @std-errno). Most of the values come from the
host's socket layer, translated by the emulator to the numbers in
#cmd("<errno.h>"): #cmd("ECONNREFUSED"), #cmd("ECONNRESET"),
#cmd("EADDRINUSE"), #cmd("ENOTSOCK"), #cmd("EMFILE") and the others. An
entry lists only the values that the library itself sets, and those that
mean something particular for that call.

=== What Is Not Provided

The following calls of the POSIX socket interface are not in the library:
#cmd("socketpair"), #cmd("shutdown"), #cmd("setsockopt"),
#cmd("getsockopt"), #cmd("sendto"), #cmd("recvfrom"), #cmd("sendmsg"),
#cmd("recvmsg"), #cmd("poll"), #cmd("getaddrinfo"), #cmd("getnameinfo"),
#cmd("gethostbyname_r") and #cmd("h_errno"). #cmd("struct linger") and
#cmd("SOMAXCONN") are declared, but no function takes them. A datagram
socket (#cmd("SOCK_DGRAM")) can be created, but without #cmd("sendto") and
#cmd("recvfrom") it can only be used with #cmd("connect()"), #cmd("send()") and
#cmd("recv()").

Several prototypes differ from POSIX\; the differences are listed in the
Notes of each entry. The one met most often: #cmd("bind()"),
#cmd("connect()") and #cmd("accept()") take a #cmd("struct sockaddr_in *"), not
a #cmd("struct sockaddr *"). The usual cast to #cmd("(struct sockaddr *)")
therefore draws a warning about an incompatible pointer type, which
#cmd("-Werror") turns into an error. Pass the address of the
#cmd("struct sockaddr_in") itself.

=== Types and Constants

#tab(caption: [Socket types on this target])[
  #table(columns: (1.6in, 0.5in, 1fr),
    [Type], [Bytes], [Members],
    [#cmd("struct sockaddr")], [16], [#cmd("sa_family") (#cmd("unsigned short")),
      #cmd("sa_data[14]")],
    [#cmd("struct sockaddr_in")], [16], [#cmd("sin_family") (#cmd("short")),
      #cmd("sin_port") (#cmd("unsigned short")), #cmd("sin_addr"),
      #cmd("sin_zero[8]")],
    [#cmd("struct in_addr")], [4], [#cmd("s_addr")],
    [#cmd("in_addr_t")], [4], [#cmd("unsigned long")],
    [#cmd("socklen_t")], [4], [#cmd("unsigned int")],
    [#cmd("fd_set")], [128], [1024 bits, in 32-bit words],
    [#cmd("struct timeval"), #cmd("timeval")], [8], [#cmd("tv_sec"),
      #cmd("tv_usec") (#cmd("long"))],
    [#cmd("struct hostent")], [20], [#cmd("h_name"), #cmd("h_aliases"),
      #cmd("h_addrtype"), #cmd("h_length"), #cmd("h_addr_list")],
  )
] <sockets-types-tab>

#tab(caption: [Socket constants])[
  #table(columns: (1.6in, 0.9in, 1fr),
    [Name], [Value], [Use],
    [#cmd("AF_INET")], [2], [the only address family supported],
    [#cmd("SOCK_STREAM")], [1], [a TCP socket],
    [#cmd("SOCK_DGRAM")], [2], [a UDP socket (see above)],
    [#cmd("IPPROTO_TCP")], [6], [],
    [#cmd("IPPROTO_UDP")], [17], [],
    [#cmd("INADDR_ANY")], [0], [bind to every local address],
    [#cmd("INADDR_NONE")], [#cmd("0xFFFFFFFF")], [returned by
      #cmd("inet_addr()") for an invalid address],
    [#cmd("INET_ADDRSTRLEN")], [16], [the size of a buffer for an address
      in text form],
    [#cmd("FD_SETSIZE")], [1024], [bits in an #cmd("fd_set")],
    [#cmd("FIONBIO")], [1], [#cmd("ioctlsocket()"): set or clear non-blocking
      mode],
    [#cmd("FIONREAD")], [2], [#cmd("ioctlsocket()"): bytes waiting to be
      read],
  )
] <sockets-const-tab>

=== Example

@sockets-echo-fig is a small server. It accepts one connection on port
7007 and sends back everything it receives, until the client closes the
connection. It makes the connection non-blocking and waits in
#cmd("select()"), so that #cmd("recv()") returns whatever has arrived (see
@sockets-recv). The data is echoed unchanged, so it needs no translation.

#fig(caption: [ECHO, a socket server])[
  #code(read("../ex/sockets/echo.c"), numbers: true)
] <sockets-echo-fig>

== accept <sockets-accept>

#idx("accept")
=== Format

```
#include <sys/socket.h>

int accept(int ss, struct sockaddr_in *addr, int *length);
```

=== Description

#cmd("accept()") takes the next connection waiting on the listening socket
#var("ss") and returns a new socket for it. When #var("addr") is not NULL,
the address of the peer is stored in it. #var("ss") stays open and
listening.

On a blocking socket #cmd("accept()") waits until a connection arrives,
looking every 0.08 seconds. On a non-blocking socket it returns at once.

=== Returns

The number of the new socket, or -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EWOULDBLOCK")], [#var("ss") is non-blocking and no connection is
    waiting.],
  [#cmd("EMFILE")], [the emulator's socket table is full.],
)

=== Notes

- #var("addr") is a #cmd("struct sockaddr_in *"), not a
  #cmd("struct sockaddr *"), and #var("length") an #cmd("int *"), not a
  #cmd("socklen_t *").
- #var("length") is neither read nor changed. The peer address is always 16
  bytes, a #cmd("struct sockaddr_in").
- The new socket is blocking, whatever #var("ss") is.

=== Related

@sockets-listen, @sockets-select, @sockets-getpeername

== bind <sockets-bind>

#idx("bind")
=== Format

```
#include <sys/socket.h>

int bind(int ss, struct sockaddr_in *add, int length);
```

=== Description

#cmd("bind()") gives the socket #var("ss") the local address and port in
#var("add"). Set #cmd("sin_family") to #cmd("AF_INET"), #cmd("sin_port")
to the port and #cmd("sin_addr.s_addr") to a local address of the host, or
to #cmd("INADDR_ANY") for all of them.

When #cmd("sin_port") is 0, the library chooses a port: it tries random
ports from 10000 to 42767 until one can be bound, at most 100 times, and
leaves the port it used in #cmd("add->sin_port").

=== Returns

0, or -1 for an error.

=== Notes

- #var("add") is a #cmd("struct sockaddr_in *") and #var("length") an
  #cmd("int")\; #var("length") is not used.
- The port is a port of the host, not of MVS. It must be free on the host,
  and on most hosts a port below 1024 can be bound only when Hercules runs
  with the privileges for it.
- Choosing a port for #cmd("sin_port") 0 calls #cmd("srand()") with the time
  of day, which restarts the sequence #cmd("rand()") returns to the program.
- If all 100 attempts fail, #cmd("sin_port") is set back to 0 and the
  request is passed to the host unchanged.

=== Related

@sockets-listen, @sockets-getpeername

== closesocket <sockets-closesocket>

#idx("closesocket")
=== Format

```
#include <mvs/socket.h>

int closesocket(int ss);
```

=== Description

#cmd("closesocket()") closes the socket #var("ss") and frees its number. A
connection on it is closed. It takes the place of the #cmd("close") call of
POSIX systems, which does not exist for sockets here.

=== Returns

0, always.

=== Notes

- Closing a number that is not open is not reported.
- Sockets are not closed when a program ends\; see @sockets-provider.

=== Related

@sockets-socket

== connect <sockets-connect>

#idx("connect")
=== Format

```
#include <sys/socket.h>

int connect(int ss, struct sockaddr_in *addr, int length);
```

=== Description

#cmd("connect()") connects the socket #var("ss") to the address and port in
#var("addr").

On a blocking socket the connection is started, and if it is not yet
established, tested again one second later. If it is still not established
then, #cmd("connect()") gives up.

=== Returns

0, or -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("ECONNREFUSED")], [the connection was not established within about
    one second, on a blocking socket.],
  [#cmd("EINPROGRESS")], [#var("ss") is non-blocking and the connection is
    under way.],
)

=== Notes

- #var("addr") is a #cmd("struct sockaddr_in *") and #var("length") an
  #cmd("int")\; #var("length") is not used.
- A blocking #cmd("connect()") never waits much longer than one second. A
  slow peer is reported as #cmd("ECONNREFUSED"), not #cmd("ETIMEDOUT").

=== Related

@sockets-socket, @sockets-getpeername

== FD_ZERO, FD_SET, FD_CLR, FD_ISSET <sockets-fd>

#idx("FD_ZERO")
#idx("FD_SET")
#idx("FD_CLR")
#idx("FD_ISSET")
=== Format

```
#include <sys/select.h>

FD_ZERO(fd_set *p)
FD_SET(int n, fd_set *p)
FD_CLR(int n, fd_set *p)
FD_ISSET(int n, fd_set *p)
```

=== Description

These macros manage the sets of sockets passed to #cmd("select()").
#cmd("FD_ZERO") empties the set #var("p"), #cmd("FD_SET") adds socket
#var("n") to it, #cmd("FD_CLR") removes it, and #cmd("FD_ISSET") tests
it.

=== Returns

#cmd("FD_ISSET") is nonzero when #var("n") is in the set.

=== Notes

- #cmd("FD_ZERO") is inline assembler that clears the set with an executed
  #cmd("XC") whose length is the size of the set. Because #cmd("XC") clears
  one byte more than its length operand, #cmd("FD_ZERO") also clears the
  byte that follows the 128-byte #cmd("fd_set"). Clear a set with
  #cmd("memset(p, 0, sizeof(*p))") instead.
- Socket #var("n") is bit #var("n")#cmd("%32"), counted from the low-order
  end, of word #var("n")#cmd("/32"), as on other systems.
- #cmd("FD_SETSIZE") may be defined smaller before the header is
  included\; the set is then smaller. Defined as 2048 or more, it makes the
  set too large for #cmd("FD_ZERO") to clear.

=== Related

@sockets-select

== getaddrbyname <sockets-getaddrbyname>

#idx("getaddrbyname")
=== Format

```
#include <mvs/socket.h>

unsigned getaddrbyname(const char *name);
```

=== Description

#cmd("getaddrbyname()") looks up the host #var("name") with the resolver of
the workstation that runs Hercules and returns its first IPv4 address. The
name is in EBCDIC\; it ends at its NUL or at its first blank.

=== Returns

The address, in network byte order, or 0 when the name is empty or not
found.

=== Notes

#cmd("getaddrbyname()") is not a POSIX function. It does the work of
#cmd("gethostbyname()") without building a #cmd("struct hostent").

=== Related

@sockets-gethostbyname, @sockets-inet-aton

== gethostbyaddr <sockets-gethostbyaddr>

#idx("gethostbyaddr")
=== Format

```
#include <netdb.h>

struct hostent *gethostbyaddr(void *addr);
```

=== Description

#cmd("gethostbyaddr()") looks up the name of the IPv4 address at #var("addr")
(four bytes, in network byte order) with the resolver of the workstation
that runs Hercules. The result is a #cmd("struct hostent") as for
#cmd("gethostbyname()"), with #cmd("h_name") set to the name, translated to
EBCDIC.

=== Returns

A pointer to the #cmd("struct hostent"), or NULL when the address has no
name or the C run-time environment is not set up.

=== Notes

- POSIX #cmd("gethostbyaddr()") takes three arguments, the address, its length
  and its type\; this one takes the address alone.
- The result is in the same storage as that of #cmd("gethostbyname()") and is
  overwritten by the next call of either function in the same task.
- #cmd("h_name") holds at most 79 characters.
- The function writes a line beginning #cmd("__75ghba()"), with the address,
  to #cmd("stdout") on every call.

=== Related

@sockets-gethostbyname

== gethostbyname <sockets-gethostbyname>

#idx("gethostbyname")
=== Format

```
#include <netdb.h>

struct hostent *gethostbyname(const char *name);
```

=== Description

#cmd("gethostbyname()") looks up the host #var("name") with the resolver of
the workstation that runs Hercules and returns a #cmd("struct hostent")
describing it:

#deflist(width: 1.35in,
  [#cmd("h_name")], [the name as given, up to 79 characters.],
  [#cmd("h_aliases")], [an empty list.],
  [#cmd("h_addrtype")], [#cmd("AF_INET").],
  [#cmd("h_length")], [4.],
  [#cmd("h_addr_list")], [a list of one address, in network byte order,
    ended by NULL.],
)

The name is in EBCDIC\; it ends at its NUL or at its first blank.

=== Returns

A pointer to the #cmd("struct hostent"), or NULL when the name is not found
or the C run-time environment is not set up.

=== Notes

- Only the first address of the host is returned, however many it has.
- The result is in storage that belongs to the calling task. The next call
  of #cmd("gethostbyname()") or #cmd("gethostbyaddr()") in the same task
  overwrites it\; other tasks have their own.
- There is no #cmd("h_errno")\; the reason for a failure is not available.

=== Related

@sockets-getaddrbyname, @sockets-gethostbyaddr

== getpeername, getsockname <sockets-getpeername>

#idx("getpeername")
#idx("getsockname")
=== Format

```
#include <sys/socket.h>

int getpeername(int ss, struct sockaddr *addr, int *addrlen);
int getsockname(int ss, struct sockaddr *addr, int *addrlen);
```

=== Description

#cmd("getsockname()") stores the local address and port of the socket
#var("ss") in #var("addr")\; #cmd("getpeername()") stores the address and
port of the peer it is connected to.

The library remembers the addresses of each socket it opened, bound,
connected or accepted, and answers from what it remembers when it can, at
most #var("*addrlen") bytes of it. Otherwise it asks the emulator.

=== Returns

0, or -1 for an error.

=== Notes

- #var("addrlen") is an #cmd("int *"), not a #cmd("socklen_t *").
- On return #var("*addrlen") is set to 16, the size of a
  #cmd("struct sockaddr"), whatever was stored.
- After #cmd("bind()"), #cmd("getsockname()") returns the address as it was
  given to #cmd("bind()"), which may be #cmd("INADDR_ANY").

=== Related

@sockets-accept, @sockets-connect

== htonl, htons, ntohl, ntohs <sockets-htonl>

#idx("htonl")
#idx("htons")
#idx("ntohl")
#idx("ntohs")
=== Format

```
#include <netinet/in.h>

htonl(x)    htons(x)    ntohl(x)    ntohs(x)
HTONL(x)    HTONS(x)    NTOHL(x)    NTOHS(x)
```

=== Description

These macros convert a 32-bit (#cmd("l")) or 16-bit (#cmd("s")) value
between host and network byte order. On System/370 the two are the same,
so each macro is its argument, unchanged.

=== Notes

They are macros, not functions, and the argument is not converted to a 16-
or 32-bit type.

== inet_addr, inet_aton <sockets-inet-aton>

#idx("inet_aton")
#idx("inet_addr")
=== Format

```
#include <arpa/inet.h>

int inet_aton(const char *cp, struct in_addr *inp);
in_addr_t inet_addr(const char *cp);
```

=== Description

Both functions convert an IPv4 address in text form to a 32-bit address in
network byte order. They accept the forms BSD systems accept:

#deflist(width: 1.35in,
  [#cmd("a.b.c.d")], [four parts, one byte each.],
  [#cmd("a.b.c")], [#var("c") fills the last 16 bits.],
  [#cmd("a.b")], [#var("b") fills the last 24 bits.],
  [#cmd("a")], [the whole address.],
)

Each part is decimal, octal when it begins with #cmd("0"), or hexadecimal
when it begins with #cmd("0x") or #cmd("0X"). The address ends at the end of
the string or at a blank, tab or newline. A sign, an empty part, a fifth part
and a part too large for its place make the string invalid.

#cmd("inet_aton()") stores the address in #var("*inp")\; #var("inp") may be
NULL, to check the string only. #cmd("inet_addr()") returns the address.

=== Returns

#cmd("inet_aton()") returns 1 when #var("cp") is an address and 0 when it is
not or is NULL. #cmd("inet_addr()") returns the address, or
#cmd("INADDR_NONE") when #var("cp") is not one.

=== Notes

- #cmd("\"255.255.255.255\"") is a valid address whose value is
  #cmd("INADDR_NONE")\; only #cmd("inet_aton()") can tell it from an error.
- #var("*inp") is left unchanged when the string is not an address.
- Earlier versions of the library defined #cmd("in_addr_t") as a structure
  and returned 0 and -1 from #cmd("inet_aton()"). Programs written for them
  must be changed.
- The conversion does not use #cmd("scanf()") or #cmd("printf()") and adds only
  a few hundred bytes to a load module.

=== Example

```
struct in_addr a;

inet_aton("10.1", &a);          /* a.s_addr == 0x0A000001 */
inet_addr("192.168.1.1");       /* 0xC0A80101 */
```

=== Related

@sockets-inet-pton, @sockets-inet-ntop

== inet_ntoa <sockets-inet-ntoa>

#idx("inet_ntoa")
=== Format

```
#include <arpa/inet.h>

char *inet_ntoa(struct in_addr in);
```

=== Description

#cmd("inet_ntoa()") converts the address #var("in") to the text form
#cmd("a.b.c.d").

=== Returns

A pointer to the text, or NULL when the C run-time environment is not set
up.

=== Notes

The text is in one buffer per address space, which the next call
overwrites, whichever task makes it. Programs with more than one task use
#cmd("inet_ntop()") with a buffer of their own.

=== Related

@sockets-inet-ntop

== inet_ntop <sockets-inet-ntop>

#idx("inet_ntop")
=== Format

```
#include <arpa/inet.h>

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
```

=== Description

#cmd("inet_ntop()") converts the #cmd("struct in_addr") at #var("src") to the
text form #cmd("a.b.c.d") and stores it, with its terminating NUL, in
#var("dst"), which is #var("size") bytes long. #cmd("INET_ADDRSTRLEN")
bytes are always enough.

=== Returns

#var("dst"), or NULL for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EAFNOSUPPORT")], [#var("af") is not #cmd("AF_INET").],
  [#cmd("ENOSPC")], [the text does not fit in #var("size") bytes.
    #var("dst") is not changed.],
)

=== Related

@sockets-inet-pton, @sockets-inet-ntoa

== inet_pton <sockets-inet-pton>

#idx("inet_pton")
=== Format

```
#include <arpa/inet.h>

int inet_pton(int af, const char *src, void *dst);
```

=== Description

#cmd("inet_pton()") converts the text #var("src") to an address and stores it
in the #cmd("struct in_addr") at #var("dst"). It accepts the strict POSIX
form only: exactly four decimal parts from 0 to 255, separated by periods,
without leading zeros and with nothing after the last part.

=== Returns

1 when #var("src") is an address\; 0 when it is not, and #var("dst") is then
unchanged\; -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EAFNOSUPPORT")], [#var("af") is not #cmd("AF_INET").],
)

=== Related

@sockets-inet-aton, @sockets-inet-ntop

== ioctlsocket <sockets-ioctlsocket>

#idx("ioctlsocket")
#idx("FIONBIO")
#idx("FIONREAD")
=== Format

```
#include <mvs/socket.h>

int ioctlsocket(int ss, int cmd, void *argp);
```

=== Description

#cmd("ioctlsocket()") controls the socket #var("ss"). Two commands are
supported:

#deflist(width: 1.35in,
  [#cmd("FIONBIO")], [makes the socket non-blocking when #var("argp") is
    not NULL, and blocking when #var("argp") is NULL.],
  [#cmd("FIONREAD")], [stores in the #cmd("unsigned") word at #var("argp")
    the number of bytes that have arrived for the socket and can be read
    without waiting.],
)

=== Returns

0, or -1 for an error. #cmd("FIONBIO") does not fail.

=== Notes

- For #cmd("FIONBIO") the library passes the pointer #var("argp") to the
  emulator, not the value it points to. A pointer to a word holding 0
  therefore makes the socket non-blocking, just as a pointer to a word
  holding 1 does\; to make a socket blocking again, pass NULL.
- After #cmd("FIONBIO") the word at #var("argp") is set to 0.
- #cmd("FIONREAD") counts what has reached the host. A program that asks
  #cmd("recv()") for exactly that many bytes does not wait, on a blocking
  socket either.

=== Related

@sockets-recv, @sockets-select

== listen <sockets-listen>

#idx("listen")
=== Format

```
#include <sys/socket.h>

int listen(int ss, int backlog);
```

=== Description

#cmd("listen()") marks the bound socket #var("ss") as one that accepts
connections. #var("backlog") is the number of connections the host keeps
waiting for #cmd("accept()")\; 0 asks for the host's maximum.

=== Returns

0, or -1 for an error.

=== Related

@sockets-bind, @sockets-accept

== recv <sockets-recv>

#idx("recv")
=== Format

```
#include <sys/socket.h>

int recv(int ss, void *buf, int len, int flags);
```

=== Description

#cmd("recv()") reads data that has arrived on the connected socket #var("ss")
into #var("buf"), at most #var("len") bytes.

*On a blocking socket, #cmd("recv()") returns only when #var("len") bytes
have arrived, when the peer closes the connection, or when an error
occurs.* It does not return the part of a message that has arrived when the
data stops. On a non-blocking socket it returns what has arrived, up to
#var("len") bytes.

=== Returns

The number of bytes read\; 0 when the peer has closed the connection and
nothing was read\; -1 for an error. When an error or the end of the
connection follows data, the data is returned and the error is not.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EPERM")], [#var("ss") is negative or #var("buf") is NULL.],
  [#cmd("EWOULDBLOCK")], [#var("ss") is non-blocking and no data has
    arrived.],
)

=== Notes

- #var("flags") is not used. #cmd("MSG_PEEK") and the other flags of POSIX
  are not available.
- The return type is #cmd("int") and #var("len") an #cmd("int"), not
  #cmd("ssize_t") and #cmd("size_t").
- The data is moved without translation\; see @sockets-provider.
- To read what has arrived without waiting for #var("len") bytes, either
  make the socket non-blocking, or ask for the count with #cmd("FIONREAD")
  (see @sockets-ioctlsocket) and read no more than that. On a non-blocking
  socket, #cmd("errno") may be #cmd("EWOULDBLOCK") after a call that
  returned data.
- The data is copied from the host in pieces of 256 bytes\; this is not
  visible to the program.

=== Related

@sockets-send, @sockets-ioctlsocket, @sockets-select

== select <sockets-select>

#idx("select")
=== Format

```
#include <sys/select.h>

int select(int msock, fd_set *r, fd_set *w, fd_set *e, timeval *t);
```

=== Description

#cmd("select()") waits until one of the sockets in the sets #var("r"),
#var("w") and #var("e") is ready: a socket in #var("r") has data to read or
a connection to accept, a socket in #var("w") can be written, or a socket in
#var("e") has an exceptional condition. #var("msock") is the highest socket
number in the sets plus one. Any of the sets may be NULL.

On return each set holds only the sockets that are ready. #var("t") is the
longest time to wait\; NULL waits until a socket is ready, and a
#cmd("timeval") of zero tests once without waiting.

=== Returns

The number of ready sockets\; 0 when the time ran out\; -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("ENOTSOCK")], [a set holds a number that is not an open socket.],
)

=== Notes

- #cmd("select()") tests the sockets every 0.08 seconds. #var("t") is
  converted to a number of such tests: twelve for each second of
  #cmd("tv_sec"). A nonzero #cmd("tv_usec") adds one test for every 64
  microseconds, so that half a second becomes more than ten minutes. Give
  the time in whole seconds, with #cmd("tv_usec") 0.
- When #var("msock") is less than 2 -- no socket can be in the sets --
  #cmd("select()") returns 0 at once and does not change the sets.
- The socket #var("msock")#cmd("-1") must be open\; the emulator keeps
  its state for the call under that number.
- #var("t") is not changed.

=== Related

@sockets-selectex, @sockets-fd

== selectex <sockets-selectex>

#idx("selectex")
#idx("ECB", "waiting with sockets")
=== Format

```
#include <mvs/socket.h>

int selectex(int, fd_set *, fd_set *, fd_set *, timeval *, unsigned **);
```

=== Description

#cmd("selectex()") is #cmd("select()") with one argument more: a list of
event control blocks (ECBs). It returns when a socket is ready, when the
time runs out, or when one of the ECBs is posted, whichever comes first.
This lets a server wait for network activity and for a signal from another
task, such as a request to stop, at the same time.

The sixth argument is either

- a list of ECB addresses in the form MVS uses, in which the last address
  has its high-order bit set\; or
- an array of ECB addresses built with the functions of
  #cmd("<ext/array.h>") (see @ext-array), in which NULL entries are
  skipped.

A NULL list makes #cmd("selectex()") the same as #cmd("select()").

=== Returns

As for #cmd("select()"). When an ECB is posted, #cmd("selectex()") returns 0,
the same value as for a timeout, and the sets are empty.

=== Notes

- The ECBs are tested every 0.08 seconds, with the sockets. They are not
  waited on with #cmd("WAIT") and are not cleared.
- To tell a posted ECB from a timeout, test the ECBs after the call: an ECB
  is posted when its bit #cmd("0x40000000") is set.

=== Related

@sockets-select

== send <sockets-send>

#idx("send")
=== Format

```
#include <sys/socket.h>

int send(int ss, const void *buf, int len, int flags);
```

=== Description

#cmd("send()") sends #var("len") bytes from #var("buf") on the connected
socket #var("ss"). It may send fewer bytes than asked when the host's send
buffer is nearly full\; the program then sends the rest with another call.

When the send buffer is full, a blocking #cmd("send()") waits for the peer to
read: it tries again every 0.1 seconds, up to 100 times. If no byte could be
sent in those ten seconds, it fails.

=== Returns

The number of bytes sent, or -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EWOULDBLOCK")], [nothing could be sent: #var("ss") is
    non-blocking and the send buffer is full, or #var("ss") is blocking and
    the peer has not read for ten seconds.],
)

=== Notes

- #var("flags") is not used.
- The return type is #cmd("int") and #var("len") an #cmd("int"), not
  #cmd("ssize_t") and #cmd("size_t").
- The data is moved without translation\; see @sockets-provider.

=== Related

@sockets-recv

== socket <sockets-socket>

#idx("socket")
=== Format

```
#include <sys/socket.h>

int socket(int af, int type, int protocol);
```

=== Description

#cmd("socket()") creates a socket and returns its number. #var("af") is
#cmd("AF_INET")\; #var("type") is #cmd("SOCK_STREAM") for TCP or
#cmd("SOCK_DGRAM") for UDP\; #var("protocol") is 0, #cmd("IPPROTO_TCP") or
#cmd("IPPROTO_UDP").

The first #cmd("socket()") call of a program sets up the library's table of
open sockets.

=== Returns

The socket number, from 1 to 1023, or -1 for an error.

=== Errors

#deflist(width: 1.35in,
  [#cmd("EMFILE")], [all socket numbers are in use, by this or by other
    programs.],
)

=== Notes

- The program must run in the C run-time environment that the startup
  module sets up\; the socket table is kept there, one for each address
  space.
- The first call is also where a missing TCPIP instruction shows\; see
  @sockets-provider.

=== Related

@sockets-closesocket, @sockets-bind, @sockets-connect

== The Socket Table <sockets-table>

#idx("socket table")
#idx("CLIBSOCK")
=== Format

```
#include <mvs/socket.h>

int __soadd(int ss, void *name, void *peer);
int __soupd(int ss, void *name, void *peer);
int __sodel(int ss);
int __sofind(int ss, CLIBSOCK **s);
int __sosnam(int ss, void *name);
int __sopnam(int ss, void *peer);
```

=== Description

The library keeps a table of the sockets the address space has open, one
#cmd("CLIBSOCK") (48 bytes, eyecatcher #cmd("CLIBSOCK")) for each. It holds
the socket number and the local and peer addresses, each as a
#cmd("struct sockaddr_in"). #cmd("socket()"), #cmd("bind()"),
#cmd("connect()"), #cmd("accept()") and #cmd("closesocket()") maintain it, and
#cmd("getsockname()") and #cmd("getpeername()") read it, through these
functions.

#cmd("__soadd") adds an entry. #cmd("__soupd") replaces the addresses of an
entry, or adds the entry when there is none\; a NULL #var("name") or
#var("peer") leaves that address as it is. #cmd("__sodel") removes an
entry. #cmd("__sofind") finds the entry for #var("ss") and stores its
address in #var("*s").

=== Returns

#cmd("__sofind") returns the position of the entry, counted from 1, or 0
when there is none. #cmd("__soupd") and #cmd("__sodel") return 0, and
#cmd("__soadd") returns -1, whether it added the entry or not.

=== Notes

- A program does not normally call these functions. They change only the
  library's record, never the socket itself.
- #cmd("__sosnam") and #cmd("__sopnam") are declared but not defined in
  the library\; a program that calls them does not link. Use
  #cmd("getsockname()") and #cmd("getpeername()").
- #cmd("struct clientid") is declared in the header, but no function uses
  it.
