//TSTUASTM JOB (SYS),'LIBC370 125',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #125 - __uastep() keeps its CRT pointer across TESTAUTH.
//* See test/mvs/tstuastm.c.  GREEN is this tree, RED the installed
//* library.  Both are copied into SYS2.LINKLIB and run from there,
//* authorized: run this in a disposable container only.
//*
//COPY     EXEC PGM=IEBCOPY
//SYSPRINT DD  SYSOUT=*
//IN       DD  DISP=SHR,DSN=IBMUSER.LIBC370.UASCR
//OUT      DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSUT3   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSUT4   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSIN    DD  *
  COPY INDD=IN,OUTDD=OUT
  SELECT MEMBER=((TSTUASTG,,R),(TSTUASTR,,R))
/*
//GREEN    EXEC PGM=TSTUASTG,REGION=4M,COND=(0,NE,COPY),TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//RED      EXEC PGM=TSTUASTR,REGION=4M,COND=((0,NE,COPY),EVEN)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//
