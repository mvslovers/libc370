/* STRERROR.C */
#include <stddef.h>
#include <string.h>
#include "errno.h"

static char *errors[];

/* the library's own VSAM errnos, EVSTYPE .. EVSERROR */
static char *vserrors[] = {
/* EVSTYPE             150 */ "Invalid VSAM type",
/* EVSACCESS           151 */ "Invalid VSAM access",
/* EVSMODE             152 */ "Invalid VSAM mode",
/* EVSOPEN             153 */ "VSAM cluster could not be opened",
/* EVSERROR            154 */ "VSAM error occurred",
};

/* C99 7.21.6.2: a string for every value, never a null pointer (#420).
   The values no errno has, from slots of the table below, from above
   ENOTRECOVERABLE or below 0, share one text.  char *, as <string.h>
   declares it; the strings are not to be written. */
char *
strerror(int error)
{
    char        *str    = 0;

    if (error >= 0 && error <= ENOTRECOVERABLE) {
        str = errors[error];
    }
    else if (error >= EVSTYPE && error <= EVSERROR) {
        str = vserrors[error - EVSTYPE];
    }

    return str ? str : "Unknown error";
}

static char *errors[] = {
/*                       0 */ "No Error",
/* EPERM                 1 */ "Operation not permitted",
/* ENOENT                2 */ "No such file or directory",
/* ESRCH                 3 */ "No such process",
/* EINTR                 4 */ "Interrupted system call",
/* EIO                   5 */ "I/O error",
/* ENXIO                 6 */ "No such device or address",
/* E2BIG                 7 */ "Argument list too long",
/* ENOEXEC               8 */ "Exec format error",
/* EBADF                 9 */ "Bad file number",
/* ECHILD               10 */ "No child processes",
/* (none)               11 */ 0,   /* no errno has this number */
/* ENOMEM               12 */ "Out of memory",
/* EACCES               13 */ "Permission denied",
/* EFAULT               14 */ "Bad address",
/* ENOTBLK              15 */ "Block device required",
/* EBUSY                16 */ "Device or resource busy",
/* EEXIST               17 */ "File exists",
/* EXDEV                18 */ "Cross-device link",
/* ENODEV               19 */ "No such device",
/* ENOTDIR              20 */ "Not a directory",
/* EISDIR               21 */ "Is a directory",
/* EINVAL               22 */ "Invalid argument",
/* ENFILE               23 */ "File table overflow",
/* EMFILE               24 */ "Too many open files",
/* ENOTTY               25 */ "Not a typewriter",
/* ETXTBSY              26 */ "Text file busy",
/* EFBIG                27 */ "File too large",
/* ENOSPC               28 */ "No space left on device",
/* ESPIPE               29 */ "Illegal seek",
/* EROFS                30 */ "Read-only file system",
/* EMLINK               31 */ "Too many links",
/* EPIPE                32 */ "Broken pipe",
/* EDOM                 33 */ "Math argument out of domain of func",
/* ERANGE               34 */ "Math result not representable",
/* EAGAIN               35 */ "Resource temporarily unavailable",
/* EINPROGRESS          36 */ "Operation now in progress",
/* EALREADY             37 */ "Operation already in progress",

/* ENOTSOCK             38 */ "Socket operation on non-socket",
/* EDESTADDRREQ         39 */ "Destination address required",
/* EMSGSIZE             40 */ "Message too long",
/* EPROTOTYPE           41 */ "Protocol wrong type for socket",
/* ENOPROTOOPT          42 */ "Protocol not available",
/* EPROTONOSUPPORT      43 */ "Protocol not supported",
/* ESOCKTNOSUPPORT      44 */ "Socket type not supported",
/* EOPNOTSUPP           45 */ "Operation not supported",
/* EPFNOSUPPORT         46 */ "Protocol family not supported",
/* EAFNOSUPPORT         47 */ "Address family not supported by protocol family",
/* EADDRINUSE           48 */ "Address already in use",
/* EADDRNOTAVAIL        49 */ "Can't assign requested address",

/* ENETDOWN             50 */ "Network is down",
/* ENETUNREACH          51 */ "Network is unreachable",
/* ENETRESET            52 */ "Network dropped connection on reset",
/* ECONNABORTED         53 */ "Software caused connection abort",
/* ECONNRESET           54 */ "Connection reset by peer",
/* ENOBUFS              55 */ "No buffer space available",
/* EISCONN              56 */ "Socket is already connected",
/* ENOTCONN             57 */ "Socket is not connected",
/* ESHUTDOWN            58 */ "Can't send after socket shutdown",
/* ETOOMANYREFS         59 */ "Too many references: can't splice",
/* ETIMEDOUT            60 */ "Operation timed out",
/* ECONNREFUSED         61 */ "Connection refused",

/* ELOOP                62 */ "Too many levels of symbolic links",
/* ENAMETOOLONG         63 */ "File name too long",
/* EHOSTDOWN            64 */ "Host is down",
/* EHOSTUNREACH         65 */ "No route to host",
/* ENOTEMPTY            66 */ "Directory not empty",
/* EPROCLIM             67 */ "Too many processes",
/* EUSERS               68 */ "Too many users",
/* EDQUOT               69 */ "Disc quota exceeded",
/* ESTALE               70 */ "Stale NFS file handle",
/* EREMOTE              71 */ "Too many levels of remote in path",

/* EMULTIHOP            72 */ "Multihop attempted",
/* EDOTDOT              73 */ "RFS specific error",
/* EBADMSG              74 */ "Not a data message",
/* EOVERFLOW            75 */ "Value too large for defined data type",
/* ENOTUNIQ             76 */ "Name not unique on network",
/* EBADFD               77 */ "File descriptor in bad state",
/* EREMCHG              78 */ "Remote address changed",
/* ELIBACC              79 */ "Can not access a needed shared library",
/* ELIBBAD              80 */ "Accessing a corrupted shared library",
/* ELIBSCN              81 */ ".lib section in a.out corrupted",
/* ELIBMAX              82 */ "Attempting to link in too many shared libraries",
/* ELIBEXEC             83 */ "Cannot exec a shared library directly",
/* EILSEQ               84 */ "Illegal byte sequence",
/* ERESTART             85 */ "Interrupted system call should be restarted",
/* ESTRPIPE             86 */ "Streams pipe error",
/* (none)               87 */ 0,   /* no errno has this number */
/* (none)               88 */ 0,   /* no errno has this number */
/* (none)               89 */ 0,   /* no errno has this number */
/* (none)               90 */ 0,   /* no errno has this number */
/* (none)               91 */ 0,   /* no errno has this number */
/* (none)               92 */ 0,   /* no errno has this number */
/* (none)               93 */ 0,   /* no errno has this number */
/* (none)               94 */ 0,   /* no errno has this number */
/* (none)               95 */ 0,   /* no errno has this number */
/* (none)               96 */ 0,   /* no errno has this number */
/* (none)               97 */ 0,   /* no errno has this number */
/* (none)               98 */ 0,   /* no errno has this number */
/* (none)               99 */ 0,   /* no errno has this number */
/* (none)              100 */ 0,   /* no errno has this number */
/* (none)              101 */ 0,   /* no errno has this number */
/* (none)              102 */ 0,   /* no errno has this number */
/* (none)              103 */ 0,   /* no errno has this number */
/* (none)              104 */ 0,   /* no errno has this number */
/* (none)              105 */ 0,   /* no errno has this number */
/* (none)              106 */ 0,   /* no errno has this number */
/* (none)              107 */ 0,   /* no errno has this number */
/* (none)              108 */ 0,   /* no errno has this number */
/* (none)              109 */ 0,   /* no errno has this number */
/* (none)              110 */ 0,   /* no errno has this number */
/* (none)              111 */ 0,   /* no errno has this number */
/* (none)              112 */ 0,   /* no errno has this number */
/* (none)              113 */ 0,   /* no errno has this number */
/* (none)              114 */ 0,   /* no errno has this number */
/* (none)              115 */ 0,   /* no errno has this number */
/* (none)              116 */ 0,   /* no errno has this number */
/* EUCLEAN             117 */ "Structure needs cleaning",
/* ENOTNAM             118 */ "Not a named type file",
/* ENAVAIL             119 */ "No semaphores available",
/* EISNAM              120 */ "Is a named type file",
/* EREMOTEIO           121 */ "Remote I/O error",
/* (none)              122 */ 0,   /* no errno has this number */
/* ENOMEDIUM           123 */ "No medium found",
/* EMEDIUMTYPE         124 */ "Wrong medium type",
/* ECANCELED           125 */ "Operation Canceled",
/* ENOKEY              126 */ "Required key not available",
/* EKEYEXPIRED         127 */ "Key has expired",
/* EKEYREVOKED         128 */ "Key has been revoked",
/* EKEYREJECTED        129 */ "Key was rejected by service",
/* EOWNERDEAD          130 */ "Owner died",
/* ENOTRECOVERABLE     131 */ "State not recoverable",
};
