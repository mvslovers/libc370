//TST75PRT JOB (SYS),'LIBC370 165',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #165 - does a PARTIAL X'75' RECV consume what it returned?
//* A loopback pair the program owns both ends of: the peer sends P of
//* 636 bytes (pattern i % 251), one RECV for 636 gets the short count,
//* the peer sends the rest, one RECV for the remainder.  Checks where
//* the continuation starts and what FIONREAD says around each call.
//* See test/mvs/tst75prt.c.  Result: TST75PRT SUMMARY lines.
//*
//*   RC=0 short counts happened, every continuation correct
//*   RC=4 a short count never happened - NO verdict
//*   RC=8 replay, corruption or bytes left queued
//*
//S1       EXEC PGM=TST75PRT,REGION=4096K,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.XESCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
