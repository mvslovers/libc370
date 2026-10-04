//TSTLSTNM JOB (SYS),'LIBC370 61',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #61, #157, #158 - the list builders under a storage
//* shortage.  See test/mvs/tstlstnm.c.  STEPLIB is the scratch PDS
//* recvlnm.jcl restores into.  TSTLNM links the fixed libc.a, TSTLNMR
//* the installed sysroot libc.a - the red control.
//*
//* Run:     mvsdev JOB01347, 2026-10-04: GREEN CC 0000, 12/12;
//*          RED CC 0001, 42 short lists, 5 of 12 failed.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTLNM,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LNMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTLNMR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LNMSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
