//TSTRACFL JOB (SYS),'LIBC370 64',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #64 - racf_login() and racf_logout() stop taking the AS-wide
//* ASXB ENQ, and racf_logout() stops parking a foreign ACEE in ASXBSENV.
//*
//* The whole test reports through WTO, so its results are in THIS job log,
//* not in SYSPRINT.  Cases (2), (3) and (6) discriminate; a pre-fix library
//* fails all three and ends CC 0008.  (6) is the defect itself - a worker
//* TCB reading back an ACEE nobody but racf_logout() could have put there -
//* and it is statistical: 200 login/logout rounds hit the window ~14 times
//* in ~1200 worker loops.  Lowering ROUNDS to 60 made it miss entirely.
//*
//* APF: racf_login()/racf_logout() issue MODESET KEY=ZERO,MODE=SUP, so
//* TSTRACFL must be linked AC=1 and the library must be APF-authorized, or
//* the job ends S047 with nothing in SYSPRINT.  On the reference system
//* SYS2.LINKLIB is in SYS1.PARMLIB(IEAAPF00) and in the LNKLST, hence no
//* STEPLIB DD here.  See test/mvs/tstracfl.c for the build - the AC has to
//* be given to ld370 twice (mvslovers/cc370#37).
//*
//* The probe creates and deletes ACEEs for IBMUSER via PASSCHK=NO, so no
//* password is needed, and it restores the address space's resting ACEE
//* before it ends.
//*
//* RC 0 = every check passed, 8 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTRACFL,REGION=4M
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
