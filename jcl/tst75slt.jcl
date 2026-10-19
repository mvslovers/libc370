//TST75SLT JOB (SYS),'LIBC370 162',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* MEASUREMENT PROBE for issue #162 - X'75' slot exhaustion and
//* the "talk" aliasing in find_slot().  See test/mvs/tst75slt.c.
//*
//* WARNING: OWN DISPOSABLE CONTAINER ONLY.  This exhausts an
//* emulator-wide resource shared by every address space and its
//* verification transfer MAY CRASH THE EMULATOR.  Never run on a
//* shared or real system; never add as a [[test]] entry.
//*
//* Read the job log (JESMSGLG) for the TST75SLT wtof lines:
//*   TST75SLT limit=N pin=P aliasfrom=C
//*     -> N distinct slots, pin value P, first duplicated at
//*        leak call C
//*   TST75SLT verify ...   -> the post-exhaustion transfer
//* A limit far below the leak-call count, with the pin repeated
//* after it, is the find_slot aliasing.  A missing verify line
//* means the emulator went down.
//*
//S1       EXEC PGM=TST75SLT,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
