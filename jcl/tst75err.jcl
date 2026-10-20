//TST75ERR JOB (SYS),'LIBC370 161',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #161 - can the error a program reads back through X'75'
//* (GETERRORS code 2, GETERROR code 3) be somebody else's?  One
//* address space, 50 rounds of: crossed slot (error on X, other error
//* on Y, read X), stale slot (fresh socket reads its predecessor's
//* error), crossed global (failed socket(), then GETERROR).  See
//* test/mvs/tst75err.c.  Then 10 rounds of select() on a socket
//* number that a subtask closes and takes again under it: the errno
//* select() reports.  Result: TST75ERR SUMMARY lines.
//*
//* GREEN is TST75ERG (this tree), RED is TST75ERR (the library it
//* is compared against).  Only the select() part uses the library.
//*
//*   RC=0 nothing crossed or stale   RC=4 a condition not created
//*   RC=8 a read returned an error that was not the reader's own
//*
//GREEN    EXEC PGM=TST75ERG,REGION=4096K,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.XESCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RED      EXEC PGM=TST75ERR,REGION=4096K,TIME=(0,30),COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.XESCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//
