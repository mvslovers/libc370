//TSTFX37  JOB (SYS),'LIBC370 539',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #539 - __fx37(): the x37 an ENOSPC write stood for.
//* See test/mvs/tstfx37.c.  STEPLIB is the scratch PDS recvfx37.jcl
//* restores into.  No red control: __fx37() is new.
//*
//* Run:     mvsdev JOB02347, 2026-10-15: CC 0000, 7/7.  Controls: IEBDG
//*          JOB02342 (D37-04, B37-04), JOB02343 (PDS: D37-04, E37-04).
//*
//* RC 0 = every check passed, 8 = a check failed.  An IEC031I in the
//* job log means an x37 abended: RED whatever the condition code.
//*
//GREEN    EXEC PGM=TSTFX37,REGION=4M,PARM='IBMUSER.TSTFX37'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FX37SCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net: the probe deletes its data sets itself, so these answer 8.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTFX37.A PURGE
  DELETE IBMUSER.TSTFX37.B PURGE
  DELETE IBMUSER.TSTFX37.C PURGE
  DELETE IBMUSER.TSTFX37.D PURGE
  DELETE IBMUSER.TSTFX37.E PURGE
  DELETE IBMUSER.TSTFX37.F PURGE
  DELETE IBMUSER.TSTFX37.G PURGE
  SET MAXCC=0
/*
