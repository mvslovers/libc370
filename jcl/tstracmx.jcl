//TSTRACMX JOB (SYS),'LIBC370 63',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #63 - what the RACHECK flag1 bit actually does.
//*
//* racf_auth() sets 0x10 under the name RACHECK_FLAG1_LOG_NONE.  0x10 is
//* DSTYPE=V; LOG=NONE is 0x02.  Before the library may set the correct bit,
//* one question has to be answered on the target: does suppressing the audit
//* also SOFTEN A DENIAL?  Cells (3), (6), (7) and (8) are that question.
//*
//* COND CODE 8 = a denial stopped denying, #63 does not proceed.
//*           4 = a permitted resource stopped answering 0 - not a security
//*               defect, but the consumer contract moves a second time.
//*           0 = every cell that could run behaved.
//*
//* THE AUDIT COUNTS ARE NOT IN THE COND CODE.  Each measurement is bracketed
//* by two WTOs; count the RAKF lines between them in JESMSGLG.
//*
//* APF: the probe issues MODESET KEY=ZERO,MODE=SUP, so TSTRACMX must be
//* linked AC=1 and live in an APF-authorized library, or the job ends S047
//* with an EMPTY SYSPRINT.  On the reference system SYS2.LINKLIB is in
//* SYS1.PARMLIB(IEAAPF00) and in the LNKLST, hence no STEPLIB DD here.  An
//* authorized program's WTO has no '+' prefix - that is how to tell.
//*
//* Build - the AC goes to ld370 TWICE (mvslovers/cc370#37):
//*   cc370 -O1 -Iinclude -c test/mvs/tstracmx.c -o tstracmx.o
//*   ld370 --entry @@CRT0 --ac 1 -o TSTRACMX build/sdk/crt0.o tstracmx.o \
//*         -Lbuild/sdk -lc
//*   ld370 --pack TSTRACMX=TSTRACMX --ac 1 -o probe -xmit \
//*         --dsn IBMUSER.LIBC370.PROBE.LINKLIB
//* Install: RECEIVE into a scratch library, IEBCOPY the member into the APF
//*          library, and delete it again afterwards.  Never RECEIVE over the
//*          APF library itself - RECEIVE does not merge and wants the target
//*          deleted first.
//*
//* FIXTURE - system setup the probe cannot do for itself.  Without the DD
//* below only the two "no profile" cells run and everything else reports
//* SKIPPED, which is a legitimate partial run.  Values must be UPPERCASE.
//*
//* USER must NOT be an administrator.  IBMUSER is permitted to everything,
//* so the gate cells could never deny it and would pass untested.
//*
//* PASS appears in JESJCL when the DD is instream.  Point FIXTURE at a
//* protected data set if that matters on your system.
//*
//RUN      EXEC PGM=TSTRACMX,REGION=4M
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*FIXTURE  DD  *
//** FACILITY profile the fixture user IS permitted to READ
//*FACOK  LIBC370.TSTRACMX.ALLOW
//** FACILITY profile the fixture user is NOT permitted to
//*FACNO  LIBC370.TSTRACMX.DENY
//** DATASET profile: READ permitted, UPDATE not (cell 7 needs both halves)
//*DSNOK  LIBC370.RACTEST.ALLOW
//** DATASET profile the fixture user is NOT permitted to
//*DSNNO  LIBC370.RACTEST.DENY
//** a userid that is NOT an administrator, and its password
//*USER   RACTEST
//*PASS   ????????
//*/*
