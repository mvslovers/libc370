//TSTSPLNK JOB (SYS),'LIBC370 89',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #89 - subpool inheritance through LINK (T3) and the
//* abend-path reclaim (T4): TSTSPLNK LINKs TSTSPINR from this
//* STEPLIB ten times under __linkds; six of the nine S0C1 abends
//* are deliberately NOT reclaimed to prove the probe can see the
//* leak.  (Six, not five: since #93 the inner @@CRT0 stacks no
//* longer leak, see test/mvs/tstsplnk.c.)
//* Console note: one WTO per TSTSPINR run, all abend dumps
//* suppressed by the ESTAE.  Must be COND CODE 0000.  REGION=8M is
//* part of the experiment - do not change it without redoing the
//* arithmetic in test/mvs/tstsplnk.c.
//*
//S1       EXEC PGM=TSTSPLNK,REGION=8M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
