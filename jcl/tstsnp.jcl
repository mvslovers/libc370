//TSTSNP   JOB (SYS),'LIBC370 339',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #339 - snprintf/vsnprintf size_t, setbuf void.  See
//* test/mvs/tstsnp.c.  STEPLIB is the scratch PDS recvsnp.jcl
//* restores into.
//*
//* Run:     mvsdev JOB01313, 2026-10-03: GREEN CC 0000, 5/5.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTSNP,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SNPSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
