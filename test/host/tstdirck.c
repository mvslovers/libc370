/*
 * tstdirck.c - libc370 #189 step 1: a stream refuses the wrong direction.
 *
 * ISSUE #189: a read on a stream opened for output drove __aread() against
 * the output DCB - ABEND S400 on the READ, then B14-10 when the damaged DCB
 * was closed (IEC217I, measured by brexx370 on MVS/CE).  __fgetc() had no
 * check for _FILE_FLAG_READ at all, and __fread() in record mode calls
 * __aread() directly.  The mirror: __fwrite() in record mode calls
 * __awrite() on an input DCB, and __fputc() on a read-only stream returned
 * the character - success - and set nothing.
 *
 * The fix: every entry that can reach the access method checks the
 * direction first and answers EOF / NULL / 0 with errno = EBADF.  It does
 * NOT set the error indicator: since #149 that refuses every later call, so
 * one misdirected read would kill a healthy write stream.
 *
 * This test compiles the REAL @@fflush.c, @@fwrite.c, @@fputc.c,
 * @@fgetc.c, @@fread.c and @@fgets.c against access-method shims that
 * count their calls - a check that works is one where they are never
 * reached.
 *
 * CHECKS - reading a write stream
 *   (1) __fgetc() -> EOF, errno EBADF, no __aread()                    RED
 *   (2) __fgets() -> NULL, errno EBADF, no __aread()                   RED
 *   (3) __fread() text mode -> 0, errno EBADF, no __aread()            RED
 *   (4) __fread() record mode -> 0, errno EBADF, no __aread()          RED
 *   (5) the stream is not poisoned: no error indicator, and the next
 *       write still reaches __awrite()
 * CHECKS - writing a read stream
 *   (6) __fputc() -> EOF, errno EBADF, no __awrite()                   RED
 *   (7) __fwrite() record mode -> 0, errno EBADF, no __awrite()        RED
 *   (8) __fwrite() text mode -> 0, errno EBADF, no __awrite()          RED
 *   (9) the stream is not poisoned: the next read still works
 * CHECKS - regression
 *  (10) a write stream writes, a read stream reads
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tsterrfl.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R/src/internal -o t tstdirck.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* string.h suppressed for the host, as in tsterrfl.c */
#define STRING_H
#include "src/internal/fileio.h"
#include <stddef.h>
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *t, const void *s, size_t n);
size_t  strlen(const char *s);
int     strcmp(const char *a, const char *b);
char   *strcpy(char *t, const char *s);
void   *memchr(const void *s, int c, size_t n);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ibm/mvs/dcbd.h>

static int  errno_cell;
int *__errno(void) { return &errno_cell; }

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- access-method shims: count, and bound the reads ------------------- */

static int              awrite_calls;
static int              aread_calls;
static unsigned char    aread_data[80];

int __awrite(void *handle, unsigned char **buf, size_t *sz)
{
    (void)handle; (void)buf; (void)sz;
    awrite_calls++;
    return 0;
}

int __aread(void *handle, void *buf, size_t *len)
{
    (void)handle;
    if (++aread_calls > 4) return -1;   /* EOF: a red case must not hang  */
    *(unsigned char **)buf = aread_data;
    *len = sizeof(aread_data);
    return 0;
}


/* __fpswt() turns a '+' stream round (#189).  Plain streams never reach
   it: fopen() gives a writer _FILE_FLAG_DCBOUT from the start, and the
   FILEs below are built the same way.  A call here is a test failure. */
static int fpswt_calls;
int __fpswt(FILE *fp, int out)
{
    (void)fp; (void)out;
    fpswt_calls++;
    printf("  !! __fpswt() reached from a plain stream\n");
    errno = EBADF;
    return -1;
}
int __fpupc(FILE *fp, int c)
{
    (void)fp; (void)c;
    fpswt_calls++;
    printf("  !! __fpupc() reached from a plain stream\n");
    errno = EBADF;
    return -1;
}

/* ---- the real thing ---------------------------------------------------- */

#include "../../src/stdio/@@fflush.c"
#undef begwrite
#undef finwrite
#include "../../src/stdio/@@fwrite.c"
#include "../../src/stdio/@@fputc.c"
#include <ctype.h>
#include "../../src/stdio/@@isbuf.c"   /* isspace() is a table macro */
#include "../../src/stdio/@@fgetc.c"
#include "../../src/stdio/@@fread.c"
#include "../../src/stdio/@@fgets.c"

/* ---- a FILE the shims can serve ---------------------------------------- */

static DCB              fake_dcb;
static unsigned char    filebuf[128];
static unsigned char    asmbuf[128];
static FILE             f;

static void mkfile(unsigned short flags)
{
    memset(&f, 0, sizeof(f));
    memcpy(f.eye, _FILE_EYE, sizeof(f.eye) - 1);
    memset(&fake_dcb, 0, sizeof(fake_dcb));
    fake_dcb.dcbdevt = 0x20;            /* anything but DCBDVTRM           */
    f.dcb     = &fake_dcb;
    f.asmbuf  = asmbuf;
    f.buf     = filebuf;
    f.upto    = filebuf;
    /* __fpopen() sets upto/endbuf up for writing only; a read stream
       starts with an empty buffer */
    f.endbuf  = (flags & _FILE_FLAG_WRITE) ? filebuf + 80 : filebuf;
    f.lrecl   = 80;
    f.blksize = 800;
    f.recfm   = _FILE_RECFM_F;
    f.ungetch = -1;
    f.flags   = flags;
    awrite_calls = aread_calls = 0;
    memset(aread_data, 'R', sizeof(aread_data));
    errno = 0;
}

#define TXW (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_DCBOUT)
#define TXR (_FILE_FLAG_OPEN | _FILE_FLAG_READ)
#define RCW (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_BINARY \
             | _FILE_FLAG_RECORD | _FILE_FLAG_DCBOUT)
#define RCR (_FILE_FLAG_OPEN | _FILE_FLAG_READ  | _FILE_FLAG_BINARY \
             | _FILE_FLAG_RECORD)

int main(void)
{
    char            line[100];
    unsigned char   rec[80];
    int             c;
    size_t          got;
    char            *s;

    memset(rec, 'X', sizeof(rec));
    printf("=== tstdirck: #189 - a stream refuses the wrong direction ===\n\n");

    printf("reading a write stream\n");

    /* (1) the S400 case itself */
    mkfile(TXW);
    __fputc('A', &f);                   /* stale bytes in the write buffer */
    errno = 0;
    c = __fgetc(&f);
    check(c == EOF && errno == EBADF && aread_calls == 0,
          "(1) __fgetc() -> EOF, EBADF, no __aread()");

    /* (2) */
    mkfile(TXW);
    errno = 0;
    s = __fgets(line, sizeof(line), &f);
    check(s == NULL && errno == EBADF && aread_calls == 0,
          "(2) __fgets() -> NULL, EBADF, no __aread()");

    /* (3) */
    mkfile(TXW);
    errno = 0;
    got = __fread(line, 1, 10, &f);
    check(got == 0 && errno == EBADF && aread_calls == 0,
          "(3) __fread() text mode -> 0, EBADF, no __aread()");

    /* (4) */
    mkfile(RCW);
    errno = 0;
    got = __fread(line, sizeof(line), 1, &f);
    check(got == 0 && errno == EBADF && aread_calls == 0,
          "(4) __fread() record mode -> 0, EBADF, no __aread()");

    /* (5) the refusal leaves the writer usable */
    mkfile(TXW);
    __fgetc(&f);
    __fputc('A', &f);
    __fputc('\n', &f);
    check(!ferror(&f) && !feof(&f) && awrite_calls == 1,
          "(5) no error or EOF indicator; the next write reaches __awrite()");

    printf("writing a read stream\n");

    /* (6) */
    mkfile(TXR);
    errno = 0;
    c = __fputc('A', &f);
    check(c == EOF && errno == EBADF && awrite_calls == 0,
          "(6) __fputc() -> EOF, EBADF, no __awrite()");

    /* (7) */
    mkfile(RCR);
    errno = 0;
    got = __fwrite(rec, sizeof(rec), 1, &f);
    check(got == 0 && errno == EBADF && awrite_calls == 0,
          "(7) __fwrite() record mode -> 0, EBADF, no __awrite()");

    /* (8) */
    mkfile(TXR);
    errno = 0;
    got = __fwrite("AB\n", 1, 3, &f);
    check(got == 0 && errno == EBADF && awrite_calls == 0,
          "(8) __fwrite() text mode -> 0, EBADF, no __awrite()");

    /* (9) the refusal leaves the reader usable */
    mkfile(TXR);
    __fputc('A', &f);
    c = __fgetc(&f);
    check(c == 'R' && !ferror(&f) && aread_calls == 1,
          "(9) no error indicator; the next read reaches __aread()");

    printf("regression\n");

    /* (10) */
    mkfile(TXW);
    __fputc('A', &f);
    __fputc('\n', &f);
    c = awrite_calls;
    mkfile(TXR);
    check(c == 1 && __fgetc(&f) == 'R',
          "(10) a write stream writes, a read stream reads");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
