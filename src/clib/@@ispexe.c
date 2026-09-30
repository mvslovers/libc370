#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/subsys.h>
#include <ext/array.h>        /* dynamic array prototypes     */
#include <mvs/ispf.h>		/* ISPF prototypes				*/
#include <mvs/link.h>		/* __link()						*/
#include <mvs/tso.h>		/* tsocmd()						*/
#include <ibm/mvs/ikjcppl.h>		/* CPPL typedef 				*/
#include <ibm/mvs/ikject.h>			/* ECT typedef					*/

int ispexec(const char *fmt, ...)
{
	int			rc 	= 20;
    va_list     list;
    char 		buf[256];

	va_start(list, fmt);
	vsprintf(buf, fmt, list);
	va_end(list);

	rc = tsocmdf("ISPEXEC", buf);
	return rc;
}
