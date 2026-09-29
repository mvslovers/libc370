//TSTCLSPC JOB (SYS),'LIBC370 182 CLOSE',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #182 - an out-of-space on the block CLOSE writes.
//*
//* The probe fills a TRK(1,0) data set to learn how many records fit in
//* full blocks (R), then rewrites it with R+1 .. R+9 records, so the last
//* block is a short one that only CLOSE writes.  fclose() must answer EOF
//* (errno ENOSPC) exactly when the read-back comes up short.
//*
//*   GREEN  = CC 0000 in step S1, no FAIL line.
//*   RED    = CC 0008; before the fix check (4) fails - fclose() says 0
//*            while the short block is gone.
//*
//* Every line also goes to the console (grep the job log for TSTCLSPC).
//*
//* Build against build/sdk, not the installed sysroot:
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstclspc.c \
//*           -o TSTCLSPC -flinker-output=iebcopy
//*     ld370 --pack TSTCLSPC=TSTCLSPC.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.CLSSCR
//* and upload probe.xmit to IBMUSER.LIBC370.CLSXMIT - a private staging
//* data set, not the shared IBMUSER.MBT.XMIT.IN.  RECEIVE cannot merge,
//* so DEL scratches the load library first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.CLSSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.LIBC370.CLSXMIT')
/*
//*
//S1       EXEC PGM=TSTCLSPC,REGION=4096K,COND=(0,NE,RECV)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.CLSSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//* A net: the probe deletes the data set itself on a green run.
//*
//SCRATCH  EXEC PGM=IDCAMS,COND=EVEN
//SYSPRINT DD  SYSOUT=*
//SYSIN    DD  *
  DELETE IBMUSER.TSTCLSPC.WORK PURGE
  SET MAXCC=0
/*
//
