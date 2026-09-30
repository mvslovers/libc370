#ifndef MVS_IDCAMS_H
#define MVS_IDCAMS_H
/* mvs/idcams.h - run IDCAMS commands.
**
** libc370 2.0 splits this out of mvssupa.h (#256).
*/

#include <stddef.h>

#pragma linkage(__idcams, OS)
int __idcams(size_t len, char *data);   /* non-reentrant assembler subroutine, Yick! */

int idcams(const char *fmt, ...);   /* reentrant C function, LINKs to IDCAMS external program */

#endif /* MVS_IDCAMS_H */
