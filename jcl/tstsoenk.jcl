//TSTSOENK JOB (SYS),'LIBC370 601',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #601 - the CANCEL case of test/mvs/tstsoenm.c: TSTSOKG
//* (this tree) waits holding port 24606; cancel the job (C TSTSOENK)
//* once its TSTSOENM line is on the console, then submit
//* jcl/tstsoenl.jcl, which binds the port.  For the installed library
//* change G to R and 24606 to 24616 in both members.
//*
//K        EXEC PGM=TSTSOKG,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SESCR
//
