/*********************************************************************/
/*                                                                   */
/*  inttypes.h - format conversion of integer types (C99 7.8)        */
/*                                                                   */
/*********************************************************************/

#ifndef __INTTYPES_INCLUDED
#define __INTTYPES_INCLUDED
#include <sys/_cc370.h>

#include <stdint.h>

/* The types behind the macros, under cc370 (measured 2026-10-02):
   int8_t signed char, int16_t short, int32_t long, int64_t and
   intmax_t long long, intptr_t int.  The least and fast types are the
   exact-width ones.  -Wformat checks every macro against its type, so a
   change in <stdint.h> that breaks one fails a -Werror build. */

/* fprintf: 8 and 16 bits are promoted to int, so they need no modifier */
#define PRId8 "d"
#define PRIi8 "i"
#define PRIo8 "o"
#define PRIu8 "u"
#define PRIx8 "x"
#define PRIX8 "X"
#define PRIdLEAST8 "d"
#define PRIiLEAST8 "i"
#define PRIoLEAST8 "o"
#define PRIuLEAST8 "u"
#define PRIxLEAST8 "x"
#define PRIXLEAST8 "X"
#define PRIdFAST8 "d"
#define PRIiFAST8 "i"
#define PRIoFAST8 "o"
#define PRIuFAST8 "u"
#define PRIxFAST8 "x"
#define PRIXFAST8 "X"
#define PRId16 "d"
#define PRIi16 "i"
#define PRIo16 "o"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"
#define PRIdLEAST16 "d"
#define PRIiLEAST16 "i"
#define PRIoLEAST16 "o"
#define PRIuLEAST16 "u"
#define PRIxLEAST16 "x"
#define PRIXLEAST16 "X"
#define PRIdFAST16 "d"
#define PRIiFAST16 "i"
#define PRIoFAST16 "o"
#define PRIuFAST16 "u"
#define PRIxFAST16 "x"
#define PRIXFAST16 "X"
#define PRId32 "ld"
#define PRIi32 "li"
#define PRIo32 "lo"
#define PRIu32 "lu"
#define PRIx32 "lx"
#define PRIX32 "lX"
#define PRIdLEAST32 "ld"
#define PRIiLEAST32 "li"
#define PRIoLEAST32 "lo"
#define PRIuLEAST32 "lu"
#define PRIxLEAST32 "lx"
#define PRIXLEAST32 "lX"
#define PRIdFAST32 "ld"
#define PRIiFAST32 "li"
#define PRIoFAST32 "lo"
#define PRIuFAST32 "lu"
#define PRIxFAST32 "lx"
#define PRIXFAST32 "lX"
#define PRId64 "lld"
#define PRIi64 "lli"
#define PRIo64 "llo"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIX64 "llX"
#define PRIdLEAST64 "lld"
#define PRIiLEAST64 "lli"
#define PRIoLEAST64 "llo"
#define PRIuLEAST64 "llu"
#define PRIxLEAST64 "llx"
#define PRIXLEAST64 "llX"
#define PRIdFAST64 "lld"
#define PRIiFAST64 "lli"
#define PRIoFAST64 "llo"
#define PRIuFAST64 "llu"
#define PRIxFAST64 "llx"
#define PRIXFAST64 "llX"
#define PRIdMAX "lld"
#define PRIiMAX "lli"
#define PRIoMAX "llo"
#define PRIuMAX "llu"
#define PRIxMAX "llx"
#define PRIXMAX "llX"
#define PRIdPTR "d"
#define PRIiPTR "i"
#define PRIoPTR "o"
#define PRIuPTR "u"
#define PRIxPTR "x"
#define PRIXPTR "X"

/* fscanf: only the widths scanf can store.  It knows the length
   modifiers h and l and not yet hh, ll or j (#318), so SCN*8, SCN*64 and
   SCN*MAX are left out on purpose: a missing macro fails at compile
   time, where "lld" would compile and write 4 bytes of an 8-byte object. */
#define SCNd16 "hd"
#define SCNi16 "hi"
#define SCNo16 "ho"
#define SCNu16 "hu"
#define SCNx16 "hx"
#define SCNdLEAST16 "hd"
#define SCNiLEAST16 "hi"
#define SCNoLEAST16 "ho"
#define SCNuLEAST16 "hu"
#define SCNxLEAST16 "hx"
#define SCNdFAST16 "hd"
#define SCNiFAST16 "hi"
#define SCNoFAST16 "ho"
#define SCNuFAST16 "hu"
#define SCNxFAST16 "hx"
#define SCNd32 "ld"
#define SCNi32 "li"
#define SCNo32 "lo"
#define SCNu32 "lu"
#define SCNx32 "lx"
#define SCNdLEAST32 "ld"
#define SCNiLEAST32 "li"
#define SCNoLEAST32 "lo"
#define SCNuLEAST32 "lu"
#define SCNxLEAST32 "lx"
#define SCNdFAST32 "ld"
#define SCNiFAST32 "li"
#define SCNoFAST32 "lo"
#define SCNuFAST32 "lu"
#define SCNxFAST32 "lx"
#define SCNdPTR "d"
#define SCNiPTR "i"
#define SCNoPTR "o"
#define SCNuPTR "u"
#define SCNxPTR "x"

typedef struct { intmax_t quot; intmax_t rem; } imaxdiv_t;

intmax_t imaxabs(intmax_t j);
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);
intmax_t strtoimax(const char *nptr, char **endptr, int base);
uintmax_t strtoumax(const char *nptr, char **endptr, int base);
/* wcstoimax() and wcstoumax() follow with the wide-string conversions,
   which libc370 does not have yet. */

#endif
