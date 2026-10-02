//TSTSTRF  JOB (SYS),'LIBC370 314',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #314 - strtof() and strtold().  See test/mvs/tststrf.c.
//* STEPLIB is the scratch PDS recvstrf.jcl restores into.
//*
//* GREEN links libc.a's strtof(); RED converts the probe with a naive
//* (float)strtod() and is expected to end S0CC.
//*
//* Run:     mvsdev JOB01159, 2026-10-02: GREEN CC 0000, 15/15;
//*          RED ABEND S0CC.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//GREEN    EXEC PGM=TSTSTRF,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STRFSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTSTRR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STRFSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
