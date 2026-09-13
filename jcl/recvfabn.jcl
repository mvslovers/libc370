//TSTRCVA  JOB (SYS),'RECV TSTFABND',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #168 __fabandon() probe into a scratch PDS.  RECEIVE cannot
//* merge into an existing PDS, and it leaves a usable load library behind,
//* so tstfabnd.jcl STEPLIBs straight at the scratch data set rather than
//* IEBCOPYing into the shared probe LINKLIB (which is out of extents).
//*
//* Upload test/mvs/tstfabnd.c built against build/sdk - NOT the installed
//* sysroot, which has no __fabandon() - as
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstfabnd.c \
//*           -o TSTFABND -flinker-output=iebcopy
//*     ld370 --pack TSTFABND=TSTFABND.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.ABNSCR
//* to IBMUSER.MBT.XMIT.IN first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.ABNSCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.MBT.XMIT.IN')
/*
//
