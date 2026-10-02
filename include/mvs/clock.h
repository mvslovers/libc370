#ifndef MVS_CLOCK_H
#define MVS_CLOCK_H
#include <sys/_cc370.h>
/* mvs/clock.h - the TOD clock and the system time zone offset.
**
** libc370 2.0 splits this out of mvssupa.h (#256).
*/

#pragma linkage(__getclk, OS)
unsigned int __getclk(void *buf);
#pragma linkage(__gettz, OS)
int __gettz(void);

#endif /* MVS_CLOCK_H */
