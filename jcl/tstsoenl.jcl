//TSTSOENL JOB (SYS),'LIBC370 601',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #601 - after jcl/tstsoenk.jcl was cancelled: bind its port.
//*
//L        EXEC PGM=TSTSOLG,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//
