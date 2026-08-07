//TSTTM64  JOB (SYS),'LIBC370 49',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #49 step A - pin the present behaviour of the time64 clock
//* family and the divisor layer under it, BEFORE anything is fixed.
//*
//* This job changes nothing and asserts nothing about what the values
//* OUGHT to be in absolute terms.  It records three things:
//*
//*   (A) CURRENT BEHAVIOUR - clock64() and mclock64() are the same
//*       function today and clock64() is ~1000x time64().  These cases
//*       are meant to be rewritten by the step-B fix; a step-B PR that
//*       leaves them untouched has not done what it claims.
//*
//*   (B) INVARIANT - time64() feeds gmtime64() and lands in this era,
//*       time64() has a seconds-since-1970 magnitude, the us/ms/s tiers
//*       agree, the clock is monotonic, and - case (9) - two time64()
//*       readings across a real 2-second STIMER wait differ by 1..3.
//*       Case (9) is dispatch_work()'s arithmetic from @@cminit.c with
//*       no thread manager to set up: it is the guard on the implicit
//*       "time64() returns seconds" assumption.  These must stay green
//*       across step B.
//*
//*   (C) ARITHMETIC - fixed vectors through __64_div_u32() and
//*       __64_divmod_u32() at both 1000 and 1000000, the two divisors
//*       step B chooses between.
//*
//* No setup, no data sets, nothing left behind.  The job runs for a
//* little over two seconds because of the wait in case (9).
//*
//* Class (C) has to run HERE rather than on a build host: __64 mixes
//* .u64, u32[] and array[] views of the same eight bytes, which agree
//* only on a big-endian machine.  On a little-endian host the sources
//* still compile and run and simply return wrong answers, so there is
//* nothing to gain and a false green to lose.  test/host/tsttm64vec.c
//* re-derives the expected quotients natively instead, touching no
//* libc370 code.
//*
//* See test/mvs/tsttm64.c.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tsttm64.c -o TSTTM64 \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every check passed, 8 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTTM64,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//*
//* RECFM=FB, not FBA: with ANSI carriage control the first byte of every
//* record is the control character and is consumed, which eats the first
//* character of every line the program prints.  LRECL 133 is pinned
//* because the widest diagnostic below is 106 columns and the system
//* default is not worth guessing.
//*
//SYSPRINT DD  SYSOUT=*,DCB=(RECFM=FB,LRECL=133,BLKSIZE=1330)
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
