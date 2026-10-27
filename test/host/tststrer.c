/*
 * tststrer.c - libc370 #420: strerror() returns a string for every value.
 *
 * ISSUE #420: strerror() returned NULL above ENOTRECOVERABLE (131) -
 * the library's own VSAM errnos 150-154 among them - and below 0, and
 * the slots of its table that no errno has (11, 87-116, 122) held texts
 * of other systems' numbers.  C99 7.21.6.2 allows no null pointer.
 *
 * This #includes the real strerror.c.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tststrer.c && ./t
 *
 * sweep(): every value errno.h defines has a text of its own, the gaps
 * are "Unknown error", and no int gets a null or empty string.  Each of
 * these fails the run: the VSAM range removed, an errno's text removed, a
 * text in the dead slot 103, strerror() returning the table entry as is.
 *
 * RED, 2026-10-19, against main's strerror.c: NULL for 150, 200 and -1,
 * "Software caused connection abort" for 103.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <limits.h>
#include "../../src/string/strerror.c"

static int bad;

/* Every errno errno.h defines, one name per value.  A new errno that is
   missing here fails the "unknown" check below until it is added. */
static const int known[] = {
    EPERM, ENOENT, ESRCH, EINTR, EIO, ENXIO, E2BIG, ENOEXEC, EBADF, ECHILD,
   ENOMEM, EACCES, EFAULT, ENOTBLK, EBUSY, EEXIST, EXDEV, ENODEV, ENOTDIR,
   EISDIR, EINVAL, ENFILE, EMFILE, ENOTTY, ETXTBSY, EFBIG, ENOSPC, ESPIPE,
   EROFS, EMLINK, EPIPE, EDOM, ERANGE, EAGAIN, EINPROGRESS, EALREADY,
   ENOTSOCK, EDESTADDRREQ, EMSGSIZE, EPROTOTYPE, ENOPROTOOPT,
   EPROTONOSUPPORT, ESOCKTNOSUPPORT, EOPNOTSUPP, EPFNOSUPPORT,
   EAFNOSUPPORT, EADDRINUSE, EADDRNOTAVAIL, ENETDOWN, ENETUNREACH,
   ENETRESET, ECONNABORTED, ECONNRESET, ENOBUFS, EISCONN, ENOTCONN,
   ESHUTDOWN, ETOOMANYREFS, ETIMEDOUT, ECONNREFUSED, ELOOP, ENAMETOOLONG,
   EHOSTDOWN, EHOSTUNREACH, ENOTEMPTY, EPROCLIM, EUSERS, EDQUOT, ESTALE,
   EREMOTE, EMULTIHOP, EDOTDOT, EBADMSG, EOVERFLOW, ENOTUNIQ, EBADFD,
   EREMCHG, ELIBACC, ELIBBAD, ELIBSCN, ELIBMAX, ELIBEXEC, EILSEQ, ERESTART,
   ESTRPIPE, EUCLEAN, ENOTNAM, ENAVAIL, EISNAM, EREMOTEIO, ENOMEDIUM,
   EMEDIUMTYPE, ECANCELED, ENOKEY, EKEYEXPIRED, EKEYREVOKED, EKEYREJECTED,
   EOWNERDEAD, ENOTRECOVERABLE, EVSTYPE, EVSACCESS, EVSMODE, EVSOPEN,
   EVSERROR,
};

static int is_known(int n)
{
    unsigned i;

    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (known[i] == n) return 1;
    return 0;
}

/* #420: all values the library defines have a text of their own, the gaps
   share "Unknown error", and no int at all gets a null pointer or "". */
static void sweep(void)
{
    unsigned i;
    int n, nullish = 0, texts = 0, gaps = 0, wrong = 0;
    long v;

    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        const char *s = strerror(known[i]);

        if (s == NULL || s[0] == 0 || __builtin_strcmp(s, "Unknown error") == 0) {
            printf("  FAIL  errno %d has no text of its own\n", known[i]);
            bad++;
        } else texts++;
    }
    for (n = 1; n <= ENOTRECOVERABLE; n++) {
        const char *s = strerror(n);

        if (is_known(n)) continue;
        gaps++;
        if (s == NULL || __builtin_strcmp(s, "Unknown error") != 0) {
            printf("  FAIL  errno %d is not defined but has a text (%s)\n", n,
                   s ? s : "(null)");
            wrong++;
        }
    }
    for (v = -70000L; v <= 70000L; v++) {
        const char *s = strerror((int)v);

        if (s == NULL || s[0] == 0) nullish++;
    }
    if (strerror(INT_MIN) == NULL || strerror(INT_MAX) == NULL ||
        strerror(INT_MIN)[0] == 0 || strerror(INT_MAX)[0] == 0) nullish++;
    printf("  %s  %d errno values have their own text\n", texts == (int)(sizeof(known) / sizeof(known[0])) ? "ok  " : "FAIL", texts);
    printf("  %s  %d undefined values 1..%d are \"Unknown error\"\n", wrong ? "FAIL" : "ok  ", gaps, ENOTRECOVERABLE);
    printf("  %s  no null or empty text for -70000..70000, INT_MIN, INT_MAX\n", nullish ? "FAIL" : "ok  ");
    if (wrong || nullish) bad++;
    if (__builtin_strcmp(strerror(0), "No Error") != 0) { printf("  FAIL  strerror(0)\n"); bad++; }
}

static void want(int n, const char *expect)
{
    const char *got = strerror(n);
    int ok = got != NULL && __builtin_strcmp(got, expect) == 0;

    printf("  %s  strerror(%d) = \"%s\"\n", ok ? "ok  " : "FAIL", n,
           got ? got : "(null)");
    if (!ok) bad++;
}

int main(void)
{
    want(EIO, "I/O error");
    want(ENOTRECOVERABLE, "State not recoverable");
    want(EVSTYPE, "Invalid VSAM type");
    want(EVSERROR, "VSAM error occurred");
    want(103, "Unknown error");
    want(11, "Unknown error");
    want(132, "Unknown error");
    want(200, "Unknown error");
    want(-1, "Unknown error");
    sweep();
    printf("tststrer: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
