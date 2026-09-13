//TSTX37   JOB (SYS),'LIBC370 176 X37 EXIT',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #176 - an out-of-space write is a return code, not an ABEND.
//*
//* @@AOPEN plants an EXLST type X'08' exit.  IFG0554T scans the exit list
//* for it before it abends and takes R15=1 as "rewrite the format-1 DSCB and
//* drive the caller's SYNAD with output error, no space available", which is
//* the path #147 already built - so the condition arrives as ferror() +
//* ENOSPC and __fflush() reaches its reset: label, leaving nothing for
//* fclose() to re-drive.
//*
//* THE PROBE USES NO try().  If the exit is not planted, is not taken, or
//* returns the wrong code, this step ABENDS SD37 and the job log carries
//* IEC031I.  So:
//*
//*   GREEN  = CC 0000 *and* NO IEC031I LINE IN THE JOB LOG.
//*   RED    = ABEND SD37, or CC 0008 with a FAIL line.
//*
//* Every check is also written to the console (grep the job log for TSTX37),
//* because an abend discards whatever SYSOUT is still in the QSAM buffer.
//*
//* The work data set must NOT exist when the job starts.  PARM overrides the
//* name (default IBMUSER.TSTX37.WORK); the probe creates and deletes it.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvx37.jcl restores into.  It
//* MUST hold a module linked against a libc370 carrying the exit - build
//* with -L build/sdk, not against the installed sysroot.
//*
//S1       EXEC PGM=TSTX37,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.X37SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net, not a measurement: the probe deletes the data set itself on a
//* green run, so this DELETE is expected to answer 8.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTX37.WORK PURGE
  SET MAXCC=0
/*
//
