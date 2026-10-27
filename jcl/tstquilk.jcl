//TSTQLGA  JOB  (SYS),'LIBC370 714 A',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #714 - what a cancelled job writes to the console while a task
//* sits inside __linkds() / __linkdsx(), and the abend report of a called
//* program.  See test/mvs/tstquilk.c.  Eight independent jobs, submit them
//* one at a time.  A and B write "TSTQUILK x READY: cancel the job" and
//* wait: cancel them with C jobname.  X and Y end by themselves.
//* GREEN (TSTQUILG) is this tree, RED (TSTQUILR) a build of main before
//* the change.  Count "libc370 recovery:" in the job log of A and B:
//*   A  green 0     red 1
//*   B  green 0     red 1
//* X and Y: the "ABEND S0C1 detected ..." and "PSW:" lines are the same on
//* green and red (41 lines each, S0C1 in the called program).
//* Run: own MVS/CE container, 2026-10-27, C jobname: exactly these counts.
//*
//RUN      EXEC PGM=TSTQUILG,PARM='A',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLGB  JOB  (SYS),'LIBC370 714 B',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILG,PARM='B',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLGX  JOB  (SYS),'LIBC370 714 X',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILG,PARM='X',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLGY  JOB  (SYS),'LIBC370 714 Y',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILG,PARM='Y',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLRA  JOB  (SYS),'LIBC370 714 A',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILR,PARM='A',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLRB  JOB  (SYS),'LIBC370 714 B',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILR,PARM='B',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLRX  JOB  (SYS),'LIBC370 714 X',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILR,PARM='X',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQLRY  JOB  (SYS),'LIBC370 714 Y',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUILR,PARM='Y',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//
