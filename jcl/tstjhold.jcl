//TSTJHOLD JOB (SYS),'LIBC370 79 HOLD',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1),TYPRUN=HOLD
//*
//* libc370 #79 - a job that waits between submit and start, so that
//* tstjsub.jcl can tell the submit time from the start time.  Submit,
//* wait, release with $AJnnnn, then run tstjsub.jcl.
//*
//NOP      EXEC PGM=IEFBR14
