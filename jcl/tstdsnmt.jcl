//TSTDSNMT JOB (SYS),'LIBC370 181 NOMNT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #181 - __dsalc() with UNIT= or VOLSER= sets S99NOMNT.
//*
//* The host test (test/host/tstdsnmt.c) pins the flag byte.  This job
//* answers what it cannot: a mounted volume still allocates, and an
//* unmounted one is refused at once instead of waiting on IEF238D.
//*
//* PARM is a mounted volume reachable through UNIT=SYSDA (default
//* PUB001).  The work data set IBMUSER.LIBC370.DSNMT.WORK must NOT
//* exist when the job starts.
//*
//* Case (3) names a volume that does not exist.  An IEF238D in the job
//* log fails it, whatever the probe printed - the probe cannot see the
//* operator wait from inside.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvdsnm.jcl restores into.
//*
//S1       EXEC PGM=TSTDSNMT,REGION=4096K,PARM='PUB001'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DSNMTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
