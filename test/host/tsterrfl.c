/*
 * tsterrfl.c - libc370 #149: a stream that has failed fails fast.
 *
 * ISSUE #149 asked for two things and got the premise of one of them
 * wrong.  clearerr() has been in the tree since the initial commit
 * (48111ed, 2024-09-12), so there was nothing to write; what was missing
 * is that NOTHING in stdio ever looked at _FILE_FLAG_ERROR.  __fgetc()
 * and __fread() tested only _FILE_FLAG_EOF, and __fwrite()/__fputc()
 * tested neither - so after a failed write the bytes were still accepted
 * into the FILE buffer, flushed into a WRITE that failed again, and
 * dropped at @@fflush.c's reset: label.
 *
 * Measured on mvsdev JOB00252 (test/mvs/tstnospc.c): after one ENOSPC,
 * 46 of the next 50 fwrite() calls returned the full 80 bytes and not one
 * of those records reached the disk.  The caller heard about 4 of 50.
 *
 * The fix guards every entry that can reach the access method and reports
 * the right errno from _FILE_FLAG_ENOSPC, a new flag bit that records
 * WHICH error set _FILE_FLAG_ERROR - the FILE keeps no errno of its own
 * and @@AWRITE clears IOSFLAGS before it returns, so without the bit a
 * refused call could only leave a stale value standing.
 *
 * This test compiles the REAL @@fflush.c, @@fwrite.c, @@fputc.c,
 * @@fgetc.c, @@fread.c, clearerr.c, feof.c and ferror.c against shims for
 * __awrite()/__aread() that return a programmable rc and COUNT their
 * calls.  The count is what makes fail-fast testable at all: a guard that
 * works is one where the access method is never reached.
 *
 * CHECKS - write side
 *   (1) a clean record write reaches __awrite()            control
 *   (2) rc=12 sets ERROR + ENOSPC and errno ENOSPC
 *   (3) the next fwrite() returns 0 and does NOT call __awrite()   RED
 *   (4) ...and reports ENOSPC, not a stale errno                   RED
 *   (5) rc=8 sets ERROR without ENOSPC, errno EIO
 *   (6) the next fwrite() returns 0 and reports EIO                RED
 *   (7) __fputc() after an error returns EOF, no __awrite()        RED
 *       (fprintf/fputs/puts reach the DCB through here, not __fwrite)
 *   (8) clearerr() lifts both bits and writing resumes
 *   (9) __fflush() on an errored stream discards the buffer and
 *       returns without re-driving the write                       RED
 * CHECKS - read side
 *  (10) a clean read reaches __aread()                     control
 *  (11) a read error sets ERROR and errno EIO
 *  (12) the next fread() returns 0 and does NOT call __aread()     RED
 *  (13) __fgetc() likewise returns EOF with no __aread()           RED
 *  (14) clearerr() lifts it and reading resumes
 * CHECKS - the macros
 *  (15) the ferror() MACRO returns 1, not the raw flag 2           RED
 *  (16) the feof() MACRO returns 1
 *  (17) macro and function forms agree
 *
 * BUILD / RUN (host, from test/host; same flag recipe as tstfcls.c):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -o t tsterrfl.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
/* libc370's <string.h> defines memset() as a static __inline holding an
   S/370 MVCL, and -D'__asm__(...)=' does not neutralise the __volatile__
   form of it - the host assembler sees the MVCL and stops.  Earlier host
   tests worked around that by shimming whichever TU called memset();
   claiming clibstr.h's include guard here and declaring the handful of
   functions the TUs under test actually use lets them all be compiled for
   real instead.  The declarations match the host libc, which supplies
   them at link time. */
#define CLIBSTR_H
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
#include <osdcb.h>

/* libc370 reaches errno through __errno(); on MVS it is per-task storage
   off the GRT.  One int is all this test needs. */
static int  errno_cell;
int *__errno(void) { return &errno_cell; }

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- the macro forms, captured before feof.c/ferror.c #undef them ----- */

static int macro_ferror(FILE *fp) { return ferror(fp); }
static int macro_feof(FILE *fp)   { return feof(fp); }

/* ---- shims for the access method -------------------------------------- */

static int  awrite_rc;          /* what __awrite() will answer             */
static int  awrite_calls;
static int  aread_rc;           /* >0 error, <0 eof, 0 data                */
static int  aread_calls;
static unsigned char aread_data[80];

int __awrite(void *handle, unsigned char **buf, size_t *sz)
{
    (void)handle; (void)buf; (void)sz;
    awrite_calls++;
    return awrite_rc;
}

int __aread(void *handle, void *buf, size_t *len)
{
    (void)handle;
    aread_calls++;
    if (aread_rc == 0) {
        *(unsigned char **)buf = aread_data;
        *len = sizeof(aread_data);
    }
    return aread_rc;
}

/* ---- the real thing ---------------------------------------------------- */

#include "../../src/clib/@@fflush.c"
#undef begwrite
#undef finwrite
#include "../../src/clib/@@fwrite.c"
#include "../../src/clib/@@fputc.c"
/* @@fgetc.c calls isspace() without including <ctype.h>; cc370 tolerates
   the implicit declaration, a modern host compiler does not. */
#include <ctype.h>
#include "../../src/clib/@@isbuf.c"   /* isspace() is a table macro */
#include "../../src/clib/@@fgetc.c"
#include "../../src/clib/@@fread.c"
#include "../../src/clib/clearerr.c"
#include "../../src/clib/feof.c"
#include "../../src/clib/ferror.c"

/* ---- a FILE the shims can serve --------------------------------------- */

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
    f.endbuf  = filebuf + 80;
    f.lrecl   = 80;
    f.blksize = 800;
    f.recfm   = _FILE_RECFM_F;
    f.ungetch = -1;
    f.flags   = flags;
    awrite_calls = aread_calls = 0;
    errno = 0;
}

#define REC (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_BINARY \
             | _FILE_FLAG_RECORD)
#define BYT (_FILE_FLAG_OPEN | _FILE_FLAG_WRITE | _FILE_FLAG_BINARY)
#define RDR (_FILE_FLAG_OPEN | _FILE_FLAG_READ  | _FILE_FLAG_BINARY \
             | _FILE_FLAG_RECORD)

int main(void)
{
    unsigned char   rec[80];
    unsigned char   in[80];
    size_t          got;
    int             c;
    int             err;

    memset(rec, 'X', sizeof(rec));
    printf("=== tsterrfl: #149 - a failed stream fails fast ===\n\n");

    /* ---- write side, record mode -------------------------------------- */
    printf("write side\n");

    mkfile(REC);
    awrite_rc = 0;
    got = __fwrite(rec, 1, sizeof(rec), &f);
    check(got == 1 && awrite_calls == 1 && !(f.flags & _FILE_FLAG_ERROR),
          "(1) a clean record write reaches __awrite()");

    awrite_rc = 12;                     /* the x37 exit (#176)             */
    errno = 0;
    got = __fwrite(rec, 1, sizeof(rec), &f);
    check(got == 0
          && (f.flags & _FILE_FLAG_ERROR)
          && (f.flags & _FILE_FLAG_ENOSPC)
          && errno == ENOSPC,
          "(2) rc=12 sets ERROR + ENOSPC and errno ENOSPC");

    awrite_calls = 0;
    errno = 0;
    got = __fwrite(rec, 1, sizeof(rec), &f);
    check(got == 0 && awrite_calls == 0,
          "(3) the next fwrite() returns 0 without calling __awrite()");
    check(errno == ENOSPC,
          "(4) ...and reports ENOSPC, not a stale errno");

    mkfile(REC);
    awrite_rc = 8;                      /* SYNAD, a real device error      */
    errno = 0;
    got = __fwrite(rec, 1, sizeof(rec), &f);
    check(got == 0
          && (f.flags & _FILE_FLAG_ERROR)
          && !(f.flags & _FILE_FLAG_ENOSPC)
          && errno == EIO,
          "(5) rc=8 sets ERROR without ENOSPC, errno EIO");

    awrite_calls = 0;
    errno = 0;
    got = __fwrite(rec, 1, sizeof(rec), &f);
    check(got == 0 && awrite_calls == 0 && errno == EIO,
          "(6) the next fwrite() returns 0 and reports EIO");

    /* ---- write side, the byte path fprintf()/fputs() take -------------- */

    mkfile(BYT);
    awrite_rc = 12;
    memcpy(f.buf, rec, 80);
    f.upto = f.buf + 80;                /* a full buffer, ready to flush   */
    err = __fflush(&f);
    check(err == 12
          && (f.flags & _FILE_FLAG_ENOSPC)
          && f.upto == f.buf,
          "(7a) __fflush() records ENOSPC and resets the buffer");

    awrite_calls = 0;
    errno = 0;
    c = __fputc('A', &f);
    check(c == EOF && awrite_calls == 0 && errno == ENOSPC,
          "(7) __fputc() after an error returns EOF, no __awrite()");

    awrite_rc = 0;
    clearerr(&f);
    awrite_calls = 0;
    check(!(f.flags & _FILE_FLAG_ERROR) && !(f.flags & _FILE_FLAG_ENOSPC),
          "(8a) clearerr() lifts ERROR and ENOSPC");
    c = __fputc('A', &f);
    check(c == 'A' && !(f.flags & _FILE_FLAG_ERROR),
          "(8) ...and writing resumes");

    mkfile(BYT);
    f.flags |= _FILE_FLAG_ERROR | _FILE_FLAG_ENOSPC;
    memcpy(f.buf, rec, 80);
    f.upto = f.buf + 80;
    awrite_calls = 0;
    err = __fflush(&f);
    check(err != 0 && awrite_calls == 0 && f.upto == f.buf,
          "(9) __fflush() on an errored stream discards, does not re-drive");

    /* ---- read side ----------------------------------------------------- */
    printf("\nread side\n");

    mkfile(RDR);
    aread_rc = 0;
    got = __fread(in, sizeof(in), 1, &f);
    check(got == 1 && aread_calls == 1 && !(f.flags & _FILE_FLAG_ERROR),
          "(10) a clean read reaches __aread()");

    aread_rc = 8;                       /* SYNAD on input (#147)           */
    errno = 0;
    got = __fread(in, sizeof(in), 1, &f);
    check(got == 0 && (f.flags & _FILE_FLAG_ERROR) && errno == EIO,
          "(11) a read error sets ERROR and errno EIO");

    aread_calls = 0;
    errno = 0;
    got = __fread(in, sizeof(in), 1, &f);
    check(got == 0 && aread_calls == 0 && errno == EIO,
          "(12) the next fread() returns 0 without calling __aread()");

    f.flags &= (unsigned short)~_FILE_FLAG_RECORD;
    aread_calls = 0;
    errno = 0;
    c = __fgetc(&f);
    check(c == EOF && aread_calls == 0 && errno == EIO,
          "(13) __fgetc() likewise returns EOF with no __aread()");

    /* Back to the record path for the recovery check: on the byte path
       __fgetc() would serve whatever is still in the FILE buffer without
       going near __aread(), which proves nothing about the guard. */
    aread_rc = 0;
    f.flags |= _FILE_FLAG_RECORD;
    clearerr(&f);
    aread_calls = 0;
    got = __fread(in, sizeof(in), 1, &f);
    check(got == 1 && aread_calls == 1 && !(f.flags & _FILE_FLAG_ERROR),
          "(14) clearerr() lifts it and reading resumes");

    /* ---- the macros ---------------------------------------------------- */
    printf("\nmacros\n");

    mkfile(BYT);
    f.flags |= _FILE_FLAG_ERROR;
    check(macro_ferror(&f) == 1,
          "(15) the ferror() macro returns 1, not the raw flag 2");
    f.flags |= _FILE_FLAG_EOF;
    check(macro_feof(&f) == 1,
          "(16) the feof() macro returns 1");
    check(macro_ferror(&f) == ferror(&f) && macro_feof(&f) == feof(&f),
          "(17) macro and function forms agree");

    printf("\n=== tsterrfl: %d check(s) failed ===\n", bad);
    return bad ? 1 : 0;
}
