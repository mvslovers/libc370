#include <stdio.h>
#include <string.h>
#include <ext/int64.h>

__asm__("\n&FUNC    SETC '__64_mul_i32'");
void __64_mul_i32(__64* a, int32_t b, __64* c)
{
	__64	tmp;

	if (a && c) {
		/* a negative b is its two's complement, as everywhere in __64;
		   modulo 2**64 that multiplies by b.  A negative b divided
		   (#397). */
		__64_from_i32(&tmp, b);
		__64_mul(a, &tmp, c);
	}
}

