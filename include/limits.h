/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards.                            */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  limits.h - limits header file.                                   */
/*                                                                   */
/*********************************************************************/

#ifndef __LIMITS_INCLUDED
#define __LIMITS_INCLUDED
#include <sys/_cc370.h>

#define CHAR_BIT 8
#define SCHAR_MIN -128
#define SCHAR_MAX 127
#define UCHAR_MAX 255

#define CHAR_MIN 0
#define CHAR_MAX 255

#define MB_LEN_MAX 1

#define SHRT_MAX 32767
#define SHRT_MIN (-SHRT_MAX-1)
#define USHRT_MAX (65535)
#define UINT16_MAX (65535)

#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX-1)
#define INT32_MAX 2147483647L
#define UINT_MAX 4294967295U

#define LONG_MAX 2147483647L
#define LONG_MIN (-LONG_MAX-1)
#define ULONG_MAX 4294967295UL

#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX-1LL)
#define ULLONG_MAX 18446744073709551615ULL

#endif
