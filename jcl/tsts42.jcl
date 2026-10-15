//TSTS42   JOB (SYS),'LIBC370 538',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #538 - out of space at 16 extents is ENOSPC, not EIO.
//* See test/mvs/tsts42.c.  STEPLIB is the scratch PDS recvs42.jcl
//* restores into.  GREEN: this tree's libc.a; RED: the installed one.
//*
//* Run:     mvsdev JOB02339, 2026-10-15: GREEN CC 0000, 3/3 (16 extents,
//*          3050 records: ENOSPC); RED CC 0008 (EIO).
//*
//* RC 0 = every check passed, 8 = a check failed.  An IEC031I in the
//* job log means an x37 abended: RED whatever the condition code.
//*
//GREEN    EXEC PGM=TSTS42,REGION=4M,PARM='IBMUSER.TSTS42G'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.S42SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//RED      EXEC PGM=TSTS42R,REGION=4M,PARM='IBMUSER.TSTS42R',COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.S42SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net: the probe deletes its data sets itself, so these answer 8.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTS42G.D37 PURGE
  DELETE IBMUSER.TSTS42G.X16 PURGE
  DELETE IBMUSER.TSTS42R.D37 PURGE
  DELETE IBMUSER.TSTS42R.X16 PURGE
  SET MAXCC=0
/*
