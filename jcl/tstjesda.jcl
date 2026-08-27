//TSTJESDA JOB (ACCT),'142 JES DISCOVERY',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1),NOTIFY=&SYSUID
//*
//* libc370 #142 - how does jesopen() find the JES2 checkpoint and spool
//* without a DD in the caller's JCL?  See test/mvs/tstjesda.c.
//*
//* NOTE THE ABSENCE.  There is deliberately NO HASPCKPT and NO HASPACE1
//* DD here: every route the probe measures has to work without them, or
//* it is not a route.  DYNAMNBR reserves the TIOT entries the dynamic
//* allocations in cases (3) and (5) need.
//*
//* Adjust: STEPLIB DSN.
//*
//S1       EXEC PGM=TSTJESDA,REGION=4M,DYNAMNBR=20
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
