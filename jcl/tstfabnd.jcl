//TSTFABND JOB (SYS),'LIBC370 168 ABANDON',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #168 - __fabandon(): close a FILE whose last write failed.
//*
//* The host test (test/host/tstfabnd.c) pins the shape of the call.  This
//* job answers what it cannot: whether CLOSE with nothing pending COMPLETES
//* on a data set that is out of space - BSAM CLOSE writes the EOF mark and
//* can reach EOV - and whether the DD then really goes, so the partial data
//* set can be scratched from the SAME address space that hit the abend.
//*
//* Both cases use mvslovers/ftpd's shape (ftpd#129): __dsalcf() creates with
//* SPACE=TRK(1,0), __dsfree() catalogs, fopen(dsn,"wb") reopens by name, and
//* fwrite() runs into a D37 under try().
//*
//*   GREEN step: __fabandon(), then remove() must answer 0.  CC 0000 is the
//*               verdict.
//*   RED   step: fclose(), which abends again and strands the DD; then a
//*               third case that splits fclose() into fflush() and
//*               __aclose() to say WHICH half takes the second abend.
//*               This step ENDS ABEND SC03 by construction - see below.
//*
//* None of the work data sets may exist when the job starts.  PARM takes the
//* step mode and an optional base name (default IBMUSER.TSTFABND); the cases
//* append .G, .S and .R.
//*
//* MEASURED 2026-09-13 on mvsdev, JOB00241: GREEN CC 0000, RED 7/7 PASS and
//* ABEND SC03 as described below.
//*
//* EXPECT S0D37 MESSAGES AND AN ABENDING RED STEP.  Three D37s are the point
//* of the run.  The GREEN step's COND CODE is the verdict; the RED step's is
//* not, and cannot be.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvfabn.jcl restores into.
//*
//GREEN    EXEC PGM=TSTFABND,PARM='GREEN',REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* The RED step is a SEPARATE step because it cannot end cleanly.  It leaves
//* behind a DCB that MVS CLOSE could not finish, and task termination closes
//* it again: IFG0TC0A takes the same failure and the step ends ABEND SC03.
//* That is BY CONSTRUCTION - the defect is a data set that cannot be closed -
//* so this step's COND CODE is not a verdict.  Its record is the TSTFABND
//* WTOs in the JES2 job log; three of them are PASS lines and the rest are
//* measurements.  Keeping it out of the GREEN step is what lets that step's
//* COND CODE stay meaningful.
//*
//RED      EXEC PGM=TSTFABND,PARM='RED',REGION=4096K,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net, not a measurement.  The GREEN step deletes .G itself; the RED step
//* leaves .R and .S stranded on purpose - it cannot delete them, which IS the
//* defect.  A different address space can, and this step succeeding on .R/.S
//* is the same evidence the issue recorded on 2026-09-09.  DELETE of .G is
//* expected to answer 8; MAXCC is reset so the job ends on the GREEN step's
//* verdict and not on the cleanup.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTFABND.R PURGE
  DELETE IBMUSER.TSTFABND.S PURGE
  DELETE IBMUSER.TSTFABND.G PURGE
  SET MAXCC=0
/*
//
