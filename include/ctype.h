/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards.                            */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  ctype.h - ctype header file.                                     */
/*                                                                   */
/*********************************************************************/

#ifndef __CTYPE_INCLUDED
#define __CTYPE_INCLUDED
#include <sys/_cc370.h>

extern unsigned short *__isbuf;
extern short *__tolow;
extern short *__toup;

/* the functions, declared beside their macros as C99 7.1.4 requires:
   (isalpha)(c), &isalpha and #undef isalpha then reach libc.a (#337) */
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

/* the argument is cast to int: a plain char (unsigned under cc370) indexes
   the table without -Wall's "subscript has type char", and EOF (-1)
   still reaches the entry before the table (#416) */
#define isalnum(c) (__isbuf[(int)(c)] & 0x0001U)
#define isalpha(c) (__isbuf[(int)(c)] & 0x0002U)
#define iscntrl(c) (__isbuf[(int)(c)] & 0x0004U)
#define isdigit(c) (__isbuf[(int)(c)] & 0x0008U)
#define isgraph(c) (__isbuf[(int)(c)] & 0x0010U)
#define islower(c) (__isbuf[(int)(c)] & 0x0020U)
#define isprint(c) (__isbuf[(int)(c)] & 0x0040U)
#define ispunct(c) (__isbuf[(int)(c)] & 0x0080U)
#define isspace(c) (__isbuf[(int)(c)] & 0x0100U)
#define isupper(c) (__isbuf[(int)(c)] & 0x0200U)
#define isxdigit(c) (__isbuf[(int)(c)] & 0x0400U)
#define isblank(c) (__isbuf[(int)(c)] & 0x0800U)
#define tolower(c) (__tolow[(int)(c)])
#define toupper(c) (__toup[(int)(c)])

#endif
