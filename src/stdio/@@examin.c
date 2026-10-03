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

#define unused(x) ((void)(x))
/* Emit one character.  For the string sink (fq == NULL) the write is
   bounded by smax, the space the caller says is left in s; extraCh keeps
   counting the LOGICAL length regardless, so the caller still learns what
   the full conversion would have needed (#128).  The FILE sink writes
   through __fputc, not putc: vvprintf already holds the FILE lock, and
   the public wrapper's unlock would give that hold away (#145). */
#define outch(ch) do { \
        if (fq != NULL) __fputc(ch, fq); \
        else if (sput < smax) { *s++ = (char)ch; sput++; } \
    } while (0)
#define inch() ((fp == NULL) ? \
    (ch = (unsigned char)*s++) : (ch = getc(fp)))

static const char DIGITS[] = "0123456789ABCDEF";
static const char digits[] = "0123456789abcdef";

extern void
__dblcvt(double num, char cnvtype, size_t nwidth, int nprecision,
         char *result, size_t rsize);

int
__examin(const char **formt, FILE *fq, char *s, va_list *arg, int smax)
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
    int         half        = 0;
    int         lng         = 0;
    int         specifier   = 0;
    int         fin;
    long        lvalue;
    short       hvalue;
    int         ivalue;
    unsigned    ulvalue;
    double      vdbl;
    char        *svalue;
    char        work[128];
    int         x;
    int         y;
    int         rem;
    const char  *format;
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
        /* all environments should promote shorts to ints,
           so we should be able to ignore the 'h' specifier.
           It will create problems otherwise. */
        /* half = 1; */
        if (*(format+1) == 'h') {
            format++;               /* hh: char promotes to int too */
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
        else if (half) {
            /* short is promoted to int, so use int */
            hvalue = va_arg(*arg, int);
            if (specifier == 'u') lvalue = (unsigned short)hvalue;
            else lvalue = hvalue;
        }
        else {
            ivalue = va_arg(*arg, int);
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

        while (x < precision) {
            work[x] = '0';
            x++;
        }

        if (neg) {
            work[x++] = '-';
        }
        else if (flagPlus) {
            work[x++] = '+';
        }
        else if (flagSpace) {
            work[x++] = ' ';
        }

        if (flagZero) {
            fillCh = '0';
        }
        else {
            fillCh = ' ';
        }

        y = x;
        if (!flagMinus) {
            while (y < width) {
                outch(fillCh);
                extraCh++;
                y++;
            }
        }

        if (flagHash && (toupper((unsigned char)specifier) == 'X')) {
            outch('0');
            outch('x');
            extraCh += 2;
        }

        x--;
        while (x >= 0) {
            outch(work[x]);
            extraCh++;
            x--;
        }

        if (flagMinus) {
            while (y < width) {
                outch(fillCh);
                extraCh++;
                y++;
            }
        }
    }
    else if (strchr("eEgGfF", specifier) != NULL && specifier != 0) {
        if (precision < 0) {
            precision = 6;
        }

        vdbl = va_arg(*arg, double);
        /* the digits only; width and flags are placed here, as for the
           integers: __dblcvt() pads with blanks on the left, which left the
           0 and - flags unheard and put + or space outside the width (#355) */
        __dblcvt(vdbl, specifier, 0, precision, work,
                 sizeof(work) - 1);   /* keep a byte for the sign (#222) */
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
            if (!flagMinus && flagZero) {
                for (x = 0; x < pad; x++) outch('0');
            }
            for (x = 0; x < (int)slen; x++) {
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
    *formt = format;
    return (extraCh);
}
