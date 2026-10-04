//TSTVARG  JOB (SYS),'LIBC370 382',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #382 - va_start/va_arg by kind of last named parameter.
//* See test/mvs/tstvarg.c.  STEPLIB is the scratch PDS recvvarg.jcl
//* restores into.  TSTVARG is built with the fixed <stdarg.h>, TSTVARGR
//* with the one before #382 - the red control.
//*
//* Run:     mvsdev JOB01362, 2026-10-05: GREEN CC 0000, 8/8;
//*          RED CC 0001, 6 of 8 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTVARG,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VARGSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTVARGR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VARGSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
