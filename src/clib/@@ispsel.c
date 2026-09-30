#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/subsys.h>
#include <libc370/array.h>        /* dynamic array prototypes     */
#include <mvs/ispf.h>		/* ISPF prototypes				*/
#include <mvs/link.h>		/* __link()						*/
#include <mvs/tso.h>		/* tsocmd()						*/
#include <ibm/mvs/ikjcppl.h>		/* CPPL typedef 				*/
#include <ibm/mvs/ikject.h>			/* ECT typedef					*/

int isp_select(const char *fmt, ...)
{
	int			rc 	= 20;
    va_list     list;
    char 		buf[256];
    int			len;

	va_start(list, fmt);
	len = vsprintf(buf, fmt, list);
	va_end(list);

	rc = isplink("SELECT  ", &len, ISP_LAST(buf));
	return rc;
}
