//TSTATOM  JOB (SYS),'LIBC370 48',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #48 - the atomics.  __swap() exchanges and returns the previous
//* value; __cas() compares first and, when the comparison fails, leaves
//* memory alone and reports what is there instead.
//*
//* Case (1) is the regression guard for the defect __swap() inherited from
//* __cs(): it stored the word AT new_value instead of new_value.
//*
//* See test/mvs/tstatom.c.  MVS only - both are inline assembler.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tstatom.c -o TSTATOM \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTATOM,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
