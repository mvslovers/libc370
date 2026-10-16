/* TZSET.C */
#include <ctype.h>
#include <mvs/wto.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>       /* our own prototype, so the definition is checked */
#include "mvs/env.h"
#include "mvs/crt.h"
#include "src/internal/wtofmt.h"

void
tzset(void)
{
    int 	*cvt;
    char	*tz;
    char    *p;
    CLIBCRT *crt;
    int     tzoffset;
	int		rem;
	int		neg;
	int		tmp;

    tz = getenv("tz");
    if (!tz) tz = getenv("TZ");

    if (tz) {
		/* assume TZ is "[-]HH[:MM][:SS]" format */
		p = tz;
		tzoffset = 0;
		neg = 0;
		/* wtof("%s: procesing TZ=\"%s\"", __func__, p); */

		while (*p=='-') {
			neg = 1;
			p++;
			/* wtof("%s: neg=1", __func__); */
		}

		if (isdigit(*p)) {
			/* hours */
			tmp = atoi(p);
			/* wtof("%s: hours=%d", __func__, tmp); */
			tzoffset += tmp * 60 * 60;
			p = strchr(p, ':');

			if (p) {
				/* minutes */
				p++;
				tmp = atoi(p);
				/* wtof("%s: minutes=%d", __func__, tmp); */
				tzoffset += tmp * 60;
				p = strchr(p, ':');
			}

			if (p) {
				/* seconds */
				p++;
				tmp = atoi(p);
				/* wtof("%s: seconds=%d", __func__, tmp); */
				tzoffset += tmp;
			}

			if (neg) tzoffset = 0 - tzoffset;
			/* wtof("%s: tzoffset=%d", __func__, tzoffset); */
			goto done;
		}

		{
			/* without the printf family (#564): tzset() is linked by
			   every program that uses a time function */
			char	msg[120];
			int		i = 0;

			wtofmt_str(msg, &i, sizeof(msg),
				"tzset: Invalid time zone format in TZ variable \"");
			wtofmt_str(msg, &i, sizeof(msg), tz);
			wtofmt_str(msg, &i, sizeof(msg), "\"");
			wto(msg);
			i = 0;
			wtofmt_str(msg, &i, sizeof(msg),
				"tzset: Expected format is TZ=[-]HH[:MM][:SS]");
			wto(msg);
			i = 0;
			wtofmt_str(msg, &i, sizeof(msg),
				"tzset: Defaulting to time zone offset calculated from CVTTZ");
			wto(msg);
		}
    }

	/* calculate time zone offset from system CVTTZ value */
    cvt = *(int**)16;                      		/* get CVT pointer from PSA */
    tzoffset = (int)(cvt[0x130/4] * 1.0485765); /* convert CVTTZ to seconds from UTC */

	/* attempt to correct for cast/rounding errors */
	/* wtof("%s: tzoffset=%d", __func__, tzoffset); */
    if (tzoffset < 0) {
		rem = (-tzoffset) % 10;
		/* wtof("%s: rem=%d", __func__, rem); */

		if (rem) tzoffset -= (10 - rem); /* correct tzoffset */
		/* wtof("%s: tzoffset=%d", __func__, tzoffset); */
	}
	else if (tzoffset > 0) {
		rem = tzoffset % 10;
		/* wtof("%s: rem=%d", __func__, rem); */

		if (rem) tzoffset += (10 - rem); /* correct tzoffset */
		/* wtof("%s: tzoffset=%d", __func__, tzoffset); */
	}

done:
    __tzset(tzoffset);
    crt = __crtget();                       /* once per task (#564) */
    if (crt) crt->crtflag |= CRTFLAG_TZSET;
}
