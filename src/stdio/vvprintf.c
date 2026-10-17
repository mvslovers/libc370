/* VVPRINTF.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <mvs/lock.h>

extern void
__dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
         char *result, size_t rsize);

/* Where the output goes: the stream fq through putcf and putsf, or the
   string s when fq is NULL.  Built on the caller's stack, so nothing here
   is static (#577). */
typedef struct sink {
    FILE    *fq;
    char    *s;
    int     (*putcf)(int c, FILE *fp);
    int     (*putsf)(const char *s, FILE *fp);
} SINK;

/* one character */
__asm__("\n&FUNC    SETC 'out1'");
static void
out1(SINK *k, int ch)
{
    if (k->fq == NULL) *k->s++ = (char)ch;
    else k->putcf(ch, k->fq);
}

/* a string of len bytes, NUL-terminated: __fputs, not fputs, since
   vvprintf holds the FILE lock (#145) */
__asm__("\n&FUNC    SETC 'outs'");
static void
outs(SINK *k, const char *p, size_t len)
{
    if (k->fq == NULL) {
        memcpy(k->s, p, len);
        k->s += len;
    }
    else {
        k->putsf(p, k->fq);
    }
}

/* %d without flags, width or precision */
__asm__("\n&FUNC    SETC 'outdec'");
static int
outdec(SINK *k, int vint)
{
    char        digits[12];
    char        *nptr   = digits;
    unsigned    uvint   = (vint < 0) ? -vint : vint;
    int         count   = 0;

    do {
        *nptr++ = (char)('0' + uvint % 10);
        uvint /= 10;
    } while (uvint > 0);

    if (vint < 0) *nptr++ = '-';

    do {
        nptr--;
        out1(k, *nptr);
        count++;
    } while (nptr != digits);

    return count;
}

/* One conversion; *formt is at the character after the '%' and is left at
   the last character of the specification.  Returns the characters it
   produced; done is what the line has produced so far, for %n. */
__asm__("\n&FUNC    SETC 'onespec'");
static int
onespec(SINK *k, const char **formt, va_list *arg, int done)
{
    const char  *format = *formt;
    const char  *vcptr;
    size_t      len;
    int         extraCh;
    char        numbuf[96];      /* %f of any HFP value (#222) */

    if (*format == 'd') {
        return outdec(k, va_arg(*arg, int));
    }
    if (*format != 0 && strchr("eEgGfF", *format) != NULL) {
        __dblcvt(va_arg(*arg, double), *format, 0, 6, numbuf,
                 sizeof(numbuf));   /* 'e','f' etc. */
        len = strlen(numbuf);
        outs(k, numbuf, len);
        return (int)len;
    }
    if (*format == 's') {
        vcptr = va_arg(*arg, const char *);
        if (vcptr == NULL) vcptr = "(null)";
        len = strlen(vcptr);
        outs(k, vcptr, len);
        return (int)len;
    }
    if (*format == 'c') {
        out1(k, va_arg(*arg, int));
        return 1;
    }
    if (*format == 'n') {
        *va_arg(*arg, int *) = done;
        return 0;
    }
    if (*format == '%') {
        out1(k, '%');
        return 1;
    }

    /* the string sink here is vsprintf()'s, which is unbounded by
       definition - INT_MAX says so explicitly now that __examin honours
       its budget (#128) */
    extraCh = __examin(formt, k->fq, k->s, arg, INT_MAX, done, k->putcf);
    if (k->s != NULL) k->s += extraCh;
    return extraCh;
}

int
vvprintf(const char *format, va_list arg, FILE *fq, char *s,
         int (*putcf)(int c, FILE *fp), int (*putsf)(const char *s, FILE *fp))
{
    SINK        k;
    int         chcount = 0;
    int         owned   = 0;

    k.fq    = fq;
    k.s     = s;
    k.putcf = putcf;
    k.putsf = putsf;

    /* lock file handle (required for __fputc() call); rc=8 means the
       caller already holds it, and then it is not ours to release (#145) */
    if (fq) owned = __flock(fq);

    /* a stream not open for writing takes nothing (#385) */
    if (fq && !(fq->flags & _FILE_FLAG_WRITE)) {
        errno = EBADF;
        chcount = -1;
        goto quit;
    }

    for (; *format != '\0'; format++) {
        if (*format == '%') {
            format++;
            chcount += onespec(&k, &format, &arg, chcount);
        }
        else {
            out1(&k, *format);
            chcount++;
        }
    }

    /* an output error is a negative result, not the count (#385).  A
       failed flush sets the error indicator, also one inside __examin(),
       and a stream that had it already refused every byte.  A write that
       is still in the buffer fails only at a later flush. */
    if (fq && (fq->flags & _FILE_FLAG_ERROR)) chcount = -1;

quit:
    if (owned) unlock(fq,0);

    return (chcount);
}
