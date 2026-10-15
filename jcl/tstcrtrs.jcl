//TSTCRTRS JOB (SYS),'LIBC370 546',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #546 - heap left behind by a normal run of a LINKed C module.
//* See test/mvs/tstcrtrs.c.  STEPLIB is the scratch PDS recvcrs.jcl
//* restores into.  The outer probe is the same in both steps; the inner
//* module it LINKs differs: GREEN TSTCRTRI (this tree's libc.a), RED
//* TSTCRTRR (the installed one).
//*
//* Run:     mvsdev JOB02349, 2026-10-15: GREEN CC 0000, 0K lost per
//*          500 runs; RED CC 0008, 64K per 500 runs (128 bytes each).
//*
//* RC 0 = every check passed, 8 = a check failed.
//*
//GREEN    EXEC PGM=TSTCRTRS,REGION=6M,PARM='TSTCRTRI'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CRSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTCRTRS,REGION=6M,PARM='TSTCRTRR',COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CRSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
