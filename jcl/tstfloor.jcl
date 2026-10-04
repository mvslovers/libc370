//TSTFLOOR JOB (SYS),'LIBC370 273',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #273 - floor(), ceil(), modf() and fmod() beyond 2**31.
//* See test/mvs/tstfloor.c.  STEPLIB is the scratch PDS recvflr.jcl
//* restores into.  TSTFLR links the fixed libc.a, TSTFLRR the installed
//* sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01337, 2026-10-04: GREEN CC 0000, 40/40;
//*          RED CC 0001, 25 of 40 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTFLR,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FLRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTFLRR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FLRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
