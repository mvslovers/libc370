#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <clib.h>
#include <clibssib.h>
#include <libc370/array.h>        /* dynamic array prototypes     */
#include <mvs/ispf.h>		/* ISPF prototypes				*/
#include <mvs/link.h>		/* __link()						*/
#include <mvs/tso.h>		/* tsocmd()						*/
#include <ibm/mvs/ikjcppl.h>		/* CPPL typedef 				*/
#include <ibm/mvs/ikject.h>			/* ECT typedef					*/

int isplink(const char *service_name, ...)
{
	int			rc 	= 20;
	int			i;
	void 		*r1;
	unsigned    *u;

	if (!service_name) {
		wtof("%s: Missing service name.", __func__);
		goto quit;
	}
	
	/* on entry, R11 points to service_name (parameter list) */
	__asm__("ST\t11,%0" : "=m"(r1));

	/* make sure we have a parameter address with a high order bit set */
	for(i=0, u=r1; i < 20; i++) {
		if (u[i] & 0x80000000) break;
	}
	if (i==20) {
		/* we didn't see the end of the parameters */
		wtof("%s: Missing hit order bit in plist.", __func__);
		goto quit;
	}

	__asm__("LR\t1,11           parameter list\n\t"
			"L\t15,=V(ISPLINK)\n\t"
			"BALR\t14,15          call ISPLINK\n\t"
			"ST\t15,%0      save return code" : "=m"(rc) );
quit:
	return rc;
}
