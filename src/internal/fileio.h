#ifndef SRC_INTERNAL_FILEIO_H
#define SRC_INTERNAL_FILEIO_H
/* fileio.h - internal: libc370's stdio below the ISO functions.
**
** libc370 2.0 splits this out of clibio.h (#256).
*/

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

/* return name of function that called the caller of __caller() */
extern char *   __caller(char   *caller);

/* the following are functions that should only be called while
** holding a lock on the file handle.
*/
extern int      __fflush(FILE *fp);
extern int      __fflnl(FILE *fp);  /* '\n' on a text stream: always a record */
extern int      __fpswt(FILE *fp, int out); /* turn a '+' stream's DCB (#189) */
extern int      __fpupc(FILE *fp, int c);   /* a byte written in place (#189) */
extern int      __fgetc(FILE *fp);
extern char *   __fgets(char *s, int n, FILE *fp);
extern int      __fputc(int c, FILE *fp);
extern int      __fputs(const char *s, FILE *fp);
extern size_t   __fread(void *ptr, size_t size, size_t nmemb, FILE *fp);
extern FILE *   __reopen(const char *fn, const char *mode, FILE *fp);
extern int      __fseek(FILE *fp, long int offset, int whence);
extern size_t   __fwrite(const void *vptr, size_t size, size_t nmemb, FILE *fp);

/* __fpterm() - the teardown tail shared by fclose() and __fabandon(): drop
 * the buffer, unallocate a dynamically allocated DD, remove the FILE from
 * grt->grtfile, free it, and - when unlk is nonzero - release the FILE lock.
 * Returns __fpfree()'s rc, 0 when there was no DD to free. */
extern int      __fpterm(FILE *fp, int unlk);
extern int      __fpfree(FILE *fp);

/* __flock() - take the FILE lock for one stdio call (#453, #470).  Returns 1
 * when it took the lock, so the caller releases it with unlock(fp, 0); 0 when
 * the caller already holds it, or when no other task of the job step can run
 * C code on the stream - this task has no subtask and no task above it is a
 * C task.  The ENQ/DEQ pair is 99% of a fgetc()/fputc() call. */
extern int      __flock(FILE *fp);
extern int      __fptmp(FILE *fp);
/* __fpput() - open fp as "*PUTLINE": a stream that writes each line through
 * PUTLINE to the TSO terminal monitor program, ECT and UPT found through
 * the LWA (#463).  1 with errno ENODEV when there is no TMP. */
extern int      __fpput(FILE *fp);
/* __fpget() - open fp as "*GETLINE": a stream that reads each line through
 * GETLINE from the TMP - SYSTSIN in a batch TMP, the terminal in the
 * foreground (#467).  1 with errno ENODEV when there is no TMP. */
extern int      __fpget(FILE *fp);

/* the formatting engines behind the printf and scanf families: output goes
 * to fq, or to s when fq is NULL; input comes from fp, or from s when fp is NULL */
extern int      vvprintf(const char *format, va_list arg, FILE *fq, char *s);
extern int      vvscanf(const char *format, va_list arg, FILE *fp, const char *s);

/* __ddbusy() - would this OPEN be the second concurrent DCB on a spool
 * data set?  JES2 refuses that with ABEND S013-C0 (#184); this is what
 * lets fopen() answer NULL + EBUSY instead of dying inside OPEN. */
extern int __ddbusy(FILE *fp);

#endif /* SRC_INTERNAL_FILEIO_H */
