//TST75RST JOB (SYS),'LIBC370 154',CLASS=A,MSGCLASS=H,
//             MSGLEVEL=(1,1)
//*
//* libc370 #154 - the X'75' copy must resume in the HOST buffer where a
//* nullifying page fault stopped it, not at its start.  This job places a
//* page boundary at a chosen multiple of 256 inside a 1024-byte receive
//* buffer, releases the page beyond it with PGRLSE, and receives over a
//* loopback pair it owns both ends of.  See test/mvs/tst75rst.c.
//*
//*   unfixed emulator: the bytes from the boundary on are the host buffer
//*                     replayed from its start - RC=8
//*   fixed emulator:   the pattern is intact in all four cases - RC=0
//*
//* The first case puts the boundary at offset 0, so the very first segment
//* faults having copied nothing.  That is correct on a fixed AND on an
//* unfixed emulator and must pass either way; if it fails, something other
//* than this defect is wrong and the rest of the run means nothing.
//*
//* RC=0 ALONE PROVES NOTHING.  A clean run has two causes - the emulator
//* resumes correctly, or the receive never faulted - and the guest cannot
//* tell them apart.  Run this under mvslovers/hyperion diag/x75-restart-trace
//* and read the Hercules log next to this job's RC:
//*
//*   X75 restart 7 (3 after a completed segment): dir=1 left=512 done=512 ...
//*
//* The second number is the one that matters.  If every restart reports
//* done=0 the probe never got past segment 0 and measured nothing.
//*
//S1       EXEC PGM=TST75RST,REGION=4096K
//STEPLIB  DD  DISP=SHR,DSN=IBMUSER.LIBC370.PROBE.LINKLIB
//SYSPRINT DD  SYSOUT=*
//SYSTERM  DD  SYSOUT=*
//SYSUDUMP DD  SYSOUT=*
