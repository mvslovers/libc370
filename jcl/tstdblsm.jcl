//TSTDBLSM JOB (SYS),'LIBC370 225',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #225 - how many digits of %e are right.  See test/mvs/tstdblsm.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTDBLSG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DSSCR
//SYSPRINT DD  SYSOUT=*
//RED      EXEC PGM=TSTDBLSR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DSSCR
//SYSPRINT DD  SYSOUT=*
//
