//TSTDSALC JOB (SYS),'LIBC370 43',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #43 - __dsalc() reports through its return code, not to the
//* operator console.
//*
//* Cases (3)-(5) are the regression guard: an unrecognized token in any of
//* the three DISP= positions used to be written to the console while the
//* return code stayed 0, so the allocation went ahead with no disposition
//* text unit at all.  All three must now fail.
//*
//* Case (6) is the scenario from the issue - creating a data set that
//* already exists.  Its check is not in SYSPRINT but in the SYSLOG: the
//* two TSTDSALC markers the program writes itself must be ADJACENT.  Three
//* lines from __dsalc() between them means #43 is back.
//*
//* PARM is the work data set.  It is created by case (1) and deleted by
//* (7), and must NOT exist when the job starts.
//*
//* See test/mvs/tstdsalc.c.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tstdsalc.c -o TSTDSALC \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every check passed, 8 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTDSALC,REGION=4M,PARM='IBMUSER.TSTDSALC.WORK'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
