//TSTPFARG JOB (SYS),'LIBC370 383',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #383 - every printf conversion takes its argument.
//* See test/mvs/tstpfarg.c.  STEPLIB is the scratch PDS recvpfa.jcl
//* restores into.  TSTPFA links the fixed libc.a, TSTPFAR the installed 2.2.0
//* sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01364, 2026-10-05: GREEN CC 0000, 34/34;
//*          RED CC 0001, 0 of 34 passed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTPFA,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PFASCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTPFAR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PFASCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
