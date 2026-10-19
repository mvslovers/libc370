/* @@EXAMIN.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

#include <ext/int64.h>

/* Emit one character.  For the string sink (fq == NULL) the write is
   bounded by smax, the space the caller says is left in s; extraCh keeps
   counting the LOGICAL length regardless, so the caller still learns what
   the full conversion would have needed (#128).  The FILE sink writes
   through putcf, which vfprintf() passes as __fputc, not putc: vvprintf
   already holds the FILE lock, and the public wrapper's unlock would give
   that hold away (#145).  The string sinks pass NULL and so do not link
   the stream layer (size analysis F2). */
#define outch(ch) do { \
        if (fq != NULL) putcf(ch, fq); \
        else if (sput < smax) { *s++ = (char)ch; sput++; } \
    } while (0)
#define inch() ((fp == NULL) ? \
    (ch = (unsigned char)*s++) : (ch = getc(fp)))

static const char DIGITS[] = "0123456789ABCDEF";
static const char digits[] = "0123456789abcdef";

extern void
__dblcvx(double num, char cnvtype, size_t nwidth, int nprecision, int alt,
         char *result, size_t rsize);

/* hexflt() - %a and %A of the double whose two words are w (#423).
**
** A System/370 double is 0.F x 16**(E-64): a sign bit, a 7-bit exponent
** E and a fraction F of 14 hex digits.  Its digits are hexadecimal
** already, so C99's [-]0xh.hhhp+d is F's first digit h before the point,
** the rest after it, and the binary exponent 4 * (E - 65).  A normalized
** fraction has a nonzero first digit, as C99 asks for.  No floating point
** is needed; a shorter precision rounds by a hex-digit carry, half away
** from zero like the other conversions.  The result goes into out, at
** most n bytes with the NUL; a precision that does not fit is cut. */
__asm__("\n&FUNC    SETC 'hexflt'");
static void
hexflt(const unsigned *w, int upper, int prec, int alt, char *out, size_t n)
{
    const char  *dg     = upper ? DIGITS : digits;
    char        d[15];
    int         e       = (int)((w[0] >> 24) & 0x7F) - 65;
    int         nd, i, k, last;
    char        *o      = out;
    char        *end    = out + n - 8;      /* room for p, sign, exponent */

    for (i = 0; i < 6; i++) d[i] = (char)((w[0] >> (20 - 4 * i)) & 0xF);
    for (i = 0; i < 8; i++) d[6 + i] = (char)((w[1] >> (28 - 4 * i)) & 0xF);
    d[14] = 0;

    for (last = 13; last >= 0 && d[last] == 0; last--) ;
    if (last < 0) {
        e = 0;                              /* zero: 0x0p+0 */
    }
    else {
        /* an unnormalized fraction: its first nonzero digit goes first */
        for (k = 0; d[k] == 0; k++) ;
        for (i = 0; i < 14; i++) d[i] = (i + k < 14) ? d[i + k] : 0;
        e -= k;
        last -= k;
    }

    nd = (prec < 0) ? last : prec;          /* digits after the point */
    if (nd < 13 && d[nd + 1] >= 8) {
        /* round: carry from the last digit kept */
        for (i = nd; i >= 0 && ++d[i] == 16; i--) d[i] = 0;
        if (i < 0) {
            d[0] = 1;                       /* F.FF.. became 10.0 */
            e++;
        }
    }

    if (w[0] & 0x80000000U) *o++ = '-';
    *o++ = '0';
    *o++ = upper ? 'X' : 'x';
    *o++ = dg[(int)d[0]];
    if (nd > 0 || alt) *o++ = '.';
    for (i = 1; i <= nd && o < end; i++) *o++ = (i < 14) ? dg[(int)d[i]] : '0';
    *o++ = upper ? 'P' : 'p';
    e *= 4;
    *o++ = (e < 0) ? '-' : '+';
    if (e < 0) e = -e;
    k = (e >= 100) ? 3 : (e >= 10) ? 2 : 1;
    for (i = k; i > 0; i--, e /= 10) o[i - 1] = (char)('0' + e % 10);
    o[k] = '\0';
}

int
__examin(const char **formt, FILE *fq, char *s, va_list *arg, int smax,
         int done, int (*putcf)(int c, FILE *fp))
{
    int         extraCh     = 0;
    int         sput        = 0;    /* bytes actually written to s (#128)  */
    int         flagMinus   = 0;
    int         flagPlus    = 0;
    int         flagSpace   = 0;
    int         flagHash    = 0;
    int         flagZero    = 0;
    int         width       = 0;
    int         precision   = -1;
    int         lng         = 0;
    int         hmod        = 0;    /* %n: 1 = h, 2 = hh (#383)          */
    int         specifier   = 0;
    int         fin;
    int         pfx;            /* length of the 0x of %#x */
    int         sgn;            /* the sign character, or 0 */
    int         nz;             /* leading zeros to print */
    long        lvalue;
    int         ivalue;
    unsigned    ulvalue;
    double      vdbl;
    char        *svalue;
    char        work[128];
    int         x;
    int         y;
    int         rem;
    const char  *format;
    const char  *start;     /* first character after the % (#383)  */
    int         base;
    int         fillCh;
    int         neg;
    int         length;
    size_t      slen;
	/* support for long long (64 bit values ) */
	__64		value64;
	__64		rem64;
	__64		div64;
	__64		base64;

    format = *formt;
    start = format;

    /* processing flags */
    fin = 0;
    while (!fin) {
        switch (*format) {
            case '-': flagMinus = 1;
                      break;
            case '+': flagPlus = 1;
                      break;
            case ' ': flagSpace = 1;
                      break;
            case '#': flagHash = 1;
                      break;
            case '0': flagZero = 1;
                      break;
            case '*': width = va_arg(*arg, int);
                      if (width < 0)
                      {
                          flagMinus = 1;
                          width = -width;
                      }
                      break;
            default:  fin = 1;
                      break;
        }

        if (!fin) {
            format++;
        }
        else {
            if (flagSpace && flagPlus) {
                flagSpace = 0;
            }
            if (flagMinus) {
                flagZero = 0;
            }
        }
    }

    /* processing width */
    if (isdigit((unsigned char)*format)) {
        while (isdigit((unsigned char)*format)) {
            width = width * 10 + (*format - '0');
            format++;
        }
    }

    /* processing precision */
    if (*format == '.') {
        format++;
        if (*format == '*') {
            precision = va_arg(*arg, int);
            format++;
        }
        else {
            precision = 0;
            while (isdigit((unsigned char)*format)) {
                precision = precision * 10 + (*format - '0');
                format++;
            }
        }
    }

    /* processing h/l/L */
    if (*format == 'h') {
        /* h and hh: the argument is an int (promoted); hmod narrows
           it again where it is printed, and %n stores the type */
        hmod = 1;
        if (*(format+1) == 'h') {
            format++;               /* hh: char promotes to int too */
            hmod = 2;
        }
    }
    else if (*format == 'z' || *format == 't') {
        /* size_t and ptrdiff_t are 32 bit, like long (#211) */
        lng = 1;
    }
    else if (*format == 'j') {
        /* intmax_t is 64 bit, like long long (#211) */
        lng = 2;
    }
    else if (*format == 'l') {
        lng = 1;
		if (*(format+1) == 'l') {
			lng = 2;
			format++;
		}
    }
    else if (*format == 'L') {
        lng = 1;
		if (*(format+1) == 'L') {
			lng = 2;
			format++;
		}
    }
    else {
        format--;
    }
    format++;

    /* processing specifier */
    specifier = *format;

    if (strchr("dxXuiop", specifier) != NULL && specifier != 0) {
        /* with a precision the 0 flag is ignored, C99 7.19.6.1 (#578) */
        if (precision >= 0) {
            flagZero = 0;
        }
        if (precision < 0) {
            precision = 1;
        }
        
        if (lng==1) {
            lvalue = va_arg(*arg, long);
        }
		else if (lng==2) {
			lvalue = va_arg(*arg, long);
			value64.u32[0] = (uint32_t)lvalue;
			lvalue = va_arg(*arg, long);
			value64.u32[1] = (uint32_t)lvalue;
		}
        else {
            ivalue = va_arg(*arg, int);
            /* h and hh: the value was promoted to int, and is printed as
               the short or char it was - %hu of -1 is 65535, %hhd of
               255 is -1 (#384) */
            if (hmod == 1) {
                ivalue = (specifier == 'd' || specifier == 'i')
                       ? (int)(short)ivalue : (int)(unsigned short)ivalue;
            }
            else if (hmod == 2) {
                ivalue = (specifier == 'd' || specifier == 'i')
                       ? (int)(signed char)ivalue : (int)(unsigned char)ivalue;
            }
            if (specifier == 'u') lvalue = (unsigned int)ivalue;
            else lvalue = ivalue;
        }

        ulvalue = (unsigned long)lvalue;
        if ((lvalue < 0) && ((specifier == 'd') || (specifier == 'i'))) {
			neg = 1;
			ulvalue = -lvalue;
        }
        else {
            neg = 0;
        }

        if ((specifier == 'X') || (specifier == 'x') || (specifier == 'p')) {
            base = 16;
        }
        else if (specifier == 'o') {
            base = 8;
        }
        else {
            base = 10;
        }

        if (specifier == 'p') {
            precision = 8;
        }

        x = 0;
        if (lng==2) {
			/* 64 bit variables */
			__64_from_i32(&base64, base);
			neg = 0;	/* __64 values are unsigned */
			if (((specifier == 'd') || (specifier == 'i'))
			    && (value64.u32[0] & 0x80000000UL)) {
				/* a negative value: print its magnitude, the two's
				   complement.  INT64_MIN's fits the unsigned 64 bits.
				   u32[0] is the high word, as filled above (#321) */
				neg = 1;
				value64.u32[0] = ~value64.u32[0];
				value64.u32[1] = ~value64.u32[1] + 1;
				if (value64.u32[1] == 0) {
					value64.u32[0]++;
				}
			}
			while (!(__64_is_zero(&value64))) {
				__64_divmod(&value64, &base64, &div64, &rem64);
				rem = __64_to_i32(&rem64);

				if ((specifier == 'X') || (specifier == 'p')) {
					work[x] = DIGITS[rem];	/* uppercase digits */
				}
				else {
					work[x] = digits[rem];	/* lowercase digits */
				}
				x++;
				__64_copy(&div64, &value64);
			}
		}
		else {
			/* 32 bit variables */
			while (ulvalue > 0) {
				rem = (int)(ulvalue % base);
				if ((specifier == 'X') || (specifier == 'p')) {
					work[x] = DIGITS[rem];	/* uppercase digits */
				}
				else {
					work[x] = digits[rem];	/* lowercase digits */
				}
				x++;
				ulvalue = ulvalue / base;
			}
		}

        /* work holds the x digits, last first.  The leading zeros - the
           precision's and the 0 flag's - are counted, not stored: a
           precision of 128 or more ran past work[128] (#384).  C99
           7.19.6.1 order: blanks, sign, 0x, zeros, digits, and with the
           - flag the blanks last.  The 0 flag's zeros went in front of
           the sign (%05d of -1 printed 000-1), and the 0x of %#x was
           not counted in the width (#384) */
        /* # (#578): 0x or 0X only in front of a value other than 0, and
           %o's first digit a 0, by a zero more if it is not one already */
        pfx = (flagHash && toupper((unsigned char)specifier) == 'X' && x > 0)
            ? 2 : 0;
        sgn = neg ? '-' : flagPlus ? '+' : flagSpace ? ' ' : 0;
        nz = (precision > x) ? precision - x : 0;
        if (flagHash && specifier == 'o' && nz == 0
            && (x == 0 || work[x - 1] != '0')) {
            nz = 1;
        }
        if (flagZero && x + nz + (sgn != 0) + pfx < width) {
            nz = width - (sgn != 0) - pfx - x;
        }

        y = x + nz + (sgn != 0) + pfx;
        if (!flagMinus) {
            while (y < width) {
                outch(' ');
                extraCh++;
                y++;
            }
        }
        if (sgn) {
            outch(sgn);
            extraCh++;
        }
        if (pfx) {
            outch('0');
            outch(specifier == 'X' ? 'X' : 'x');
            extraCh += 2;
        }
        while (nz-- > 0) {
            outch('0');
            extraCh++;
        }
        while (x-- > 0) {
            outch(work[x]);
            extraCh++;
        }
        if (flagMinus) {
            while (y < width) {
                outch(' ');
                extraCh++;
                y++;
            }
        }
    }
    else if (strchr("eEgGfFaA", specifier) != NULL && specifier != 0) {
        /* %a: the hex digits after the 0x come after any zero padding */
        pfx = (specifier == 'a' || specifier == 'A') ? 2 : 0;

        vdbl = va_arg(*arg, double);
        if (pfx) {
            unsigned w[2];

            memcpy(w, &vdbl, sizeof(w));
            hexflt(w, specifier == 'A', precision, flagHash, work,
                   sizeof(work) - 1);
        }
        else {
            /* the digits only; width and flags are placed here, as for the
               integers: __dblcvt() pads with blanks on the left, which left
               the 0 and - flags unheard and put + or space outside the
               width (#355) */
            __dblcvx(vdbl, specifier, 0, precision < 0 ? 6 : precision,
                     flagHash, work,
                     sizeof(work) - 1);   /* keep a byte for the sign (#222) */
        }
        {
            const char  *body = work;
            int         sign = 0;
            int         pad;

            if (*body == '-') {
                sign = '-';
                body++;
            }
            else if (flagPlus) {
                sign = '+';
            }
            else if (flagSpace) {
                sign = ' ';
            }
            slen = strlen(body);
            pad = width - (int)slen - (sign != 0);
            if (pad < 0) {
                pad = 0;
            }

            /* C99 7.19.6.1: - pads on the right; otherwise 0 pads with
               zeros between the sign and the digits, else blanks go first */
            if (!flagMinus && !flagZero) {
                for (x = 0; x < pad; x++) outch(' ');
            }
            if (sign) {
                outch(sign);
            }
            for (x = 0; x < pfx; x++) {
                outch(body[x]);
            }
            if (!flagMinus && flagZero) {
                for (x = 0; x < pad; x++) outch('0');
            }
            for (x = pfx; x < (int)slen; x++) {
                outch(body[x]);
            }
            if (flagMinus) {
                for (x = 0; x < pad; x++) outch(' ');
            }
            extraCh += pad + (sign != 0) + (int)slen;
        }
    }
    else if (specifier == 's') {
        svalue = va_arg(*arg, char *);
        /* NULL prints "(null)", as a plain %s does; with a width or a
           precision it reached strlen()/memchr() (#384) */
        if (svalue == NULL) {
            svalue = "(null)";
        }
        fillCh = ' ';
        if (precision > 0) {
            char *p;

            p = memchr(svalue, '\0', precision);
            if (p != NULL) {
                length = (int)(p - svalue);
            }
            else {
                length = precision;
            }
        }
        else if (precision < 0) {
            length = strlen(svalue);
        }
        else {
            length = 0;
        }

        if (!flagMinus) {
            if (length < width) {
                extraCh += (width - length);
                for (x = 0; x < (width - length); x++) {
                    outch(fillCh);
                }
            }
        }

        for (x = 0; x < length; x++) {
            outch(svalue[x]);
        }

        extraCh += length;
        if (flagMinus) {
            if (length < width) {
                extraCh += (width - length);
                for (x = 0; x < (width - length); x++) {
                    outch(fillCh);
                }
            }
        }
    }
    else if (specifier == 'c') {
        /* %c with a width or a flag; a plain %c never gets here (#383) */
        ivalue = va_arg(*arg, int);
        for (x = 1; !flagMinus && x < width; x++) {
            outch(' ');
            extraCh++;
        }
        outch((unsigned char)ivalue);
        extraCh++;
        for (x = 1; flagMinus && x < width; x++) {
            outch(' ');
            extraCh++;
        }
    }
    else if (specifier == 'n') {
        /* %n with a length modifier: the count is what the caller wrote
           before this conversion, stored at the width the modifier
           names (C99 7.19.6.1) - flags and a width have no meaning here */
        void *vptr = va_arg(*arg, void *);

        if (lng == 2) {
            *(long long *)vptr = done;
        }
        else if (lng == 1) {
            *(long *)vptr = done;
        }
        else if (hmod == 2) {
            *(signed char *)vptr = (signed char)done;
        }
        else if (hmod == 1) {
            *(short *)vptr = (short)done;
        }
        else {
            *(int *)vptr = done;
        }
    }
    else if (specifier == '%') {
        /* %% with flags or a width: still one % */
        outch('%');
        extraCh++;
    }
    else {
        /* no meaning at all: print the conversion as it stands, so the
           omission shows */
        outch('%');
        extraCh++;
        for (const char *p = start; p < format; p++) {
            outch(*p);
            extraCh++;
        }
        if (specifier != 0) {
            outch(specifier);
            extraCh++;
        }
        else {
            /* "...%" at the end: the caller steps past the character
               *formt names, and that must not be the NUL */
            format--;
        }
    }
    *formt = format;
    return (extraCh);
}
