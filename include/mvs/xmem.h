#ifndef MVS_XMEM_H
#define MVS_XMEM_H
#include <sys/_cc370.h>
/* mvs/xmem.h - across address spaces: ASCB lookup, cross-memory POST.
**
** libc370 2.0 splits this out of clibos.h (#256).
*/

/* __ascb() get ASCB for ASID, or current ASCB if 0 */
void *__ascb(unsigned asid);

/* __xmpost() POST ECB with postcode in address space for ascb */
void __xmpost(void *ascb, void *ecb, unsigned postcode);

#endif /* MVS_XMEM_H */
