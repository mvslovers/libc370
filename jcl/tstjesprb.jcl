//TSTJESPR JOB (SYS),'LIBC370 23/24',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #23/#24 - the JES2 spool record walk (src/jes/jesprb.c) run on
//* the target.  Same source as the host test, test/host/tstjesprb.c: since
//* #25 the walk is free of assembler and of I/O, so it can be driven with a
//* synthetic block and needs no spool at all.
//*
//* What this adds over the host run - which is where the memory-safety
//* cases are actually gated, under -fsanitize=address:
//*   - cc370 codegen instead of the host compiler's
//*   - 24-bit pointers, and the pointer comparisons the bounds checks are
//*     built from
//*   - the struct layouts the walk assumes: sizeof(PRLINE)==3, sizeof(SPLINE)
//*     ==4 on S/370, which no host run can prove
//*
//* Build:   cc370 -O1 -Iinclude -Isrc/jes test/host/tstjesprb.c \
//*                -o TSTJESPR -flinker-output=xmit
//* Install: RECEIVE TSTJESPR.xmit into the STEPLIB below.
//*
//* RC 0 = every case passed, 1 = a case failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTJESPR,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
