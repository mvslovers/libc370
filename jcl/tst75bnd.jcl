//TST75BND JOB (SYS),'LIBC370 164',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* MEASUREMENT PROBE for issue #164 - the unchecked aux2 bound in
//* the X'75' SELECT path.  See test/mvs/tst75bnd.c.
//*
//* WARNING: OWN DISPOSABLE CONTAINER ONLY.  This drives the
//* emulator's SELECT path with an out-of-range aux2 and CAN CRASH
//* THE EMULATOR; an out-of-bounds write can corrupt other address
//* spaces first.  Never run on a shared or real system; never add
//* as a [[test]] entry.
//*
//* Read the job log (JESMSGLG) for the TST75BND wtof lines:
//*   TST75BND aux2=V rc=R alive=Y/N
//*     -> value V returned R; an ordinary socket still worked
//*        afterwards (Y/N)
//*   TST75BND highest aux2 tried = V   -> the ceiling reached
//* An alive=N line, or a run that ends with no "highest" line and
//* the container no longer answering, is the emulator going down.
//*
//S1       EXEC PGM=TST75BND,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.BNSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
