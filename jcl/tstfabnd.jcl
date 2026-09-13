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
//*   (1) green: __fabandon(), then remove() must answer 0
//*   (2) red control: fclose(), which abends again and strands the DD
//*
//* Neither work data set may exist when the job starts.  PARM overrides the
//* base name (default IBMUSER.TSTFABND); the cases append .G and .R.
//*
//* EXPECT S0D37 MESSAGES.  Two D37s are the point of the run, and case (2)
//* takes a third inside fclose().  The probe's COND CODE is the verdict,
//* not the IEC031I lines.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvfabn.jcl restores into.
//*
//S1       EXEC PGM=TSTFABND,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.ABNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net, not a measurement.  The probe deletes both work data sets itself -
//* case (2) rescues its stranded FILE with __fabandon() and then removes the
//* data set - so both DELETEs here are EXPECTED to answer 8.  They are here
//* for the paths where the probe gave up early.  MAXCC is reset so the job
//* ends on the probe's verdict and not on the cleanup.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTFABND.R PURGE
  DELETE IBMUSER.TSTFABND.G PURGE
  SET MAXCC=0
/*
//
