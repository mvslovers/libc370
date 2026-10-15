/* @@FX37.C - which x37 abend a stream's out-of-space write stood for (#539)
**
** Since #176 an output stream that runs out of space fails the write with
** ENOSPC instead of letting MVS abend the program with B37, D37 or E37.  A
** language run time that wants the abend after all (PL/I ends a full data
** set with SD37-04) needs to know which one it would have been.
**
** The system does not tell: IFG0554T keeps its internal code in its own
** R12, and IFG019RA calls the x37 exit with R1 = DCB and the application's
** registers (RES09000).  The code is therefore derived here the way EOV
** decides it (IFG0554P, MVS 3.8j), and checked against what MVS itself
** issues without the exit (IEBDG, mvsdev JOB02342/JOB02343):
**
**   - no secondary quantity (JFCBSQTY zero, EDO03000/EDO03800) -> D37-04,
**     sequential and partitioned alike;
**   - secondary space used up on a partitioned data set (EDO03804: "no such
**     thing as multivolume PDS") -> E37-04;
**   - secondary space used up on a sequential one: EOV looks for another
**     volume, finds none, and IFG0554T ends it -> B37-04.  Measured with 16
**     extents - not E37, which the IFG0554P prologue suggests.
**
** The secondary quantity comes from the JFCB when the DD gives one, and
** from the format-1 DSCB otherwise - which is what OPEN's DSCB-to-JFCB merge
** hands EOV for a data set allocated without SPACE.  The organisation comes
** from the DSCB, or from a member name in the JFCB when the DSCB cannot be
** read.  A data set that spans volumes can end with codes not derived here
** (E37-04 at the last volume, E37-08).
**
** RETURNS
**    0           the stream has not run out of space (or is not a FILE)
**    0x00D37004  D37-04: no secondary space
**    0x00B37004  B37-04: sequential, the secondary space is used up
**    0x00E37004  E37-04: partitioned, the secondary space is used up
*/
#include <mvs/file.h>
#include <mvs/dd.h>
#include <mvs/dscb.h>
#include <string.h>

#define FX37_B37    0x00B37004u
#define FX37_D37    0x00D37004u
#define FX37_E37    0x00E37004u

static int nonzero3(const void *p)
{
    const unsigned char *b = p;

    return b[0] | b[1] | b[2];
}

unsigned
__fx37(FILE *fp)
{
    JFCB        jfcb;
    DSCB        dscb;
    int         sec;
    int         pds;

    if (!fp) return 0;
    if (strcmp(fp->eye, _FILE_EYE) != 0) return 0;
    if (!(fp->flags & _FILE_FLAG_ENOSPC)) return 0;

    memset(&jfcb, 0, sizeof(jfcb));
    if (__rdjfcb((DCB *)fp->dcb, &jfcb) != 0) return FX37_D37;

    memset(&dscb, 0, sizeof(dscb));
    if (__dscbdv(jfcb.jfcbdsnm, jfcb.jfcbvols, &dscb) == 0) {
        /* the DD's SPACE wins; without one OPEN took the DSCB's */
        sec = nonzero3(jfcb.jfcbsqty) || nonzero3(dscb.dscb1.scal3);
        pds = (dscb.dscb1.dsorg1 & DSGPO) != 0;
    } else {
        sec = nonzero3(jfcb.jfcbsqty);
        pds = jfcb.jfcbelnm[0] != ' ' && jfcb.jfcbelnm[0] != 0;
    }

    if (!sec) return FX37_D37;
    return pds ? FX37_E37 : FX37_B37;
}
