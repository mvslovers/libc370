#ifndef MVS_FILE_H
#define MVS_FILE_H
#include <sys/_cc370.h>
/* mvs/file.h - MVS extensions to a stdio FILE.
**
** libc370 2.0 splits this out of clibio.h (#256).
*/

#include <stdio.h>

/* __fabandon() close a FILE whose last write failed, WITHOUT flushing (#168).
 *
 * fclose() flushes first, so when the pending block is what could not be
 * written it re-drives the failing WRITE, abends inside the close, and the DD
 * stays allocated for the life of the job - the data set can then be deleted
 * neither by the program that created it nor by its user.  __fabandon()
 * discards the buffer instead, tells the DCB there is nothing pending, and
 * issues CLOSE under an ESTAE so a close that fails anyway is reported rather
 * than propagated.  The FILE is gone either way; do not use it again.
 *
 * For a caller whose ESTAE has just recovered an x37 on a data set it created
 * and now wants to scratch.  The library cannot detect the situation itself:
 * an ABEND is not a SYNAD condition, so _FILE_FLAG_ERROR is never set and the
 * FILE looks healthy.
 *
 *    0   nothing written, CLOSE completed, the DD released
 *   >0   CLOSE abended; the 0x00sssuuu code (a D37 reads 0x00D37000).  The
 *        FILE is gone, the DD is not - the failed CLOSE still holds it
 *   -1   not a FILE
 *   -2   CLOSE completed but the DD could not be unallocated
 *   -3   ESTAE CREATE failed, CLOSE was not attempted, the FILE is untouched
 */
extern int __fabandon(FILE *fp);

#endif /* MVS_FILE_H */
