//TSTRACAU JOB (SYS),'LIBC370 58',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #58 - racf_auth() passes the ACEE in the RACHECK parameter list
//* instead of writing it into ASXBSENV and holding an ENQ on the ASXB.
//*
//* Case (3) is the regression guard, and it is about the ENQ rather than the
//* ACEE: lock() returns 8 when you already hold the lock, the old code
//* ignored that and DEQd unconditionally, so a caller holding the ASXB lock
//* across racf_auth() lost it.  Before the fix (3c) and (4a) fail.
//*
//* The race the issue is really about needs a second TCB and a window a few
//* instructions wide - not reproducible in batch.  See test/mvs/tstracau.c
//* for what this test can and cannot show.
//*
//* APF: racf_auth() issues MODESET KEY=ZERO,MODE=SUP, so TSTRACAU must be
//* linked AC=1 and the library must be APF-authorized, or the job ends S047
//* in case (1) with an EMPTY SYSPRINT.  On the reference system SYS2.LINKLIB
//* is in SYS1.PARMLIB(IEAAPF00) and in the LNKLST, hence no STEPLIB DD here.
//*
//* Build - the AC goes to ld370, TWICE.  cc370 silently drops -Wl,--ac,1,
//* and --pack loses the flag again unless it is repeated:
//*   cc370 -O1 -Iinclude -c test/mvs/tstracau.c -o tstracau.o
//*   ld370 --entry @@CRT0 --ac 1 -o TSTRACAU build/sdk/crt0.o tstracau.o \
//*         -Lbuild/sdk -lc
//*   ld370 --pack TSTRACAU=TSTRACAU --ac 1 -o probe -xmit \
//*         --dsn IBMUSER.LIBC370.PROBE.LINKLIB
//* Install: RECEIVE into a scratch library, IEBCOPY the member into the APF
//*          library, and delete it again afterwards.  Never RECEIVE over the
//*          APF library itself - RECEIVE does not merge and wants the target
//*          deleted first.
//*
//* RC 0 = every check passed, 8 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTRACAU,REGION=4M
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
