#import "../bookmaster/bookmaster.typ": *

= \<errno.h\> — Errors <std-errno>

#idx("errno.h")
#idx("errno")
The header #cmd("<errno.h>") defines #cmd("errno"), through which library
functions report the reason for a failure, and the macros for its values.

== errno <std-errno-errno>

#idx("errno", "per task")

=== Format

```
#include <errno.h>

extern int *__errno(void);
#define errno *(__errno())
```

=== Description

#cmd("errno") is a modifiable lvalue of type #cmd("int"). A library
function that fails stores a positive value in it that names the reason.
A function that succeeds may leave an earlier value in it. The functions that
list data sets and members set #cmd("errno") to 0 on entry, so that a null
result with #cmd("errno") 0 means an empty list. #cmd("fopen()") does not:
after a failed #cmd("fopen()"), #cmd("errno") may still hold an earlier
value. In general a program that needs #cmd("errno") to
decide whether a call failed sets it to 0 before the call and examines it
after. #cmd("errno") is 0 when #cmd("main") is entered.

#cmd("errno") is a macro, not a variable: it expands to a call of
#cmd("__errno"), which returns the address of the error number of the
current task. Every MVS task (TCB) that runs C code has its own C run-time
block, and the error number is kept there. A subtask that the program
attaches therefore has an #cmd("errno") of its own, and a value stored by
one task is not seen by another.

=== Notes

- Always include #cmd("<errno.h>"). A module that declares
  #cmd("extern int errno") without the header refers to a variable that
  does not exist, and the link fails with an unresolved reference.
- Before the run-time block of a task exists -- in code that runs before
  the C start-up has set it up -- #cmd("__errno") returns the address of a
  single word shared by all such callers.
- The values are those of the table below and are fixed. They are not the
  return codes or reason codes of MVS services; a function that fails in an
  MVS service maps the failure to one of these values or returns the
  service's code itself, as its entry describes.

=== Example

@std-errno-ex uses #cmd("errno") to tell a number that is too large from a
number that is simply #cmd("LONG_MAX"), which #cmd("strtol()") returns in
both cases.

#fig(caption: [Testing errno after strtol])[
  #code(read("../ex/std-errno/limit.c"), numbers: true)
] <std-errno-ex>

=== Related

#cmd("strerror()"), #cmd("perror()") (see @std-string and @std-stdio).

== Error Values <std-errno-values>

#idx("error numbers")
The header defines 105 names for errno values. They fall into four groups:
the three values the C standard requires, values for system and file
errors, values for sockets, and five values of the library's own for VSAM.
The numbers follow the usual UNIX numbering, so that a program ported to
MVS finds the names it expects; most of them describe conditions that
cannot occur on MVS and are never stored by the library. The meaning given
is the comment in the header, which is also the text #cmd("strerror()")
returns for the value.

@std-errno-std to @std-errno-vsam list all of them. The number 11 has no
name.

#tab(caption: [errno values of the C standard])[
  #table(columns: (1.45in, 0.45in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("EDOM")], [33], [Math argument out of domain of func],
    [#cmd("ERANGE")], [34], [Math result not representable],
    [#cmd("EILSEQ")], [84], [Illegal byte sequence],
  )
] <std-errno-std>

#tab(caption: [errno values for system and file errors, 1 to 32])[
  #table(columns: (1.45in, 0.45in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("EPERM")], [1], [Operation not permitted],
    [#cmd("ENOENT")], [2], [No such file or directory],
    [#cmd("ESRCH")], [3], [No such process],
    [#cmd("EINTR")], [4], [Interrupted system call],
    [#cmd("EIO")], [5], [I/O error],
    [#cmd("ENXIO")], [6], [No such device or address],
    [#cmd("E2BIG")], [7], [Argument list too long],
    [#cmd("ENOEXEC")], [8], [Exec format error],
    [#cmd("EBADF")], [9], [Bad file number],
    [#cmd("ECHILD")], [10], [No child processes],
    [#cmd("ENOMEM")], [12], [Out of memory],
    [#cmd("EACCES")], [13], [Permission denied],
    [#cmd("EFAULT")], [14], [Bad address],
    [#cmd("ENOTBLK")], [15], [Block device required],
    [#cmd("EBUSY")], [16], [Device or resource busy],
    [#cmd("EEXIST")], [17], [File exists],
    [#cmd("EXDEV")], [18], [Cross-device link],
    [#cmd("ENODEV")], [19], [No such device],
    [#cmd("ENOTDIR")], [20], [Not a directory],
    [#cmd("EISDIR")], [21], [Is a directory],
    [#cmd("EINVAL")], [22], [Invalid argument],
    [#cmd("ENFILE")], [23], [File table overflow],
    [#cmd("EMFILE")], [24], [Too many open files],
    [#cmd("ENOTTY")], [25], [Not a typewriter],
    [#cmd("ETXTBSY")], [26], [Text file busy],
    [#cmd("EFBIG")], [27], [File too large],
    [#cmd("ENOSPC")], [28], [No space left on device],
    [#cmd("ESPIPE")], [29], [Illegal seek],
    [#cmd("EROFS")], [30], [Read-only file system],
    [#cmd("EMLINK")], [31], [Too many links],
    [#cmd("EPIPE")], [32], [Broken pipe],
  )
] <std-errno-sys>

#tab(caption: [errno values for system and file errors, 62 to 131])[
  #table(columns: (1.45in, 0.45in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("ELOOP")], [62], [Too many levels of symbolic links],
    [#cmd("ENAMETOOLONG")], [63], [File name too long],
    [#cmd("ENOTEMPTY")], [66], [Directory not empty],
    [#cmd("EPROCLIM")], [67], [Too many processes],
    [#cmd("EUSERS")], [68], [Too many users],
    [#cmd("EDQUOT")], [69], [Disc quota exceeded],
    [#cmd("ESTALE")], [70], [Stale NFS file handle],
    [#cmd("EREMOTE")], [71], [Too many levels of remote in path],
    [#cmd("EMULTIHOP")], [72], [Multihop attempted],
    [#cmd("EDOTDOT")], [73], [RFS specific error],
    [#cmd("EBADMSG")], [74], [Not a data message],
    [#cmd("EOVERFLOW")], [75], [Value too large for defined data type],
    [#cmd("ENOTUNIQ")], [76], [Name not unique on network],
    [#cmd("EBADFD")], [77], [File descriptor in bad state],
    [#cmd("EREMCHG")], [78], [Remote address changed],
    [#cmd("ELIBACC")], [79], [Can not access a needed shared library],
    [#cmd("ELIBBAD")], [80], [Accessing a corrupted shared library],
    [#cmd("ELIBSCN")], [81], [.lib section in a.out corrupted],
    [#cmd("ELIBMAX")], [82], [Attempting to link in too many shared libraries],
    [#cmd("ELIBEXEC")], [83], [Cannot exec a shared library directly],
    [#cmd("ERESTART")], [85], [Interrupted system call should be restarted],
    [#cmd("ESTRPIPE")], [86], [Streams pipe error],
    [#cmd("EUCLEAN")], [117], [Structure needs cleaning],
    [#cmd("ENOTNAM")], [118], [Not a named type file],
    [#cmd("ENAVAIL")], [119], [No semaphores available],
    [#cmd("EISNAM")], [120], [Is a named type file],
    [#cmd("EREMOTEIO")], [121], [Remote I/O error],
    [#cmd("ENOMEDIUM")], [123], [No medium found],
    [#cmd("EMEDIUMTYPE")], [124], [Wrong medium type],
    [#cmd("ECANCELED")], [125], [Operation Canceled],
    [#cmd("ENOKEY")], [126], [Required key not available],
    [#cmd("EKEYEXPIRED")], [127], [Key has expired],
    [#cmd("EKEYREVOKED")], [128], [Key has been revoked],
    [#cmd("EKEYREJECTED")], [129], [Key was rejected by service],
    [#cmd("EOWNERDEAD")], [130], [Owner died],
    [#cmd("ENOTRECOVERABLE")], [131], [State not recoverable],
  )
] <std-errno-sys2>

#tab(caption: [errno values for sockets])[
  #table(columns: (1.45in, 0.45in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("EAGAIN")], [35], [Resource temporarily unavailable],
    [#cmd("EWOULDBLOCK")], [35], [Operation would block; the same value as #cmd("EAGAIN")],
    [#cmd("EINPROGRESS")], [36], [Operation now in progress],
    [#cmd("EALREADY")], [37], [Operation already in progress],
    [#cmd("ENOTSOCK")], [38], [Socket operation on non-socket],
    [#cmd("EDESTADDRREQ")], [39], [Destination address required],
    [#cmd("EMSGSIZE")], [40], [Message too long],
    [#cmd("EPROTOTYPE")], [41], [Protocol wrong type for socket],
    [#cmd("ENOPROTOOPT")], [42], [Protocol not available],
    [#cmd("EPROTONOSUPPORT")], [43], [Protocol not supported],
    [#cmd("ESOCKTNOSUPPORT")], [44], [Socket type not supported],
    [#cmd("EOPNOTSUPP")], [45], [Operation not supported],
    [#cmd("EPFNOSUPPORT")], [46], [Protocol family not supported],
    [#cmd("EAFNOSUPPORT")], [47], [Address family not supported by protocol family],
    [#cmd("EADDRINUSE")], [48], [Address already in use],
    [#cmd("EADDRNOTAVAIL")], [49], [Can’t assign requested address],
    [#cmd("ENETDOWN")], [50], [Network is down],
    [#cmd("ENETUNREACH")], [51], [Network is unreachable],
    [#cmd("ENETRESET")], [52], [Network dropped connection on reset],
    [#cmd("ECONNABORTED")], [53], [Software caused connection abort],
    [#cmd("ECONNRESET")], [54], [Connection reset by peer],
    [#cmd("ENOBUFS")], [55], [No buffer space available],
    [#cmd("EISCONN")], [56], [Socket is already connected],
    [#cmd("ENOTCONN")], [57], [Socket is not connected],
    [#cmd("ESHUTDOWN")], [58], [Can’t send after socket shutdown],
    [#cmd("ETOOMANYREFS")], [59], [Too many references: can't splice],
    [#cmd("ETIMEDOUT")], [60], [Operation timed out],
    [#cmd("ECONNREFUSED")], [61], [Connection refused],
    [#cmd("EHOSTDOWN")], [64], [Host is down],
    [#cmd("EHOSTUNREACH")], [65], [No route to host],
  )
] <std-errno-net>

#tab(caption: [errno values for VSAM])[
  #table(columns: (1.45in, 0.45in, 1fr),
    [Name], [Value], [Meaning],
    [#cmd("EVSTYPE")], [150], [Invalid VSAM type],
    [#cmd("EVSACCESS")], [151], [Invalid VSAM access],
    [#cmd("EVSMODE")], [152], [Invalid VSAM mode],
    [#cmd("EVSOPEN")], [153], [VSAM cluster could not be opened],
    [#cmd("EVSERROR")], [154], [VSAM error occurred],
  )
] <std-errno-vsam>

#idx("EWOULDBLOCK")
#idx("EAGAIN")
#cmd("EWOULDBLOCK") is defined as #cmd("EAGAIN"), so the two names cannot
be told apart. A socket function stores the number that the TCP/IP stack
reports, and that number is not checked against this list; the socket
values above agree with the numbering the stack uses.

#idx("VSAM", "errno values")
The VSAM values are stored by the functions that open a VSAM cluster (see
@mvs-vsam) and returned by them as their result.

=== Notes

- #cmd("strerror()") returns a null pointer for the VSAM values 150 to 154,
  and for any number above 131. #cmd("perror()") prints such a number as
  #cmd("unknown error:") followed by the number. A program that passes the
  result of #cmd("strerror()") to #cmd("printf()") must test it for
  #cmd("NULL") first.
- #cmd("strerror(11)") returns #cmd("\"Try again\""), although no name has
  the value 11. #cmd("EAGAIN") is 35.
- Values that the library stores itself include #cmd("EINVAL"),
  #cmd("EIO"), #cmd("ERANGE"), #cmd("ENOMEM"), #cmd("EOPNOTSUPP"),
  #cmd("EDOM"), #cmd("EBADF"), #cmd("ENOSPC"), #cmd("EAFNOSUPPORT"),
  #cmd("EOVERFLOW"), #cmd("ESPIPE"), #cmd("EPERM"), #cmd("EBUSY") and the
  VSAM values. Each function entry names the values that function stores.
