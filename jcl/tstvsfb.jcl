//TSTVSFB  JOB (SYS),'LIBC370 625',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #625 - __vsfdbk(), the outcome of each VSAM request.
//* See test/mvs/tstvsfb.c.  STEPLIB is the scratch PDS recvvsfb.jcl
//* restores into.  TSTVSFBG uses __vsfdbk() and this tree's libc.a;
//* TSTVSFBR, the red control, reads the handle's codes with the
//* library of origin/main.  The KSDS is defined, loaded with three
//* records and deleted again by this job.  VOLUMES may need changing
//* on another system.
//*
//* Run:     own MVS/CE container JOB00019, 2026-10-20: GREEN CC 0000,
//*          13/13; RED CC 0001, 7/13 (codes left from the request
//*          before).
//*
//* RC 0 = every check passed, 1 = a check failed (the COND CODE).
//*
//DEFINE   EXEC PGM=IDCAMS
//SYSPRINT DD  SYSOUT=*
//IN       DD  *
KEY00001 FIRST OF THREE
KEY00002 SECOND OF THREE
KEY00003 THIRD OF THREE
/*
//SYSIN    DD  *
  DELETE IBMUSER.LIBC370.VSFBTST CLUSTER PURGE
  SET MAXCC = 0
  DEFINE CLUSTER (NAME(IBMUSER.LIBC370.VSFBTST) -
         INDEXED KEYS(8 0) RECORDSIZE(80 80) TRACKS(1 1) -
         VOLUMES(PUB000) UNIQUE) -
         DATA (NAME(IBMUSER.LIBC370.VSFBTST.DATA)) -
         INDEX (NAME(IBMUSER.LIBC370.VSFBTST.INDEX))
  REPRO INFILE(IN) OUTDATASET(IBMUSER.LIBC370.VSFBTST)
/*
//GREEN    EXEC PGM=TSTVSFBG,REGION=4M,COND=(0,NE,DEFINE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VSFBSCR
//VSAMKSDS DD  DISP=SHR,DSN=IBMUSER.LIBC370.VSFBTST
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* The green step wrote a duplicate attempt only; the data is as
//* loaded, so the red step sees the same three records.
//RED      EXEC PGM=TSTVSFBR,REGION=4M,COND=(0,NE,DEFINE)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.VSFBSCR
//VSAMKSDS DD  DISP=SHR,DSN=IBMUSER.LIBC370.VSFBTST
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//CLEANUP  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.LIBC370.VSFBTST CLUSTER PURGE
/*
