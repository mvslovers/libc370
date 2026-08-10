//TSTSPM   JOB (SYS),'LIBC370 89',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #89 - runtime heap subpool round trips (T1), realloc
//* across the subpool boundary (T2), __getmsp/__setsp(0) pinning
//* against a FREEMAIN SP=5 release (T5), the 24-bit header guard
//* on __getm (T7), and the no-PPA contract on a cthread TCB (T8).
//* Must be COND CODE 0000.  TIME=1 guards the worker join.
//* See test/mvs/tstspm.c.
//*
//S1       EXEC PGM=TSTSPM,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
