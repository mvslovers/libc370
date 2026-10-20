//TSTDYNIT JOB (SYS),'LIBC370 52',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #52 - <dynit.h>: dynalloc(), dynfree(), dyninit().
//* See test/mvs/tstdynit.c.  There is no RED step: against a
//* library without <dynit.h> the probe does not compile.
//*
//* The data sets the probe creates go first, then the probe, then
//* LISTCAT and IEBGENER show what it left.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.T52S,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1))
//D2       DD  DSN=IBMUSER.LIBC370.T52P,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//GREEN    EXEC PGM=TSTDYNG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.DYSCR
//*
//LISTC    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  LISTCAT ENTRIES(IBMUSER.LIBC370.T52S) ALL
  LISTCAT ENTRIES(IBMUSER.LIBC370.T52P) ALL
/*
//SEQ      EXEC PGM=IEBGENER,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  DSN=IBMUSER.LIBC370.T52S,DISP=SHR
//SYSUT2   DD  SYSOUT=*
//MEM      EXEC PGM=IEBGENER,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  DUMMY
//SYSUT1   DD  DSN=IBMUSER.LIBC370.T52P(MEMA),DISP=SHR
//SYSUT2   DD  SYSOUT=*
//
