#ifndef OSIO_H
#define OSIO_H
#include <sys/_cc370.h>

#include "ibm/mvs/dcbd.h"
#include "ibm/mvs/iezdeb.h"
#include "ibm/mvs/ihadecb.h"
#include "ibm/mvs/ieziob.h"
#include "ibm/mvs/iefjfcbn.h"

/* - - - - B S A M - - - - */
/* allocate and initialize DCB for BSAM access */
extern DCB *    osbdcb(const char *ddname, EXITLIST *exitlist);

/* open DCB for "read", "write", "load" or "update" access mode */
extern int      osbopen(DCB *dcb, int typej, const char *mode);

/* close DCB with/without option and free of DCB storage
** option can be NULL, "reread", "leave", "rewind", "free", "disp".
** freedcb can be 0 (false) or any value (true) to free DCB storage.
** type_t is for close,type=t and should be 0 for normal close.
*/
extern void     osbclose(DCB *dcb, const char *option, int freedcb, int type_t);

/* read a block from DCB dataset - does not wait for completion */
extern void     osread(DECB *decb, DCB *dcb, void *buf, int length);
extern void     osbread(DECB *decb, DCB *dcb, void *buf, int length) asm("OSREAD");

/* write a block to DCB dataset - does not wait for completion */
extern int      oswrite(DECB *decb, DCB *dcb, void *buf, int length);
extern int      osbwrite(DECB *decb, DCB *dcb, void *buf, int length) asm("OSWRITE");

/* check last read or write for completion: 0 done, OSIO_EOD end of
** data, OSIO_IOERR an I/O error, otherwise the abend that ended the CHECK */
extern int      oscheck(DECB *decb);

/* return codes of oscheck() besides 0 and an abend code */
#define OSIO_EOD    4   /* end of data: CHECK took the library's EODAD */
#define OSIO_IOERR  8   /* an I/O error: CHECK took a SYNAD */

/* - - - - E X C P - - - - */
/* allocate and initialize DCB for EXCP access */
extern DCB *    osxdcb(const char *ddname, EXITLIST *exitlist);

/* open DCB for EXCP access, for UPDAT */
extern int      osxopen(DCB *dcb, int typej);

/* open DCB for EXCP access: mode NULL or "u" for UPDAT, "r" for INPUT */
extern int      osxopenm(DCB *dcb, int typej, const char *mode);

/* given a DCB, number of blocks per track, and a block number,
** calculate 8 byte MBBCCHHR value.
** returns record number or -1 on error.
*/
extern int      osxcalc(DCB *dcb, unsigned blkstrk, unsigned block,
                char *mbbcchhr);

/* read one block from dataset by block number */
extern int      osxread(DCB *dcb, unsigned blkstrk, unsigned block,
                void *buf, char *sense);

/* write one block to dataset by block number */
extern int      osxwrite(DCB *dcb, unsigned blkstrk, unsigned block,
                void *buf, char *sense);

/* close EXCP DCB created by osxdcb()
** freedcb can be 0 (false) or any value (true) to free DCB storage.
*/
extern void     osxclose(DCB *dcb, int freedcb);


/* - - - - B D A M - - - - */
/* allocate and initialize DCB for BDAM access (read+write) by relative block number */
extern DCB *    osddcb(const char *ddname);

/* open DCB for BDAM UPDATE access (read+write) */
extern int      osdopen(DCB *dcb, int typej);

/* open DCB for BDAM access: mode NULL or "u" for UPDAT, "r" for INPUT */
extern int      osdopenm(DCB *dcb, int typej, const char *mode);

/* read one block from dataset by relative block number */
extern int      osdread(DECB *decb, DCB *dcb, void *buf, int length, unsigned block);

/* write one block to dataset by relative block number */
extern int      osdwrite(DECB *decb, DCB *dcb, void *buf, int length, unsigned block);

/* close BDAM DCB created by osddcb()
** freedcb can be 0 (false) or any value (true) to free DCB storage.
*/
extern void     osdclose(DCB *dcb, int freedcb);

#endif
