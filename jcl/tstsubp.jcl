//TSTSUBP  JOB (SYS),'LIBC370 89',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #89 T0 gate - subpool ownership across TCBs, measured
//* BEFORE any #89 code is written.  Two cthread subtasks and the
//* main task GETMAIN/FREEMAIN subpool 5 against each other; one
//* subtask issues the FREEMAIN SP=5 release httpd#154 wants to use.
//* The verdict WTO (TSTSUBP verdict: ...) in this job log is the
//* fact #89 is built on - record it on the issue either way.
//* TIME=1 guards the ecb_wait handshakes.  See test/mvs/tstsubp.c.
//*
//S1       EXEC PGM=TSTSUBP,REGION=4M,TIME=1
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
