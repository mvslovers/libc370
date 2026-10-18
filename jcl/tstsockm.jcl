//TSTSOCKM JOB (SYS),'LIBC370 396',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #396 - the socket layer.  See test/mvs/tstsockm.c.  GREEN
//* this tree, RED the installed library (its select() waits ~125 s).
//* Case 5 is read from SYSPRINT: empty is the pass.
//*
//GREEN    EXEC PGM=TSTSOCKG,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SOSCR
//SYSPRINT DD  SYSOUT=*
//RED      EXEC PGM=TSTSOCKR,REGION=4M,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SOSCR
//SYSPRINT DD  SYSOUT=*
//
