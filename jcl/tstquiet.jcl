//TSTQUIGA JOB  (SYS),'LIBC370 712 A',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #712 - what a cancelled job writes to the console.  See
//* test/mvs/tstquiet.c.  Six independent jobs, submit them one at a
//* time: each writes "TSTQUIET x READY" and waits, then is cancelled
//* with C jobname.  GREEN (TSTQUIG) is this tree, RED (TSTQUIR) a build
//* of main before the change.  A..C are the PARM of the program.
//* Count "libc370 recovery:" and "@@@try.c" in the job log:
//*   A  green 0 / 0     red 1 / 0
//*   B  green 0 / 0     red 0 / 2
//*   C  green 1 / 0     red 1 / 0
//* Run: own MVS/CE container, 2026-10-27, C jobname: all six ended
//*      S222 with exactly these counts.
//*
//RUN      EXEC PGM=TSTQUIG,PARM='A',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQUIGB JOB  (SYS),'LIBC370 712 B',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUIG,PARM='B',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQUIGC JOB  (SYS),'LIBC370 712 C',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUIG,PARM='C',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQUIRA JOB  (SYS),'LIBC370 712 A',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUIR,PARM='A',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQUIRB JOB  (SYS),'LIBC370 712 B',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUIR,PARM='B',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//*
//TSTQUIRC JOB  (SYS),'LIBC370 712 C',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//RUN      EXEC PGM=TSTQUIR,PARM='C',REGION=4M,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.QUISCR
//
