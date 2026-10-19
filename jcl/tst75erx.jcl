//TST75ERA JOB (SYS),'LIBC370 161 A',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #161, two address spaces: TST75ERA and TST75ERB must run
//* AT THE SAME TIME (two initiators).  Each provokes its own global
//* error (failed socket()) 300 times, waits 0.16 s, and reads GETERROR
//* (code 3).  A read of the other job's code proves CerrGen is shared
//* across address spaces.  Before reading a zero, check IEF403I and
//* IEF404I in both job logs: the two must overlap.
//*
//*   RC=0 own error every round   RC=8 the other job's error was read
//*
//S1       EXEC PGM=TST75ERR,PARM='GEN=1',REGION=4096K,TIME=(1,0)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.XESCR
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
//TST75ERB JOB (SYS),'LIBC370 161 B',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//S1       EXEC PGM=TST75ERR,PARM='GEN=2',REGION=4096K,TIME=(1,0)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.XESCR
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
