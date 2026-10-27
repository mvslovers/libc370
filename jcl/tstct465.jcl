//TSTCT465 JOB (SYS),'LIBC370 465',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #465 - a subtask that ends while cthread_create() is still
//* returning.  See test/mvs/tstct465.c.  STEPLIB is the scratch PDS
//* recvct465.jcl restores into.  TSTC465 links this tree's libc.a,
//* TSTC465R a libc.a built from main before the fix - the red control.
//*
//* Needs two CPUs: on one the subtask runs only when the owner waits
//* and both programs pass.
//*
//* Run:     2 CPUs, local MVS/CE container, 2026-10-27:
//*          GREEN CC 0000 in all three steps (3 x 300 rounds, every
//*          counter 0);
//*          RED   ABEND S43E in the first two steps, with
//*          "cthread_delete: TCB has not ended, task and stack
//*          retained" in the job log.
//*
//* RC 0 = every round passed, 1 = a check failed (it is the COND CODE).
//* Run RED in a job of its own: the corrupted storage can leave
//* initiators drained.
//*
//GREEN    EXEC PGM=TSTC465,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.C465SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTC465R,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.C465SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
