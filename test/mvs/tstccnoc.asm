* TSTCCNOC - libc370 #507: C called from an assembler main program.
*
* An assembler main - the way a COBOL main is - calls C functions
* (test/mvs/tstccnoc.c) through CCALL stubs, with no C program on the
* task: the first call builds the C environment, the later ones use
* it, exit() in a C function releases it and returns here, the next
* call builds a new one, and CCTERM releases the last.  Each check
* failing sets its own return code; RC 0 = all of them passed.
*
*   RC 11/12  first call: count 1 / PPA set
*   RC 21/22  second call: count 2 / the same PPA
*   RC 31     atexit() registered (CATEX returns 0)
*   RC 41     exit(5) in CEXIT comes back as R15 = 5
*   RC 51/52  after exit(): count 3 (statics live in the load module)
*             / a PPA again
*   RC 61     CCTERM: RC 0
*   RC 71     CCTERM again: RC 4, no environment left
*
* SYSPRINT must hold "TSTCCNOC call 1", "call 2", "atexit ran" (from
* exit(5)) and "call 3" (flushed by CCTERM), in that order.
*
* TSTCCNOT, a second entry here, ends without CCTERM: its "call 1"
* line is still in the C buffer when the
* step ends, so SYSPRINT stays empty - the control for why CCTERM is
* needed.
*
* dev1 JOB02062, 2026-10-12 (RECEIVE JOB02061): TSTCCNOC CC 0000,
* SYSPRINT call 1, call 2, atexit ran, call 3; TSTCCNOT CC 0000 with
* SYSPRINT empty.
*
* Build:   as370 -I maclib test/mvs/tstccnoc.asm -o tstccnoc.o
*          cc370 -c -Os -Wall -Werror -Iinclude test/mvs/tstccnoc.c \
*                -o tstccnoc_c.o
*          cc370 tstccnoc.o tstccnoc_c.o -L build/sdk -o TSTCCNOC \
*                -Wl,--entry,TSTCCNOC -flinker-output=iebcopy
*          cc370 tstccnoc.o tstccnoc_c.o -L build/sdk -o TSTCCNOT \
*                -Wl,--entry,TSTCCNOT -flinker-output=iebcopy
*
TSTCCNOC CSECT
         SAVE  (14,12)
         LR    12,15
         USING TSTCCNOC,12
         LA    2,SAVEA
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
* 1: first call builds the environment
         LA    1,PL1
         L     15,=V(CCSTAT)
         BALR  14,15
         LA    9,11
         CLC   COUNT,=F'1'
         BNE   FAIL
         LA    9,12
         ICM   0,15,PPA1
         BZ    FAIL
*
* 2: second call uses it
         LA    1,PL2
         L     15,=V(CCSTAT)
         BALR  14,15
         LA    9,21
         CLC   COUNT,=F'2'
         BNE   FAIL
         LA    9,22
         CLC   PPA2,PPA1
         BNE   FAIL
*
* 3: register an atexit() function
         SR    1,1
         L     15,=V(CCATEX)
         BALR  14,15
         LA    9,31
         LTR   15,15
         BNZ   FAIL
*
* 4: exit(5) in C comes back here, the environment released
         SR    1,1
         L     15,=V(CCEXIT)
         BALR  14,15
         LA    9,41
         C     15,=F'5'
         BNE   FAIL
*
* 5: the next call builds a new environment
         LA    1,PL3
         L     15,=V(CCSTAT)
         BALR  14,15
         LA    9,51
         CLC   COUNT,=F'3'
         BNE   FAIL
         LA    9,52
         ICM   0,15,PPA3
         BZ    FAIL
*
* 6: CCTERM releases it; 7: a second CCTERM finds nothing
         SR    1,1
         L     15,=V(CCTERM)
         BALR  14,15
         LA    9,61
         LTR   15,15
         BNZ   FAIL
         SR    1,1
         L     15,=V(CCTERM)
         BALR  14,15
         LA    9,71
         C     15,=F'4'
         BNE   FAIL
         SR    15,15
         B     DONE
FAIL     DS    0H
         LR    15,9
DONE     DS    0H
         L     13,4(,13)
         RETURN (14,12),RC=(15)
*
PL1      DC    A(COUNT),A(X'80000000'+PPA1)
PL2      DC    A(COUNT),A(X'80000000'+PPA2)
PL3      DC    A(COUNT),A(X'80000000'+PPA3)
COUNT    DC    F'0'
PPA1     DC    F'0'
PPA2     DC    F'0'
PPA3     DC    F'0'
SAVEA    DS    18F
         LTORG
*
* TSTCCNOT - the control: one call, no CCTERM
TSTCCNOT CSECT
         SAVE  (14,12)
         LR    12,15
         USING TSTCCNOT,12
         LA    2,SAVEB
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         LA    1,PLN
         L     15,=V(CCSTAT)
         BALR  14,15
         SR    15,15
         L     13,4(,13)
         RETURN (14,12),RC=(15)
PLN      DC    A(CNTN),A(X'80000000'+PPAN)
CNTN     DC    F'0'
PPAN     DC    F'0'
SAVEB    DS    18F
         LTORG
*
CCSTAT   CCALL CSTAT
CCATEX   CCALL CATEX
CCEXIT   CCALL CEXIT
         END
