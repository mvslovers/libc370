#include <stddef.h>

#pragma linkage(__aopen, OS)
void *__aopen(const char *ddname, int *mode, int *recfm,
              int *lrecl, int *blksize, void **asmbuf, const char *member);
#pragma linkage(__aread, OS)
int __aread(void *handle, void *buf, size_t *len);
#pragma linkage(__awrite, OS)
int __awrite(void *handle, unsigned char **buf, size_t *sz);
#pragma linkage(__aclose, OS)
void __aclose(void *handle);
#pragma linkage(__getclk, OS)
unsigned int __getclk(void *buf);
#pragma linkage(__gettz, OS)
int __gettz(void);
#pragma linkage(__getm, OS)
void *__getm(size_t sz);
#pragma linkage(__freem, OS)
void __freem(void *ptr);

#pragma linkage(__dynal, OS)
int __dynal(size_t ddn_len, char *ddn, size_t dsn_len, char *dsn);

#pragma linkage(__idcams, OS)
int __idcams(size_t len, char *data);   /* non-reentrant assembler subroutine, Yick! */

int idcams(const char *fmt, ...);   /* reentrant C function, LINKs to IDCAMS external program */


#pragma linkage(__system, OS)
#ifdef MUSIC
int __system(int len, const char *command);
int __textlc(void);
#else
int __system(int req_type,
             size_t pgm_len,
             char *pgm,
             size_t parm_len,
             char *parm);
#endif

/* SVC 99 (dynamic allocation) is an MVS service, so it belongs outside the
   MUSIC guard it used to sit in; the caller builds the request block.  No
   linkage pragma: @@SVC99 takes the standard OS parameter list that cc370
   already builds for any call, which is what the callers in this library have
   been getting all along from the implicit declaration. */
int __svc99(void *rb);
