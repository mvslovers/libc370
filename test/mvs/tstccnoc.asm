* TSTCCNOC - libc370 #502: @@CCALL with no C program on the task.
*
* An assembler main - the way a COBOL main is - calls the C-side name
* CNOC through a CCALL stub.  No C startup has run, so there is no PPA
* on the save area chain: @@CCALL must end the step with U3101 and its
* message, before it calls anything.  CNOC is a plain assembler routine
* here; it must never be reached (RC 77 if it is).
*
* Build:   as370 -I maclib test/mvs/tstccnoc.asm -o tstccnoc.o
*          cc370 tstccnoc.o -L build/sdk -o TSTCCNOC \
*                -Wl,--entry,TSTCCNOC -flinker-output=iebcopy
* Expected: completion code U3101, "@@CCALL - no C program ...".
*
TSTCCNOC CSECT
         SAVE  (14,12)
         LR    12,15
         USING TSTCCNOC,12
         LA    2,SAVEA
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         LA    1,PLIST
         L     15,=V(CCNOC)
         BALR  14,15
         L     13,4(,13)
         RETURN (14,12),RC=(15)
PLIST    DC    A(X'80000000'+NUM)
NUM      DC    F'1'
SAVEA    DS    18F
         LTORG
*
CNOC     CSECT
         LA    15,77
         BR    14
*
CCNOC    CCALL CNOC
         END
