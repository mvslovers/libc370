//TSTRACUN JOB (SYS),'LIBC370 197',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #197 - racf_auth() from a caller that is NOT APF-authorized.
//* See test/mvs/tstracun.c for the cells.
//*
//* STEPLIB is the scratch PDS recvracu.jcl restores into.  It must NOT
//* be in IEAAPF00 and the module is linked without AC=1: the probe
//* checks __isauth() first and ends CC 12 if it runs authorized.  Its
//* WTOs carry the '+' prefix, which is how to tell at a glance.
//*
//* Run:     mvsdev JOB00659, CC 0000, 2026-09-29.  Red on the old
//*          library: JOB00655, CC 0008 - S047 in (2) and (5), and (4)
//*          came back in problem state.
//*
//* RC 0  = every gated cell passed
//*    4  = a reference answer of the raw SVC 130 moved (stand changed)
//*    8  = racf_auth() abended, disagreed with SVC 130, or left the
//*         caller in the wrong state
//*   12  = ran authorized, nothing measured
//*
//RUN      EXEC PGM=TSTRACUN,REGION=4M
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.RACUNLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
