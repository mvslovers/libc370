//TSTCALOC JOB (SYS),'LIBC370 84',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #84 - calloc() masked nmemb*size to 24 bits: a product
//* of 16 MB or more silently became a tiny valid allocation that
//* the caller then overran.  Assertion-red probe (no abend):
//* pre-fix calloc(1,16M+9) returns 16 bytes and the checks FAIL
//* (RC=8); post-fix it returns NULL/ENOMEM and the job ends RC=0.
//* REGION=8M so that the legitimate calloc(1,5M) check has room.
//* See test/mvs/tstcaloc.c.
//*
//S1       EXEC PGM=TSTCALOC,REGION=8M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
