//TST75CAP JOB (SYS),'LIBC370 154 CAP',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #154 - red/green for the recv() CHUNK CAP.
//*
//* tst75rst measures the emulator; this one measures the guest-side fix.
//* A 4096-byte receive buffer is placed so a page boundary falls at offset
//* 2560 - the first bad byte in mvslovers/ftpd#122 - and the page beyond it
//* is released with PGRLSE immediately before the receive.
//*
//*   TST75CAP  linked against the cc370 sysroot libc  (cap 4096)
//*   TST75CPN  linked against the fix branch libc.a   (cap  256)
//*
//* Both run four cases: a boundary-0 control, the pre-fix loop at cap 4096,
//* the post-fix loop at cap 256, and the real recv() out of whichever libc
//* the module was linked with.  The first two are identical in both modules
//* and are the reproduction; the last one separates the two builds.
//*
//*   unfixed emulator: cap 4096 corrupts at 2560 with the tail a replay of
//*                     the buffer head, cap 256 is clean
//*   fixed emulator:   every case clean - and the run then says NOTHING
//*                     about the cap.  The program prints that verdict.
//*
//* STEPLIB is the scratch PDS the RECEIVE in recv75cap.jcl restores into,
//* not the shared probe LINKLIB: RECEIVE already produces a usable load
//* library, and IBMUSER.LIBC370.PROBE.LINKLIB is out of space (an IEBCOPY
//* into it abends SE37 - it needs a compress or a bigger allocation).
//*
//S1       EXEC PGM=TST75CAP,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RSTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
//*
//S2       EXEC PGM=TST75CPN,REGION=4096K,COND=EVEN
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RSTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
