//TSTSTEM  JOB (SYS),'LIBC370 395',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #395 - stdio and stdlib edge cases.  See test/mvs/tststem.c.
//* GREEN is this tree; RED, where there is one, the installed library.
//*
//GREEN    EXEC PGM=TSTSTEG,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STSCR
//RECS1    DD  *
RECORD ONE
/*
//RECS2    DD  *
SECOND FILE
/*
//RECS3    DD  *
AB
/*
//RED      EXEC PGM=TSTSTER,REGION=4M,COND=EVEN,TIME=(0,10)
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.STSCR
//RECS1    DD  *
RECORD ONE
/*
//RECS2    DD  *
SECOND FILE
/*
//RECS3    DD  *
AB
/*
//
