#include <stdio.h>
#include <string.h>
#include <ext/int64.h>

__asm__("\n&FUNC    SETC '__64_div_i32'");
void __64_div_i32(__64* a, int32_t b, __64* c)
{
	__64	tmp;

	if (a && c) {
		/* a negative b is its two's complement, as in __64_mod_i32().
		   A negative b multiplied (#397). */
		__64_from_i32(&tmp, b);
		__64_div(a, &tmp, c);
	}
}

