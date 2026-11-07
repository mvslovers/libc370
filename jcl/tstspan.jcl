//TSTSPAN  JOB (SYS),'LIBC370 757 SPANNED',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #757 - a record written across a block boundary of a
//* spanned data set keeps all of its segments, and the data set stays
//* readable.  See test/mvs/tstspan.c.
//*
//*   GREEN  = step GREEN runs TSTSPAN, linked against build/sdk, all
//*            four groups: ends CC 0000, no FAIL line.  Then IBM's own
//*            readers take the data sets GREEN wrote: IDCAMS PRINT of
//*            VBS and VB (every record listed, CC 0000), IEBGENER of
//*            FWR and BIG (CC 0000), and a DUMP of the raw blocks of
//*            VBS as RECFM=U, where the SDWs can be read (flags 01,
//*            03, 02 on the three segments of the 247-byte record).
//*   RED    = the RED* steps run TSTSPAND, the same source linked
//*            against the installed sysroot, one group per step
//*            because each ends in ABEND U1234 (@@AREAD BADBLOCK) in
//*            its read-back, after the FAIL lines of the write.
//*
//* Every line also goes to the console (grep the job log for TSTSPAN).
//*
//* Build against build/sdk, not the installed sysroot:
//*     make build
//*     cc370 -O1 -Wall -Iinclude -L build/sdk test/mvs/tstspan.c \
//*           -o TSTSPAN -flinker-output=iebcopy
//*     cc370 -O1 -Wall -Iinclude test/mvs/tstspan.c \
//*           -o TSTSPAND -flinker-output=iebcopy
//*     ld370 --pack TSTSPAN=TSTSPAN.iebcopy TSTSPAND=TSTSPAND.iebcopy \
//*           -o probe -xmit --dsn IBMUSER.LIBC370.SPNSCR
//* and upload probe.xmit (binary) to IBMUSER.LIBC370.SPNXMIT (FB 80),
//* a private staging data set, not the shared IBMUSER.MBT.XMIT.IN.
//* RECEIVE cannot merge, so DEL scratches the load library first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.SPNSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.SPNXMIT')
/*
//*
//* The work data sets, in a step of their own.  VBS and FWR have a
//* block of 100 bytes so that a 247-byte record spans three blocks;
//* VB has BLKSIZE < LRECL+4, which the OPEN exit turns into spanned;
//* BIG is the issue's 100 x 200 bytes into 6233-byte blocks.
//*
//SETUP    EXEC PGM=IEFBR14,COND=(0,NE,RECV)
//VBS      DD  DSN=IBMUSER.TSTSPAN.VBS,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(DSORG=PS,RECFM=VBS,LRECL=255,BLKSIZE=100)
//FWR      DD  DSN=IBMUSER.TSTSPAN.FWR,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(DSORG=PS,RECFM=VBS,LRECL=255,BLKSIZE=100)
//VB       DD  DSN=IBMUSER.TSTSPAN.VB,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(2,1)),
//             DCB=(DSORG=PS,RECFM=VB,LRECL=255,BLKSIZE=255)
//BIG      DD  DSN=IBMUSER.TSTSPAN.BIG,DISP=(NEW,CATLG,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(5,2)),
//             DCB=(DSORG=PS,RECFM=VBS,LRECL=255,BLKSIZE=6233)
//*
//GREEN    EXEC PGM=TSTSPAN,REGION=4096K,COND=(0,NE,SETUP),
//             PARM='IBMUSER.TSTSPAN ALL'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SPNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* IBM's readers on what GREEN wrote.  PRINT lists every record of VBS
//* and VB; the RAW DD reads the blocks of VBS as RECFM=U.
//*
//IBMPRINT EXEC PGM=IDCAMS,COND=(0,NE,SETUP)
//SYSPRINT DD  SYSOUT=*
//VBS      DD  DSN=IBMUSER.TSTSPAN.VBS,DISP=SHR
//VB       DD  DSN=IBMUSER.TSTSPAN.VB,DISP=SHR
//RAW      DD  DSN=IBMUSER.TSTSPAN.VBS,DISP=SHR,
//             DCB=(RECFM=U,BLKSIZE=32760)
//SYSIN    DD  *
  PRINT INFILE(VBS) CHARACTER
  PRINT INFILE(VB) CHARACTER
  PRINT INFILE(RAW) DUMP
/*
//*
//IBMFWR   EXEC PGM=IEBGENER,COND=(0,NE,SETUP)
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  DSN=IBMUSER.TSTSPAN.FWR,DISP=SHR
//SYSUT2   DD  DUMMY,DCB=(RECFM=VBS,LRECL=255,BLKSIZE=6233)
//SYSIN    DD  DUMMY
//*
//IBMBIG   EXEC PGM=IEBGENER,COND=(0,NE,SETUP)
//SYSPRINT DD  SYSOUT=*
//SYSUT1   DD  DSN=IBMUSER.TSTSPAN.BIG,DISP=SHR
//SYSUT2   DD  DUMMY,DCB=(RECFM=VBS,LRECL=255,BLKSIZE=6233)
//SYSIN    DD  DUMMY
//*
//* The same groups with the installed sysroot's libc.  Each group
//* rewrites its data set and abends reading it back.
//*
//REDVBS   EXEC PGM=TSTSPAND,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTSPAN VBS'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SPNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDFWR   EXEC PGM=TSTSPAND,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTSPAN FWRITE'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SPNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDVB    EXEC PGM=TSTSPAND,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTSPAN VB'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SPNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//REDBIG   EXEC PGM=TSTSPAND,REGION=4096K,COND=((0,NE,SETUP),EVEN),
//             PARM='IBMUSER.TSTSPAN BIG'
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SPNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  DUMMY
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTSPAN.VBS PURGE
  DELETE IBMUSER.TSTSPAN.FWR PURGE
  DELETE IBMUSER.TSTSPAN.VB PURGE
  DELETE IBMUSER.TSTSPAN.BIG PURGE
  SET MAXCC=0
/*
//
