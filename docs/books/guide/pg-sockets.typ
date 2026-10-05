#import "../bookmaster/bookmaster.typ": *

= TCP/IP Sockets <pg-sockets>

#idx("sockets")
#idx("TCP/IP")
This chapter shows how a C program on MVS 3.8j talks to other hosts over
TCP/IP: what has to be in place before the first socket call can work, how
a small server and a small client are written, how text crosses the line
between EBCDIC and ASCII, and where the socket functions of libc370 differ
from those of a POSIX system. Each function is described in full in the
_libc370 Library Reference_, Chapter 23, “Sockets”.

== How a Socket Reaches the Network <pg-sockets-how>

#idx("X'75' instruction")
#idx("Hercules", "TCPIP instruction")
MVS 3.8j has no TCP/IP of its own, and libc370 does not bring one. The
socket functions hand each call to the Hercules emulator through the
_TCPIP instruction_, operation code #cmd("X'75'"). The emulator performs
the call with the socket layer of the workstation it runs on, and returns
the result to the program. Nothing has to be installed on MVS, and no
started task has to run.

The instruction is part of Hercules and is switched off unless the emulator
is told otherwise. Before a socket program can run:

+ Make sure that your Hercules is built with the TCP/IP extension for the
  S/370 architecture. The SDL Hyperion builds have it.
+ Enable the instruction for supervisor state and then for problem state,
  before MVS is IPLed. The usual place for the two commands is the Hercules
  configuration file:
  ```
  FACILITY ENABLE HERC_TCPIP_EXTENSION
  FACILITY ENABLE HERC_TCPIP_PROB_STATE
  ```
  C programs run in problem state, so both are needed.
+ IPL MVS.

#idx("abend", "S0C1 on first socket call")
#idx("abend", "S0C2 on first socket call")
Where the instruction is missing, the program gets no error code: its first
socket call ends it with abend #cmd("S0C1") (operation exception), or with
#cmd("S0C2") (privileged operation) when only the first facility is
enabled. The library cannot test for the instruction before it uses it. If
a socket program abends #cmd("S0C1") in its first call of #cmd("socket()"),
look at the Hercules configuration before you look at the program.

Two consequences of this design affect every socket program:

- *Socket numbers belong to the whole system.* The emulator assigns them
  from one table, 1 to 1023, shared by every program on the system. Your
  first socket is not necessarily number 1, and a number is not a file
  descriptor: the functions of #cmd("<stdio.h>") cannot use it, and there is
  no #cmd("close()") or #cmd("read()") for it.
- *Sockets are not closed when the program ends.* A socket left open keeps
  its number, and a listening socket keeps its port, until some program
  closes that number or Hercules is restarted. Close every socket with
  #cmd("closesocket()") before the program ends, on the error paths too.
  An abend skips the program's own cleanup\; how a program recovers from
  one is described in @pg-errors.

#note[Every program that uses the TCPIP instruction has the network access
of the Hercules process on the workstation, and can use or close any socket
on the system. Do not rely on the socket layer to protect one program from
another on the same system.]

== The Headers <pg-sockets-headers>

#idx("mvs/socket.h")
The POSIX headers declare what their names promise: #cmd("<sys/socket.h>"),
#cmd("<sys/select.h>"), #cmd("<netinet/in.h>"), #cmd("<arpa/inet.h>") and
#cmd("<netdb.h>"). The calls that POSIX does not have --
#cmd("closesocket()"), #cmd("ioctlsocket()"), #cmd("selectex()") and
#cmd("getaddrbyname()") -- are in #cmd("<mvs/socket.h>"). Since every socket
program closes its sockets, every socket program includes
#cmd("<mvs/socket.h>").

The program must run in the C run-time environment that the start-up module
builds (see @pg-startup): the library keeps its table of the program's
sockets there, one for each address space. The usual start-up,
#cmd("@@CRT0"), builds it.

== Text on the Line: EBCDIC and ASCII <pg-sockets-text>

#idx("EBCDIC", "socket data")
#idx("ASCII", "socket data")
#idx("translation", "of socket data")
#cmd("send()") and #cmd("recv()") move bytes and translate nothing. Your
program works in EBCDIC, and nearly every peer on the network expects
ASCII, so text has to be translated on the way out and on the way back.
Binary data -- a 32-bit length in a protocol header, an address -- is not
translated, and needs no byte swapping either: System/370 stores integers
in network byte order already. Use #cmd("htonl()") and #cmd("htons()")
anyway, so that the source stays portable\; here they return their argument
unchanged.

libc370 has no translation function for socket data, so the program brings
its own table. The code page behind it, and why the newline is
#cmd("X'15'"), are described in @pg-charset. The module in
@pg-sockets-asc-fig builds both directions from the compiler's own mapping:
a string literal that holds the printable ASCII characters in ASCII order is
stored by cc370 in EBCDIC, so the position of each character in the literal
is its ASCII code less #cmd("X'20'"). No EBCDIC code point is written into
the program, and the tables agree with every character constant the
compiler produces. The control characters a line protocol needs -- line
feed, carriage return and tab -- are added by hand, and every other byte
becomes a question mark.

#fig(caption: [asc.c, translation tables for socket text])[
  #code(read("../ex/pg-sockets/asc.c"), numbers: true)
] <pg-sockets-asc-fig>

The header, @pg-sockets-asch-fig, gives the three functions external names
of eight characters or less (see @pg-asm). The two tables take 512 bytes,
which the caller provides, normally on its stack: the module keeps no
writable data of its own, so it can be linked into a reentrant program (see
@pg-rent).

#fig(caption: [asc.h])[
  #code(read("../ex/pg-sockets/asc.h"))
] <pg-sockets-asch-fig>

Host names are the one exception: #cmd("gethostbyname()") and
#cmd("getaddrbyname()") take the name in EBCDIC, and the emulator translates
it.

== Writing a Server <pg-sockets-server>

#idx("server", "socket")
A server creates a socket, binds it to a port, listens on it and accepts
connections one after another. @pg-sockets-srv-fig is a time-of-day server:
for each connection it sends one line with the date and the time, in ASCII
and ended by a carriage return and a line feed, and closes the connection.
It serves the number of clients given as its argument, then ends.

#fig(caption: [TIMESRV, a time-of-day server])[
  #code(read("../ex/pg-sockets/timesrv.c"), numbers: true)
] <pg-sockets-srv-fig>

The steps, with what is particular to MVS:

+ *Create the socket* (line 41). #cmd("AF_INET") is the only address family
  the emulator supports.
+ *Bind it to the port* (lines 47--51). #cmd("bind()"), #cmd("connect()")
  and #cmd("accept()") take a #cmd("struct sockaddr_in *"), not a
  #cmd("struct sockaddr *"): pass the address of the
  #cmd("struct sockaddr_in") as it is. The cast that POSIX code carries,
  #cmd("(struct sockaddr *)&sin"), draws an incompatible-pointer warning,
  which #cmd("-Werror") makes an error. The port is a port of the
  workstation, not of MVS: it must be free there, and a port below 1024
  usually needs a Hercules that runs with the privileges for it.
+ *Close on every exit.* When #cmd("bind()") or #cmd("listen()") fails, the
  program closes the socket before it returns (line 53). Without that the
  socket would stay open after the program has ended.
+ *Accept a connection* (line 60). A blocking #cmd("accept()") waits until
  a connection arrives, looking every 0.08 seconds, so the waiting program
  does not hold the processor. Its third argument is an #cmd("int *") and
  is neither read nor changed.
+ *Convert the peer address* with #cmd("inet_ntop()") into a buffer of the
  program's own (line 65). #cmd("inet_ntoa()") would do as well in this
  program, but its result is one buffer for the whole address space,
  overwritten by the next call from any task.
+ *Format, translate and send* (lines 68--73). #cmd("send()") may send
  fewer bytes than asked when the buffer on the workstation is nearly
  full\; #cmd("send_all()") sends the rest. A blocking #cmd("send()") gives
  up when the peer has not read anything for ten seconds.
+ *Close the connection* (line 74), and at the end the listening socket.

The date is formatted with #cmd("%Y-%m-%d %H:%M:%S") rather than with
#cmd("%x") or #cmd("%T"), which #cmd("strftime()") of this library does not
support (see the _libc370 Library Reference_, Chapter 20, “\<time.h\> — Date
and Time”).

=== Waiting for More Than the Network

#idx("selectex")
#idx("ECB", "waiting with sockets")
A server that runs as a started task has to stop when the operator tells it
to, but #cmd("accept()") and #cmd("select()") wait only for the network.
#cmd("selectex()") waits for sockets _and_ for a list of event control
blocks, and returns when either is ready. Let another task post an ECB when
the server is to stop -- the task that reads the operator's #cmd("STOP")
command, for example (see @pg-services and @pg-tasks) -- and pass that ECB
to #cmd("selectex()") together with the listening socket. When
#cmd("selectex()") returns 0, test the posted bit, #cmd("0x40000000"), of
the ECB: a posted ECB and a timeout both return 0.

To serve several clients at the same time, give each connection to a
thread of its own (@pg-tasks). The socket table belongs to the address
space, so a thread can use a socket that another thread accepted.

== Writing a Client <pg-sockets-client>

#idx("client", "socket")
@pg-sockets-cli-fig asks the server of @pg-sockets-srv-fig for the time.
It takes the server's address, or its name, as its first argument and the
port as an optional second one.

#fig(caption: [TIMECLI, a client for TIMESRV])[
  #code(read("../ex/pg-sockets/timecli.c"), numbers: true)
] <pg-sockets-cli-fig>

+ *Find the address* (lines 24--34). #cmd("inet_aton()") converts an
  address written as text and returns 1 for success and 0 for failure, as
  on BSD systems. When the argument is not an address, the program asks the
  resolver of the workstation with #cmd("getaddrbyname()"), which returns
  the first address of the host, or 0. #cmd("gethostbyname()") does the same
  work and builds a #cmd("struct hostent") as well.
+ *Connect* (line 42). A blocking #cmd("connect()") tries for about one
  second and then gives up. A peer that is merely slow is reported as
  #cmd("ECONNREFUSED"), not #cmd("ETIMEDOUT")\; a client that has to reach
  a distant or busy server tries again.
+ *Read the answer* (lines 50--55). *A blocking #cmd("recv()") returns only
  when the full length asked for has arrived, or when the peer closes the
  connection.* It does not return the part that has arrived when the data
  stops. That is safe here, because the server closes the connection after
  its line. It is not safe for a protocol in which the peer answers and then
  waits for more: the client would wait for ever. For such a protocol, make
  the socket non-blocking and wait in #cmd("select()"), as the ECHO example
  in the Library Reference does, or ask #cmd("ioctlsocket()") with
  #cmd("FIONREAD") how many bytes have arrived and read no more than that.
+ *Close the socket* (line 56) -- before the program uses what it read, so
  that no later path can leave it open.
+ *Translate the text back* to EBCDIC and print it.

== Building and Running the Programs <pg-sockets-run>

Compile and link both programs with cc370 on the workstation. The
translation module is linked into each:

#fig(caption: [Building TIMESRV and TIMECLI])[
  #screen(raw(read("../ex/pg-sockets/build.txt")))
] <pg-sockets-build-fig>

Transfer the load modules to a load library on MVS as the _cc370 User's
Guide_ describes, and run the server as a batch job:

```
//TIMESRV  JOB  (ACCT),'TIME SERVER',CLASS=A,MSGCLASS=X
//RUN      EXEC PGM=TIMESRV,PARM='3'
//STEPLIB  DD   DSN=your.LOADLIB,DISP=SHR
//SYSPRINT DD   SYSOUT=*
```

While the job runs, the server listens on port 7008 of the workstation that
runs Hercules. Any TCP client on the network can ask it for the time, and so
can TIMECLI on MVS, given the address of that workstation as its parameter:

```
//TIMECLI  JOB  (ACCT),'TIME CLIENT',CLASS=A,MSGCLASS=X
//RUN      EXEC PGM=TIMECLI,PARM='127.0.0.1'
//STEPLIB  DD   DSN=your.LOADLIB,DISP=SHR
//SYSPRINT DD   SYSOUT=*
```

#fig(caption: [SYSPRINT of TIMESRV and TIMECLI])[
  _Output to be captured on MVS._
] <pg-sockets-run-fig>

TIMESRV writes the address of each client it serves to SYSPRINT, and ends
with return code 0 after the third client. TIMECLI writes the line it
received and ends with return code 0, or with 8 and a message from
#cmd("perror()") when it could not connect.

== Differences from POSIX <pg-sockets-posix>

#idx("sockets", "differences from POSIX")
The socket calls look like the ones a C programmer knows, and most of the
differences are discovered the hard way. @pg-sockets-diff-tab collects
those a program is most likely to meet. The Notes of each entry in the
Library Reference give the complete list.

#tab(caption: [Socket calls: what differs from POSIX])[
  #table(columns: (1.55in, 1fr),
    [*Call or area*], [*What to do on MVS*],
    [#cmd("close()")], [Does not exist for sockets. Use
      #cmd("closesocket()"), and call it on every path: the library does not
      close sockets when the program ends.],
    [#cmd("bind()"), #cmd("connect()"), #cmd("accept()")], [Take a
      #cmd("struct sockaddr_in *"): pass it without a cast. The length
      arguments are not used\; #cmd("accept()")'s is an #cmd("int *").],
    [#cmd("recv()")], [Blocking: returns only when all #var("len") bytes
      have arrived or the peer closes. Use a non-blocking socket, or
      #cmd("FIONREAD"), to read what has arrived. #var("flags") is ignored\;
      there is no #cmd("MSG_PEEK").],
    [#cmd("send()")], [May send fewer bytes than asked\; loop. A blocking
      #cmd("send()") fails with #cmd("EWOULDBLOCK") after ten seconds
      without progress. #var("flags") is ignored.],
    [#cmd("connect()")], [A blocking connect gives up after about one
      second, with #cmd("ECONNREFUSED").],
    [#cmd("select()")], [Give the time in whole seconds, with
      #cmd("tv_usec") 0: a non-zero #cmd("tv_usec") makes half a second last
      minutes. The socket #var("msock")#cmd("-1") must be open.],
    [#cmd("FD_ZERO")], [Clears one byte beyond the #cmd("fd_set"). Clear a
      set with #cmd("memset(&set, 0, sizeof(set))") instead.],
    [#cmd("ioctlsocket()")], [Takes the place of #cmd("fcntl()") for
      #cmd("FIONBIO"). Any non-NULL #var("argp") makes the socket
      non-blocking, even one that points at 0\; pass #cmd("NULL") to make it
      blocking again. A socket returned by #cmd("accept()") is always
      blocking.],
    [#cmd("inet_ntoa()")], [One buffer for the address space, shared by all
      tasks\; NULL without a C run-time environment. Use #cmd("inet_ntop()")
      with a buffer of your own.],
    [#cmd("inet_aton()"), #cmd("in_addr_t")], [Return 1 and 0, and
      #cmd("in_addr_t") is the address itself, as on BSD. Earlier versions of
      the library differed\; see @pg-migrate.],
    [#cmd("gethostbyaddr()")], [Takes the address alone, without length and
      type, and writes a trace line to #cmd("stdout") on every call.],
    [#cmd("gethostbyname()")], [Returns only the first address. There is no
      #cmd("h_errno").],
    [#cmd("bind()") with port 0], [The library chooses a port, and calls
      #cmd("srand()") on the way, which restarts the sequence of
      #cmd("rand()").],
    [Not provided], [#cmd("shutdown"), #cmd("setsockopt"),
      #cmd("getsockopt"), #cmd("sendto"), #cmd("recvfrom"), #cmd("sendmsg"),
      #cmd("recvmsg"), #cmd("poll"), #cmd("getaddrinfo"),
      #cmd("getnameinfo"), #cmd("socketpair"). Only IPv4.],
  )
] <pg-sockets-diff-tab>

#idx("errno", "socket functions")
A failing socket call returns -1 and sets #cmd("errno"). Most values come
from the socket layer of the workstation, translated by the emulator to the
numbers of #cmd("<errno.h>"), so #cmd("perror()") prints a meaningful
message for them (see @pg-errors).
