//TSTKEY0  JOB (SYS),'LIBC370 693',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #693 - what the library creates on first use inside a
//* key-0 window stays usable in key 8.  See test/mvs/tstkey0.c.
//* TSTKEY0G is this tree, TSTKEY0R the same probe on 2.24.0.  Both
//* need APF: they are copied into SYS2.LINKLIB for this job and
//* deleted again in the last step.  Writes SYS2.LINKLIB: an own
//* throwaway container only, or the maintainer's approval each time.
//* The results are WTOs in this job log; RC 0 = passed, 8 = failed.
//* Run: own MVS/CE container, 2026-10-20: GREEN 7/7, CC 0; RED 5
//*      FAIL (S0C4, S30A), step S0C4; both members deleted.
//*
//COPY     EXEC PGM=IEBCOPY
//SYSPRINT DD  SYSOUT=*
//IN       DD  DISP=SHR,DSN=IBMUSER.LIBC370.K0SCR
//OUT      DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSUT3   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSUT4   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSIN    DD  *
  COPY INDD=IN,OUTDD=OUT
  SELECT MEMBER=((TSTKEY0G,,R),(TSTKEY0R,,R))
/*
//GREEN    EXEC PGM=TSTKEY0G,REGION=4M,COND=(0,NE,COPY)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSENV   DD  *
K0VAR=ONE
/*
//RED      EXEC PGM=TSTKEY0R,REGION=4M,COND=((0,NE,COPY),EVEN)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SYSENV   DD  *
K0VAR=ONE
/*
//CLEAN    EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE 'SYS2.LINKLIB(TSTKEY0G)'
  DELETE 'SYS2.LINKLIB(TSTKEY0R)'
/*
//
