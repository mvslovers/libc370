//TSTLL    JOB (SYS),'LIBC370 314',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #314 - strtoll, strtoull, atoll, llabs, lldiv and the
//* long long limits.  See test/mvs/tstll.c.
//*
//* STEPLIB is the scratch PDS recvll.jcl restores into.  There is no
//* red step: before #314 the probe does not link.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTLL,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.LLSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
