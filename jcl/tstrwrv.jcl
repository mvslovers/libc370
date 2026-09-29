//TSTRWRV  JOB (SYS),'LIBC370 232 RWRITE',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #232 - rwrite() refuses a record @@AWRITE cannot take,
//* with EINVAL, instead of abending 002 or overrunning asmbuf; an
//* __awrite() failure sets ENOSPC/EIO.  See test/mvs/tstrwrv.c.
//*
//*   RED    = the RED* steps run TSTRWRD, the same source linked
//*            against the installed sysroot, one group per step
//*            because three of them abend: REDVB fails (3) and
//*            ends ABEND U1234 reading back the oversized record,
//*            REDBAD fails (5)/(6) and ends ABEND U0002 at (7),
//*            REDFB fails (11)/(12), REDVBS ends ABEND U0002 at
//*            (15), REDNOSPC fails (16).  mvsdev JOB00800.
//*   GREEN  = step GREEN ends CC 0000, no FAIL line.
//*
//* Every line also goes to the console (grep the job log for TSTRWRV).
//*
//* Build against build/sdk, not the installed sysroot:
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstrwrv.c \
//*           -o TSTRWRV -flinker-output=iebcopy
//*     cc370 -O1 -Iinclude test/mvs/tstrwrv.c \
//*           -o TSTRWRD -flinker-output=iebcopy
//*     ld370 --pack TSTRWRV=TSTRWRV.iebcopy TSTRWRD=TSTRWRD.iebcopy \
//*           -o probe -xmit --dsn IBMUSER.LIBC370.RWVSCR
//* and upload probe.xmit to IBMUSER.LIBC370.RWVXMIT - a private
//* staging data set, not the shared IBMUSER.MBT.XMIT.IN.  RECEIVE
//* cannot merge, so DEL scratches the load library first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.RWVSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.RWVXMIT')
/*
//*
//* The work data sets, in a step of their own.  SML is TRK(1,0) so
//* that the NOSPC group runs out of space.
//*
//SETUP    EXEC PGM=IEFBR14,COND=(0,NE,RECV)
//VB       DD  DSN=IBMUSER.TSTRWRV.VB,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=VB,LRECL=255,BLKSIZE=6233)
//FB       DD  DSN=IBMUSER.TSTRWRV.FB,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=FB,LRECL=80,BLKSIZE=800)
//VBS      DD  DSN=IBMUSER.TSTRWRV.VBS,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1)),
//             DCB=(DSORG=PS,RECFM=VBS,LRECL=255,BLKSIZE=6233)
//SML      DD  DSN=IBMUSER.TSTRWRV.SML,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,0)),
//             DCB=(DSORG=PS,RECFM=FB,LRECL=80,BLKSIZE=800)
//*
//REDVB    EXEC PGM=TSTRWRD,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV VB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDBAD   EXEC PGM=TSTRWRD,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV VBBAD'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDFB    EXEC PGM=TSTRWRD,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV FB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDVBS   EXEC PGM=TSTRWRD,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV VBS'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDNOSPC EXEC PGM=TSTRWRD,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV NOSPC'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//GREEN    EXEC PGM=TSTRWRV,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTRWRV ALL'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RWVSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTRWRV.VB PURGE
  DELETE IBMUSER.TSTRWRV.FB PURGE
  DELETE IBMUSER.TSTRWRV.VBS PURGE
  DELETE IBMUSER.TSTRWRV.SML PURGE
  SET MAXCC=0
/*
//
