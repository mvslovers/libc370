#include <mvs/wto.h>
#include <stdio.h>
#include <string.h>
#include <ext/int64.h>
#include "src/internal/wtofmt.h"

__asm__("\n&FUNC    SETC '__64_lshift_word'");
void __64_lshift_word(__64* a, int nwords)
{
	int i;
	int	j;
	int shift = nwords;

	if (a) {
		/* wtodumpf(a, sizeof(__64), "%s: a before shift %d", __func__, nwords); */
#if 1
		if (shift < 0) shift = 0;
		if (shift > __64_ARRAY_SIZE) shift = __64_ARRAY_SIZE;	/* range 0 through 4 */
		if (shift != nwords) {
			/* without printf (size analysis F4): every long long shift
			   links this */
			char	msg[80];
			int		k = 0;

			wtofmt_str(msg, &k, sizeof(msg), "__64_lshift_word: *** shift corrected from ");
			if (nwords < 0) wtofmt_str(msg, &k, sizeof(msg), "-");
			wtofmt_dec(msg, &k, sizeof(msg), (unsigned)(nwords < 0 ? -nwords : nwords));
			wtofmt_str(msg, &k, sizeof(msg), " to ");
			wtofmt_dec(msg, &k, sizeof(msg), (unsigned)shift);
			wtofmt_str(msg, &k, sizeof(msg), " ***");
			wto(msg);
		}

		/* if shift is 0 then do nothing */
		for (i=0; i < shift; i++) {
			for(j=0; j < (__64_ARRAY_SIZE - 1); j++) {
				a->array[j] = a->array[j+1];
			}
			a->array[j] = 0;
		}
		/* wtodumpf(a, sizeof(__64), "%s: a after shifting %d", __func__, shift); */
#endif
	}
}

