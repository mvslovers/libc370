//TSTANCHR JOB (SYS),'LIBC370 85',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #85 - NULL guards on every __crtget()/__grtget()
//* dereference, paired arrayadd rollbacks, and a @@GRTSET rc test
//* at startup.  This probe exercises every touched happy path -
//* anchors, env, atexit (WTO in this job log), cthread push/pop,
//* mutexes, gmtime, a firing timer - and must be COND CODE 0000
//* on BOTH sides of the change.  TIME=1 guards the ecb_wait.
//* See test/mvs/tstanchr.c.
//*
//S1       EXEC PGM=TSTANCHR,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
