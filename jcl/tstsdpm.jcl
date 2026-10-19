//TSTSDPM  JOB (SYS),'LIBC370 413',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #413 - DUMP_SDUMP is DUMP_DEFAULT.  See test/mvs/tstsdpm.c.
//* GREEN is this tree; RED a build before the fix.  The verdict is
//* the log: RED writes "system_dump requested", GREEN does not.
//* Run: own MVS/CE container, 2026-10-20: both CC 0000 (recovered),
//*      the message in RED only.
//*
//GREEN    EXEC PGM=TSTSDPG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SDPSCR
//RED      EXEC PGM=TSTSDPR,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SDPSCR
//
