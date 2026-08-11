//TSTPPAFR JOB (SYS),'LIBC370 93',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #93 - a caught abend in a LINKed program must not leak
//* the ~262K @@CRT0 stack+PPA block.  TSTPPAFR drives TSTPPAIN
//* (12 single-level S0C1s) and TSTPPAMD -> TSTPPAIN (6 nested
//* S0C1s) under __linkds, plus normal-return (no double free) and
//* garbage-at-8(TCBFSAB) (free nothing) legs, then proves the
//* region is whole with one malloc(4M): pre-fix ~6.2M of abandoned
//* stacks make it fail, post-fix it succeeds.
//* Console note: one WTO per TSTPPAIN/TSTPPAMD run, all abend
//* dumps suppressed by the ESTAE.  Must be COND CODE 0000.
//* REGION=8M is part of the experiment - do not change it without
//* redoing the arithmetic in test/mvs/tstppafr.c.
//*
//S1       EXEC PGM=TSTPPAFR,REGION=8M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
