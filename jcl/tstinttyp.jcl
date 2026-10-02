//TSTINTT  JOB (SYS),'LIBC370 314',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #314 - <inttypes.h>.  See test/mvs/tstinttyp.c.
//* STEPLIB is the scratch PDS recvintt.jcl restores into.
//*
//* Run:     mvsdev JOB01167, 2026-10-02: GREEN CC 0000, 19/19.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTINTT,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.INTTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
