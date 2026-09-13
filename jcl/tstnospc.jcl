//TSTNOSPC JOB (SYS),'LIBC370 149 ENOSPC',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #149 - what a stream DOES after ENOSPC.
//*
//* This job decides nothing.  It measures, so that #149's policy - which
//* errors are terminal and which are worth retrying - is written on numbers
//* instead of on a guess.  #176 made an out-of-space write arrive as
//* ferror() + ENOSPC instead of ABEND SD37; what it did not settle is what
//* the NEXT fwrite() on that stream does.
//*
//* Three questions, all answered in the job log:
//*   - the retry MAP: '.' is a write that returned full length, 'X' one
//*     that came up short.  At BLKSIZE 800 and LRECL 80 nine writes of ten
//*     only fill the FILE buffer, so if they read '.' a caller that checks
//*     the return value and not ferror() loses nine records in ten.
//*   - the COST: average microseconds for a full-length retry against a
//*     short one.  IFG0554T's x37 exit runs inside EOV; if every retry pays
//*     that round trip, retrying is expensive as well as futile.
//*   - whether retry DEGRADES: the last short retry must match the first,
//*     errno must never drift from ENOSPC to EIO, and nothing may abend.
//*
//* THE PROBE USES NO try().  If a retry abends, this step abends and the
//* job log carries the IEC031I - which says more than any return code.  So:
//*
//*   GREEN  = CC 0000 *and* NO IEC031I LINE IN THE JOB LOG.
//*   RED    = ABEND, or CC 0008 with a FAIL line.
//*
//* Every line also goes to the console (grep the job log for TSTNOSPC),
//* because an abend discards whatever SYSOUT is still in the QSAM buffer.
//*
//* The work data set must NOT exist when the job starts.  PARM overrides the
//* name (default IBMUSER.TSTNOSPC.WORK); the probe creates and deletes it.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recvnosp.jcl restores into.  It
//* MUST hold a module linked against a libc370 carrying the #176 x37 exit -
//* build with -L build/sdk, not against the installed sysroot.
//*
//S1       EXEC PGM=TSTNOSPC,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.NSPSCR
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
  DELETE IBMUSER.TSTNOSPC.WORK PURGE
  SET MAXCC=0
/*
//
