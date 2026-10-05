//TSTASMRC JOB (SYS),'LIBC370 425',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #425 #427 - asm stores the compiler is told about.
//* See test/mvs/tstasmrc.c.  STEPLIB is the scratch PDS recvasm.jcl
//* restores into.  TSTASMR: this tree's headers and libc.a; TSTASMRR:
//* main's string.h, strutil.h and installed libc.a - red control.
//*
//* Run:     mvsdev JOB01372, 2026-10-05: GREEN CC 0000, 5/5;
//*          RED CC 0001, 0/5.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTASMR,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ASMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTASMRR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ASMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
