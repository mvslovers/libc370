//TSTCRTLK JOB (SYS),'LIBC370 96',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #96 - a caught abend of a LINKed program must cost
//* ~nothing durable.  Four rounds of __linkds(TSTPPAIN) abending
//* S0C1 under ambient subpool 7, free storage counted by 64K+4K
//* drain before/after each abend and after FREEMAIN SP=7; the
//* steady-state rounds must cost <= 16K (pre-#96: 172K per round,
//* 132K of it open stdio FILEs no subpool release can reach).
//* Plus the composition experiments that pinned the leak (big
//* module, local abend, LOAD+BALR+DELETE).  Numbers land on the
//* console (wtof).  Must be COND CODE 0000.
//*
//S1       EXEC PGM=TSTCRTLK,REGION=6M,TIME=2
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
