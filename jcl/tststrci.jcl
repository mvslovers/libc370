//TSTSTRCI JOB (SYS),'LIBC370 183',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #183 - strcasecmp/strncasecmp, and the stricmp/strncmpi
//* aliases they join.  All four fold through the __tolow table, which
//* is what makes them EBCDIC-correct; an ASCII-style fold would not be.
//*
//* MVS only, and that is the point: on a host the fold runs through the
//* host's ASCII table and cannot observe the property under test.  The
//* EBCDIC letters sit in three runs with gaps between them, and case (2)
//* spans all three.  Case (1) is the control - if it fails the run is
//* not EBCDIC and nothing below means what it says.
//*
//* See test/mvs/tststrci.c.
//*
//* Build:   cc370 -O1 -Iinclude test/mvs/tststrci.c -o TSTSTRCI \
//*                -flinker-output=xmit
//* Install: RECEIVE the XMIT into the STEPLIB below.
//*
//* RC 0 = every check passed, 1 = a check failed (it is the COND CODE).
//*
//RUN      EXEC PGM=TSTSTRCI,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.TEST.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
