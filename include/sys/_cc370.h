#ifndef __SYS_CC370_H
#define __SYS_CC370_H

/* The oldest cc370 this libc370 works with (#315), in the form cc370
   predefines as __CC370__: MAJOR*10000 + MINOR*100 + PATCH (cc370#704).
   From 2.1.0 the compiler helpers and the prologue macros are cc370's own
   (#313), so an older compiler cannot build or link against this library.
   From 2.4.0 no startfile is installed: @@CRT0 comes out of libc.a, and
   only a cc370 from 1.4.0 on stops naming crt0.o on the link line (#159).

   Every public header includes this one, so the check holds wherever code
   is compiled, whatever installed the pieces.  It applies to the target
   only (__MVS__, which cc370 predefines): host tests compile these headers
   with the host compiler.

   The number is written from sdk/cc370.json, the one place the cc370
   requirement lives; `sdk/headermap.py check` (CI) fails when they
   disagree or when a header does not include this file. */
#define __LIBC370_MIN_CC370 10400

#if defined(__MVS__)
#if !defined(__CC370__) || __CC370__ < __LIBC370_MIN_CC370
#error "libc370 needs cc370 1.4.0 or later"
#endif
#endif

#endif
