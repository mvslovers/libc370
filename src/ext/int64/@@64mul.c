#include <mvs/wto.h>
#include <stdio.h>
#include <string.h>
#include <ext/int64.h>
#include "src/internal/wtofmt.h"

__asm__("\n&FUNC    SETC '__64_mul'");
#if 1
void __64_mul(__64* a, __64* b, __64* c)
{
	__64	row;
	__64	tmp;
	__64	result;
	__64_DTYPE_TMP intermediate;
	int 	i, j, k, x;

	if (a && b && c) {
		__64_init(&result);

		for (i = __64_ARRAY_SIZE - 1; i >= 0; i--) {
			__64_init(&row);

			for (j = __64_ARRAY_SIZE - 1; j >= 0; j--) {
				k = i + j;
				if (k > (__64_ARRAY_SIZE-2)) {
					intermediate = ((__64_DTYPE_TMP)a->array[i] * (__64_DTYPE_TMP)b->array[j]);

					if (intermediate) {
						x = ((__64_ARRAY_SIZE - 1) * 2) - k;
						if (x < 0 || x > __64_ARRAY_SIZE -1) {
							/* without printf (size analysis F4): every long
							   long multiply links this */
							char	msg[120];
							int		m = 0;
							int		w;

							wtofmt_str(msg, &m, sizeof(msg), "__64_mul: *** x=");
							if (x < 0) wtofmt_str(msg, &m, sizeof(msg), "-");
							wtofmt_dec(msg, &m, sizeof(msg), (unsigned)(x < 0 ? -x : x));
							wtofmt_str(msg, &m, sizeof(msg), " is out of range ***");
							wto(msg);
							m = 0;
							wtofmt_str(msg, &m, sizeof(msg), "__64_mul: a");
							for (w = 0; w < __64_ARRAY_SIZE; w++) {
								wtofmt_str(msg, &m, sizeof(msg), " ");
								wtofmt_hex(msg, &m, sizeof(msg), a->array[w], 4);
							}
							wtofmt_str(msg, &m, sizeof(msg), " b");
							for (w = 0; w < __64_ARRAY_SIZE; w++) {
								wtofmt_str(msg, &m, sizeof(msg), " ");
								wtofmt_hex(msg, &m, sizeof(msg), b->array[w], 4);
							}
							wto(msg);
							m = 0;
							wtofmt_str(msg, &m, sizeof(msg), "__64_mul: i=");
							wtofmt_dec(msg, &m, sizeof(msg), (unsigned)i);
							wtofmt_str(msg, &m, sizeof(msg), ", j=");
							wtofmt_dec(msg, &m, sizeof(msg), (unsigned)j);
							wtofmt_str(msg, &m, sizeof(msg), ", k=");
							wtofmt_dec(msg, &m, sizeof(msg), (unsigned)k);
							wtofmt_str(msg, &m, sizeof(msg), ", 0x");
							wtofmt_hex(msg, &m, sizeof(msg), a->array[i], 4);
							wtofmt_str(msg, &m, sizeof(msg), " * 0x");
							wtofmt_hex(msg, &m, sizeof(msg), b->array[j], 4);
							wtofmt_str(msg, &m, sizeof(msg), " intermediate=");
							wtofmt_dec(msg, &m, sizeof(msg), (unsigned)intermediate);
							wto(msg);
						}

						__64_from_u32(&tmp, intermediate);
						/* wtodumpf(&tmp, sizeof(__64), "%s: tmp", __func__); */

						__64_lshift_word(&tmp, x);
						/* wtodumpf(&tmp, sizeof(__64), "%s: tmp after lshift word %d", __func__, x); */

						__64_add(&tmp, &row, &row);
						/* wtodumpf(&row, sizeof(__64), "%s: row after add tmp", __func__); */
					}
				}
			}
			__64_add(&result, &row, &result);
			/* wtodumpf(&result, sizeof(__64), "%s: result", __func__); */
		}
		__64_copy(&result, c);
		/* wtodumpf(c, sizeof(__64), "%s: c", __func__); */
	}
}
#endif
#if 0
void __64_mul(__64* a, __64* b, __64* c)
{
	__64	row;
	__64	tmp;
	int 	i, j;

	if (a && b && c) {
		__64_init(c);

		for (i = 0; i < __64_ARRAY_SIZE; ++i) {
			__64_init(&row);

			for (j = 0; j < __64_ARRAY_SIZE; ++j) {
				if (i + j < __64_ARRAY_SIZE) {
					__64_DTYPE_TMP intermediate = ((__64_DTYPE_TMP)a->array[i] * (__64_DTYPE_TMP)b->array[j]);

					__64_from_int(&tmp, intermediate);
					__64_lshift_word(&tmp, i + j);
					__64_add(&tmp, &row, &row);
				}
			}
			__64_add(c, &row, c);
		}
	}
}
#endif

