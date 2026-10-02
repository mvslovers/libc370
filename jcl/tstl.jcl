//TSTL     JOB (SYS),'LIBC370 316',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #316 - strtol, strtoul, atoi, atol.
//* See test/mvs/tstl.c.  STEPLIB is the scratch PDS recvl.jcl
//* restores into.  TSTL links the fixed libc.a, TSTLR the 2.0.0
//* sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01173, 2026-10-03: GREEN CC 0000, 35/35;
//*          RED CC 0001, 17 of 35 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTLR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
