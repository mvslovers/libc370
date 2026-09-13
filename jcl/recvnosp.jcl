//TSTRCVNS JOB (SYS),'RECV TSTNOSPC',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #149 after-ENOSPC probe into a scratch PDS.  RECEIVE cannot
//* merge into an existing PDS, and it leaves a usable load library behind,
//* so tstnospc.jcl STEPLIBs straight at the scratch data set rather than
//* IEBCOPYing into the shared probe LINKLIB (which is out of extents).
//*
//* Upload test/mvs/tstnospc.c built against build/sdk - NOT the installed
//* sysroot, which has no x37 exit (#176) and would abend the probe for a
//* reason that has nothing to do with #149 - as
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstnospc.c \
//*           -o TSTNOSPC -flinker-output=iebcopy
//*     ld370 --pack TSTNOSPC=TSTNOSPC.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.NSPSCR
//* to IBMUSER.MBT.XMIT.IN first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.NSPSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.MBT.XMIT.IN')
/*
//
