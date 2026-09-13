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
//* ONE CASE PER STEP, so each runs in a fresh address space and each COND
//* CODE is a verdict on its own.  Running them together made one case's
//* leftovers change the next one's answer - see the source.
//*
//*   GREEN   __fabandon() instead of fclose(), then remove() must answer 0
//*   SPLIT   fflush() and __aclose() under separate try()s: WHICH half takes
//*           the second abend
//*   FCLOSE  fclose(), the defect as a control: it abends and strands the DD
//*
//* None of the work data sets may exist when the job starts.  PARM takes the
//* step mode and an optional base name (default IBMUSER.TSTFABND); the cases
//* append .G, .S and .R.
//*
//* MEASURED 2026-09-13 on mvsdev - see the source header for the numbers.
//*
//* EXPECT S0D37 MESSAGES.  Three D37s are the point of the run, one per step,
//* and each step takes a second abend on purpose.  Every check is also
//* written to the console, so a step's record survives its own teardown:
//* grep the JES2 job log for TSTFABND.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvfabn.jcl restores into.
//*
//GREEN    EXEC PGM=TSTFABND,PARM='GREEN',REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//SPLIT    EXEC PGM=TSTFABND,PARM='SPLIT',REGION=4096K,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//FCLOSE   EXEC PGM=TSTFABND,PARM='FCLOSE',REGION=4096K,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net, not a measurement: every step deletes its own work data set on a
//* green run, so all three DELETEs are EXPECTED to answer 8.  They are here
//* for the paths where a step gave up early or could not clean up.  MAXCC is
//* reset so the job ends on the steps' verdicts and not on the cleanup.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTFABND.G PURGE
  DELETE IBMUSER.TSTFABND.S PURGE
  DELETE IBMUSER.TSTFABND.R PURGE
  SET MAXCC=0
/*
//
