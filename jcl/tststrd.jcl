//TSTSTRD  JOB (SYS),'LIBC370 316',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #316 - strtod() over the whole HFP range.  See
//* test/mvs/tststrd.c.  STEPLIB is the scratch PDS recvstrd.jcl
//* restores into.  The red evidence is JOB01177 (three S0CC probes).
//*
//* Run:     mvsdev JOB01179, 2026-10-03: GREEN CC 0000, 32/32.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTSTRD,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STRDSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
