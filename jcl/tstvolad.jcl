//TSTVOLAD JOB (SYS),'LIBC370 143',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #143 - delete and rename a data set, and the members of a
//* PDS, by name and volume.  See test/mvs/tstvolad.c.  Run
//* jcl/setvolad.jcl first; this job names no data set of the test,
//* so it holds none of their ENQs.
//*
//* RED is the old API (remove(), rename(), __delmem()), which cannot
//* reach the uncataloged copies: RC 8, nothing changed.  GREEN is this
//* tree: RC 0.  The two IEHLIST steps list the VTOC entries before and
//* after, WORK00 the cataloged copy, WORK01 and PUB001 the others.
//*
//BEFORE   EXEC PGM=IEHLIST
//SYSPRINT DD  SYSOUT=*
//W0       DD  UNIT=3350,VOL=SER=WORK00,DISP=OLD
//W1       DD  UNIT=3350,VOL=SER=WORK01,DISP=OLD
//P1       DD  UNIT=3390,VOL=SER=PUB001,DISP=OLD
//SYSIN    DD  *
  LISTVTOC VOL=3350=WORK00
  LISTVTOC VOL=3350=WORK01
  LISTVTOC VOL=3390=PUB001
/*
//RED      EXEC PGM=TSTVOLR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VOSCR
//GREEN    EXEC PGM=TSTVOLG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VOSCR
//AFTER    EXEC PGM=IEHLIST,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//W0       DD  UNIT=3350,VOL=SER=WORK00,DISP=OLD
//W1       DD  UNIT=3350,VOL=SER=WORK01,DISP=OLD
//P1       DD  UNIT=3390,VOL=SER=PUB001,DISP=OLD
//SYSIN    DD  *
  LISTVTOC VOL=3350=WORK00
  LISTVTOC VOL=3350=WORK01
  LISTVTOC VOL=3390=PUB001
/*
//
