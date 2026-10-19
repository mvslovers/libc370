//TSTOSOM  JOB (SYS),'LIBC370 412',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #412 - osxopen()/osdopen() with NULL.  See test/mvs/tstosom.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTOSOG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.OOSCR
//IN       DD  UNIT=SYSDA,SPACE=(TRK,1),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//RED      EXEC PGM=TSTOSOR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.OOSCR
//IN       DD  UNIT=SYSDA,SPACE=(TRK,1),
//             DCB=(RECFM=FB,LRECL=80,BLKSIZE=800)
//
