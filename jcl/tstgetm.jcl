//TSTGETM  JOB (SYS),'LIBC370 81',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #81 - malloc() could not fail: @@GETM issued GETMAIN RU
//* (unconditional), so storage shortage abended S878 instead of
//* returning NULL.  This job runs TSTGETM in a deliberately small
//* region: pre-fix it abends S878 in the exhaustion loop, post-fix
//* it ends RC=0.  See test/mvs/tstgetm.c.
//*
//* Expect a few 'Out of memory' WTOs plus save area tracebacks on
//* the console - that is the (previously unreachable) diagnostic
//* firing by design, not a failure.
//*
//S1       EXEC PGM=TSTGETM,REGION=2048K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
