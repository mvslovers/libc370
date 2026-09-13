//TSTRCVX  JOB (SYS),'RECV TSTX37',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* RECEIVE the #176 x37-exit probe into a scratch PDS.  RECEIVE cannot merge
//* into an existing PDS, and it leaves a usable load library behind, so
//* tstx37.jcl STEPLIBs straight at the scratch data set rather than
//* IEBCOPYing into the shared probe LINKLIB (which is out of extents).
//*
//* Upload test/mvs/tstx37.c built against build/sdk - NOT the installed
//* sysroot, which has no x37 exit and would abend the probe for a reason
//* that has nothing to do with the target - as
//*     make build
//*     cc370 -O1 -Iinclude -L build/sdk test/mvs/tstx37.c \
//*           -o TSTX37 -flinker-output=iebcopy
//*     ld370 --pack TSTX37=TSTX37.iebcopy -o probe -xmit \
//*           --dsn IBMUSER.LIBC370.X37SCR
//* to IBMUSER.MBT.XMIT.IN first.
//*
//DEL      EXEC PGM=IEFBR14
//D1       DD  DSN=IBMUSER.LIBC370.X37SCR,DISP=(MOD,DELETE),
//             UNIT=SYSDA,SPACE=(TRK,(1,1,1))
//*
//RECV     EXEC PGM=IKJEFT01,DYNAMNBR=20,REGION=6144K
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  RECEIVE INDSN('IBMUSER.MBT.XMIT.IN')
/*
//
