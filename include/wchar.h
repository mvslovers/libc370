/*********************************************************************/
/*                                                                   */
/*  wchar.h - wide character types and limits                        */
/*                                                                   */
/*  Types and macros only (#195).  C99 7.24 also declares the wide   */
/*  character functions (wcslen, fgetwc, wprintf, mbrtowc, ...);     */
/*  libc370 does not implement them, so they are not declared here - */
/*  a call fails at compile time rather than at link time.  The      */
/*  multibyte conversions mbtowc, wctomb, mbstowcs and wcstombs are  */
/*  in <stdlib.h>.                                                   */
/*                                                                   */
/*********************************************************************/

#ifndef __WCHAR_INCLUDED
#define __WCHAR_INCLUDED

#ifndef __SIZE_T_DEFINED
#define __SIZE_T_DEFINED
#if (defined(__OS2__) || defined(__32BIT__) || defined(__MVS__) \
    || defined(__CMS__) || defined(__VSE__))
typedef unsigned long size_t;
#elif (defined(__MSDOS__) || defined(__DOS__) || defined(__POWERC) \
    || defined(__WIN32__) || defined(__gnu_linux__))
typedef unsigned int size_t;
#endif
#endif
#ifndef __WCHAR_T_DEFINED
#define __WCHAR_T_DEFINED
#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
#endif
#ifdef __WCHAR_TYPE__
typedef __WCHAR_TYPE__ wchar_t;
#else
typedef int wchar_t;
#endif
#endif

/* wint_t holds every wchar_t value plus WEOF (C99 7.24.1) */
#ifndef __WINT_T_DEFINED
#define __WINT_T_DEFINED
#ifdef __WINT_TYPE__
typedef __WINT_TYPE__ wint_t;
#else
typedef unsigned int wint_t;
#endif
#endif

#define WEOF ((wint_t)-1)

/* the same values as <stdint.h>, which defines them under the same guards */
#ifndef WCHAR_MAX
#define WCHAR_MAX 2147483647
#endif
#ifndef WCHAR_MIN
#define WCHAR_MIN (-WCHAR_MAX - 1)
#endif
#ifndef WINT_MAX
#define WINT_MAX 4294967295U
#endif
#ifndef WINT_MIN
#define WINT_MIN 0U
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#endif
