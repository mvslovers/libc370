//TSTBLANK JOB (SYS),'LIBC370 314',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #314 - isblank(), macro and function.  See
//* test/mvs/tstblank.c.  STEPLIB is the scratch PDS recvblnk.jcl
//* restores into.
//*
//* Run:     mvsdev JOB01157, 2026-10-02: GREEN CC 0000, 10/10.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTBLNK,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.BLNKSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
