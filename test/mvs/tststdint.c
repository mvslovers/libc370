/*
 * tststdint.c - libc370 #188: the signed minimum macros of <stdint.h>.
 *
 * C99 7.18.2 requires every limit macro to be a constant expression usable
 * in #if, with the value of the limit and the type an object of the
 * corresponding type has after the integer promotions.  Before #188 the
 * whole *_MIN family failed that, in four different ways:
 *
 *   INT32_MIN  0x80000000L - a hexadecimal constant that does not fit a
 *              long is unsigned long, so its value was +2147483648 and
 *              `c >= INT32_MIN` was always false (brexx370 lstring/mult.c)
 *   INT64_MIN  -9223372036854775808 - the literal does not fit long long;
 *              "integer constant is so large that it is unsigned" on every
 *              use (an error under -Werror) and positive in #if
 *   INT8_MIN / INT16_MIN  casts to int8_t/int16_t - not usable in #if, and
 *              the type was not promoted to int
 *   INT_FAST8_MIN  expanded to IN_LEASTT8_MIN, which does not exist
 *
 * INTMAX_MIN and the INT_LEAST/INT_FAST minimums are defined from these
 * and inherited each defect.
 *
 * The property is decided by the compiler, so COMPILING this file is the
 * test; running it only prints the summary.  It has to be compiled by
 * cc370, not by a host cc: on a 64-bit host long is 64 bits and <stdint.h>
 * takes different branches, and the "so large that it is unsigned"
 * behaviour is GCC 3.4's.
 *
 * Gate:    cc370 -std=gnu99 -Wall -Werror -O1 -Iinclude \
 *                -S test/mvs/tststdint.c -o /dev/null
 *          RC 0 = every check holds.  A failing check is a compile error:
 *          "size of array `...' is negative" names the check, an #error
 *          names the #if that failed.
 *
 * RED, 2026-09-27, against main's header before #188: rc 1, 43 lines of
 * diagnostics - INT32_MIN positive in C and in #if (range_check fails),
 * INT64_MIN/INTMAX_MIN "so large that it is unsigned", INT8_MIN/INT16_MIN
 * "missing binary operator" in #if and the wrong type in C, INT_FAST8_MIN
 * undeclared.  GREEN after the fix: rc 0, no output.
 *
 * Out of scope here and filed separately: INT8_C/INT16_C/UINT8_C/UINT16_C
 * are casts as well, and INT32_MAX has type int (limits.h defines it as
 * INT_MAX before <stdint.h> can).
 */
#include <stdio.h>
#include <stdint.h>

/* type, value and sign of one minimum, in C */
#define MIN_OK(name, M, T, V)                                             \
    typedef char name##_type[                                             \
        __builtin_types_compatible_p(__typeof__(M), T) ? 1 : -1];         \
    typedef char name##_value[((M) == (V)) ? 1 : -1];                     \
    typedef char name##_negative[((M) < 0) ? 1 : -1]

MIN_OK(int8,        INT8_MIN,        int,       -128);
MIN_OK(int16,       INT16_MIN,       int,       -32768);
MIN_OK(int32,       INT32_MIN,       long,      -2147483647L - 1);
MIN_OK(int64,       INT64_MIN,       long long, -9223372036854775807LL - 1);
MIN_OK(least8,      INT_LEAST8_MIN,  int,       -128);
MIN_OK(least16,     INT_LEAST16_MIN, int,       -32768);
MIN_OK(least32,     INT_LEAST32_MIN, long,      -2147483647L - 1);
MIN_OK(least64,     INT_LEAST64_MIN, long long, -9223372036854775807LL - 1);
MIN_OK(fast8,       INT_FAST8_MIN,   int,       -128);
MIN_OK(fast16,      INT_FAST16_MIN,  int,       -32768);
MIN_OK(fast32,      INT_FAST32_MIN,  long,      -2147483647L - 1);
MIN_OK(fast64,      INT_FAST64_MIN,  long long, -9223372036854775807LL - 1);
MIN_OK(intmax,      INTMAX_MIN,      long long, -9223372036854775807LL - 1);

/* each minimum is one below the negated maximum */
typedef char int32_span[(INT32_MIN + INT32_MAX == -1) ? 1 : -1];
typedef char int64_span[(INT64_MIN + INT64_MAX == -1) ? 1 : -1];

/* the shape brexx370 met: a range check against INT32_MIN must not be
   folded to false */
typedef char range_check[
    ((long long)0 >= INT32_MIN && (long long)0 <= INT32_MAX) ? 1 : -1];

/* and the same in #if, where C99 requires every one of them to work */
#if !(INT8_MIN == -128)
#error INT8_MIN in #if
#endif
#if !(INT16_MIN == -32768)
#error INT16_MIN in #if
#endif
#if !(INT32_MIN == -2147483647 - 1)
#error INT32_MIN in #if
#endif
#if !(INT64_MIN == -9223372036854775807LL - 1)
#error INT64_MIN in #if
#endif
#if !(INT_LEAST8_MIN == -128) || !(INT_FAST8_MIN == -128)
#error INT_LEAST8_MIN / INT_FAST8_MIN in #if
#endif
#if !(INT_LEAST16_MIN == -32768) || !(INT_FAST16_MIN == -32768)
#error INT_LEAST16_MIN / INT_FAST16_MIN in #if
#endif
#if !(INT_LEAST32_MIN == -2147483647 - 1) || !(INT_FAST32_MIN == -2147483647 - 1)
#error INT_LEAST32_MIN / INT_FAST32_MIN in #if
#endif
#if !(INT_LEAST64_MIN == -9223372036854775807LL - 1) \
    || !(INT_FAST64_MIN == -9223372036854775807LL - 1) \
    || !(INTMAX_MIN == -9223372036854775807LL - 1)
#error INT_LEAST64_MIN / INT_FAST64_MIN / INTMAX_MIN in #if
#endif
#if INT32_MIN >= 0 || INT64_MIN >= 0
#error a minimum is not negative in #if
#endif

int
main(void)
{
    printf("TSTSTDINT: libc370 #188 - every check is compile-time; "
           "this module exists, so all passed\n");
    return 0;
}
