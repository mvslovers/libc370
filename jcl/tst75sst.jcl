//TST75SST JOB (SYS),'LIBC370 160',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #160 - a page fault inside a send() buffer must not
//* corrupt what the peer receives.  Each round releases one page
//* of the send buffer with PGRLSE and sends the whole buffer over
//* a loopback pair the probe owns both ends of.
//* See test/mvs/tst75sst.c.
//*
//* GREEN is linked with this tree, RED with the library it changes.
//* RC=0 alone proves nothing: read it next to the Hercules log of
//* a mvslovers/hyperion diag/x75-restart-trace build, whose
//* "after a completed segment" count must be above zero.
//*
//GREEN    EXEC PGM=TST75SSG,REGION=4M,COND=EVEN,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SSTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//RED      EXEC PGM=TST75SSR,REGION=4M,COND=EVEN,TIME=(0,30)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.SSTSCR
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//
