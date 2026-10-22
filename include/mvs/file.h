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
 * Nothing more is written, and a member of a PDS is not stored: a new member
 * does not appear and an existing one keeps its old content (#705).  Also
 * for a FILE whose fclose() abended inside CLOSE (SB14 when the directory has
 * no room) and was recovered.
 *
 *    0   CLOSE completed, the DD released
 *   >0   CLOSE abended; the 0x00sssuuu code (an SB14 reads 0x00B14000).
 *        The FILE is gone and the DD released all the same (#705)
 *   -1   not a FILE
 *   -2   the DD could not be unallocated and is still held, whether or not
 *        CLOSE abended
 *   -3   ESTAE CREATE failed, CLOSE was not attempted, the FILE is untouched
 */
extern int __fabandon(FILE *fp);

/* __fx37() which x37 abend an out-of-space write stood for (#539).
 *
 * A write that runs out of space fails with ENOSPC instead of abending
 * B37/D37/E37 (#176).  For a caller that wants to raise the abend itself:
 * the completion code the system would have issued, in try()'s 0x00sssuuu
 * form with the reason code in the last digits.
 *
 * The system does not report the code to the library; it is derived the way
 * end-of-volume decides it, and checked against the abends MVS issues
 * without the library's exit: no secondary space quantity (from the DD, or
 * from the data set when the DD gives none) is D37-04; secondary space used
 * up is B37-04 for a sequential data set and E37-04 for a partitioned one.
 * A data set that spans volumes can end with codes not derived here.  Call
 * it while the stream is open, after the failed write.
 *
 *    0           the stream has not run out of space, or is not a FILE
 *    0x00D37004  D37-04
 *    0x00B37004  B37-04
 *    0x00E37004  E37-04 (in practice not returned: MVS abends a partitioned
 *                data set whose secondary space is used up before the
 *                library can turn it into ENOSPC)
 */
extern unsigned __fx37(FILE *fp);

#endif /* MVS_FILE_H */
