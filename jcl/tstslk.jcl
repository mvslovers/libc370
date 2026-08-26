//TSTSLK   JOB (SYS),'LIBC370 147',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #147 item 4 - the DEQ branch of __enqdeq() dropped the
//* scope bits, so sysunlock()'s DEQ went out SCOPE=STEP against a
//* SCOPE=SYSTEM ENQ and never released.  This probe measures the
//* consequence: RED = the re-syslock answers 8 (still held), GREEN =
//* release and re-acquire both rc=0.  Verdicts via wtof() in this
//* job log.  See test/mvs/tstslk.c.
//*
//S1       EXEC PGM=TSTSLK,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
