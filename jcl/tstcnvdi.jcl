//TSTCNVDI JOB (SYS),'LIBC370 190',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #190 - float <-> long long conversions, __cmpdi2 and the
//* bit builtins, each reached through a cast or __builtin_ so cc370
//* emits the libcall.  See test/mvs/tstcnvdi.c.
//*
//* STEPLIB is the scratch PDS recvcnv.jcl restores into.
//*
//* Run:     mvsdev JOB00483, CC 0000, 565/565, 2026-09-27.  Proven red
//*          with a rounding defect (JOB00479, 39 failed) and an ABI
//*          defect (JOB00481, 21 failed).
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTCNVDI,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CNVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
