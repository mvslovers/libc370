//TSTSPLNK JOB (SYS),'LIBC370 89',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #89 - subpool inheritance through LINK (T3) and the
//* abend-path reclaim (T4): TSTSPLNK LINKs TSTSPINR from this
//* STEPLIB eight times under __linkds; five of the S0C1 abends are
//* deliberately NOT reclaimed to prove the probe can see the leak.
//* Console note: one WTO per TSTSPINR run, malloc 'Out of memory'
//* + traceback on the red leg BY DESIGN, all abend dumps suppressed
//* by the ESTAE.  Must be COND CODE 0000.  REGION=8M is part of the
//* experiment - do not change it without redoing the arithmetic in
//* test/mvs/tstsplnk.c.
//*
//S1       EXEC PGM=TSTSPLNK,REGION=8M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
