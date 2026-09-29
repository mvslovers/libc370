//TSTFWREC JOB (SYS),'LIBC370 236 FWRITE',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #236 - fwrite() in record mode refuses a record @@AWRITE
//* cannot take, with EINVAL, and writes nothing for size or nmemb 0.
//* See test/mvs/tstfwrec.c for the checks.
//*
//*   RED    = the RED* steps run TSTFWRED, the same source linked
//*            against the installed sysroot, one group per step
//*            because three of them abend: REDVB U1234 reading
//*            back the oversized record of (3), REDBAD U0002 at (8),
//*            REDVBS U0002 at (19); REDFB fails (12)-(16).  mvsdev
//*            JOB00805.
//*   GREEN  = step GREEN ends CC 0000, no FAIL line.
//*
//* Every line also goes to the console (grep the log for TSTFWREC).
//*
//* Build against build/sdk, not the installed sysroot:
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfwrec.c \
//*           -o TSTFWREC -flinker-output=iebcopy
//*     cc370 -O1 -Iinclude test/mvs/tstfwrec.c \
//*           -o TSTFWRED -flinker-output=iebcopy
//*     ld370 --pack TSTFWREC=TSTFWREC.iebcopy \
//*           TSTFWRED=TSTFWRED.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.FWRSCR
//* and upload probe.xmit to IBMUSER.LIBC370.FWRXMIT - a private
//* staging data set, not the shared IBMUSER.MBT.XMIT.IN.  RECEIVE
//* cannot merge, so DEL scratches the load library first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.FWRSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.FWRXMIT')
/*
//*
//* The work data sets, in a step of their own.
//*
//SETUP    EXEC PGM=IEFBR14,COND=(0,NE,RECV)
//VB       DD  DSN=IBMUSER.TSTFWREC.VB,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=VB,LRECL=255,BLKSIZE=6233)
//FB       DD  DSN=IBMUSER.TSTFWREC.FB,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=FB,LRECL=80,BLKSIZE=800)
//VBS      DD  DSN=IBMUSER.TSTFWREC.VBS,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=VBS,LRECL=255,BLKSIZE=6233)
//*
//REDVB    EXEC PGM=TSTFWRED,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTFWREC VB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FWRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDBAD   EXEC PGM=TSTFWRED,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTFWREC VBBAD'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FWRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDFB    EXEC PGM=TSTFWRED,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTFWREC FB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FWRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDVBS   EXEC PGM=TSTFWRED,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTFWREC VBS'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FWRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//GREEN    EXEC PGM=TSTFWREC,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTFWREC ALL'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.FWRSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTFWREC.VB PURGE
  DELETE IBMUSER.TSTFWREC.FB PURGE
  DELETE IBMUSER.TSTFWREC.VBS PURGE
  SET MAXCC=0
/*
//
