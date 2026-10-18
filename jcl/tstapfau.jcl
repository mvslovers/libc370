//TSTAPFAU JOB (SYS),'LIBC370 402',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #402 (PR #593), the authorized half: TSTAPFAU (this tree)
//* and TSTAPFAR (2.18.1) go into SYS2.LINKLIB for this job only and
//* are removed again in the last step, which runs even after an abend.
//* Needs the maintainer's approval each time (it writes SYS2.LINKLIB).
//* The delete step is a remove() of both members (STOW, DISP=SHR); its
//* module is built from a three-line program, see PR #593.
//* Run:     mvsdev JOB02458, 2026-10-18: GREEN 11/11; RED S0C2 in the
//*          first key-0 case; both members removed, 19 left.
//*
//COPY     EXEC PGM=IEBCOPY
//SYSPRINT DD  SYSOUT=*
//IN       DD  DISP=SHR,DSN=IBMUSER.LIBC370.AASCR
//OUT      DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSUT3   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSUT4   DD  UNIT=SYSDA,SPACE=(TRK,(5,5))
//SYSIN    DD  *
  COPY INDD=IN,OUTDD=OUT
  SELECT MEMBER=(TSTAPFAU,TSTAPFAR)
/*
//GREEN    EXEC PGM=TSTAPFAU,REGION=4M,COND=(0,NE,COPY)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//RED      EXEC PGM=TSTAPFAR,REGION=4M,COND=((0,NE,COPY),EVEN)
//STEPLIB  DD  DISP=SHR,DSN=SYS2.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//SCRATCH  EXEC PGM=DELMEM,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ADSCR
//SYSPRINT DD  SYSOUT=*
//
