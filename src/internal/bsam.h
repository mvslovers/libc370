#ifndef SRC_INTERNAL_BSAM_H
#define SRC_INTERNAL_BSAM_H
/* src/internal/bsam.h - internal: libc370's BSAM layer under stdio, and system().
**
** libc370 2.0 splits this out of mvssupa.h (#256).
*/

#include <stddef.h>

#pragma linkage(__aopen, OS)
void *__aopen(const char *ddname, int *mode, int *recfm,
              int *lrecl, int *blksize, void **asmbuf, const char *member);
#pragma linkage(__aread, OS)
int __aread(void *handle, void *buf, size_t *len);
#pragma linkage(__awrite, OS)
int __awrite(void *handle, unsigned char **buf, size_t *sz);
/* __aclose() - write the final block, CLOSE, free the work area.  0, or
   the rc of the final block's write as __awrite() gives it: 12 out of
   space, 8 I/O error (#182).  The handle is gone either way. */
#pragma linkage(__aclose, OS)
int __aclose(void *handle);
/* __adisc() - discard whatever output is pending in the DCB work area, so
   that the next __aclose() writes nothing (#168).  Clears IOFLDATA, IOFLSDW,
   BUFFCURR and KEPTREC; issues no MVS service. */
#pragma linkage(__adisc, OS)
void __adisc(void *handle);

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

#endif /* SRC_INTERNAL_BSAM_H */
