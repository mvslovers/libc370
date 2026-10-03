//TSTPFFLT JOB (SYS),'LIBC370 355',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #355 - printf flags around %f.
//* See test/mvs/tstpfflt.c.  STEPLIB is the scratch PDS recvpff.jcl
//* restores into.  TSTPFF links the fixed libc.a, TSTPFFR the installed 2.1.0
//* sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01315, 2026-10-03: GREEN CC 0000, 19/19;
//*          RED CC 0001, 13 of 19 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTPFF,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PFFSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTPFFR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PFFSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
