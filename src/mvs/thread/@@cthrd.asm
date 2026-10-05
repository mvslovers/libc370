@@CTHRD  TITLE 'C T H R E A D  ***  C subtask driver and thread exit'
***********************************************************************
*  CTHREAD and @@CTEXIT, the subtask driver cthread_create_ex()       *
*  ATTACHes by name and the thread exit cthread_exit() calls.         *
*                                                                     *
*  They lived inside @@CRT0 (and its copy @@CRT1).  As a member of    *
*  their own they are linked only into a program that uses threads:   *
*  @@ctcrtx.c references CTHREAD hard, and @@CRT0 IDENTIFYs CTHREAD   *
*  through a weak reference only when it is linked (#159).            *
*                                                                     *
*  Original code and concepts provided by: PAUL EDWARDS.              *
*  Extensive modifications provided by: Mike Rayborn                  *
*                                                                     *
*  RELEASED TO THE PUBLIC DOMAIN                                      *
***********************************************************************
         COPY  PDPTOP
         PRINT OFF
         USING PSA,R0
         PRINT ON
         COPY  CLIBCRT
         CSECT
         ENTRY CTHREAD
CTHREAD  DS    0H
         SAVE  (14,12),,'CTHREAD &SYSDATE &SYSTIME'
         LA    R12,0(,R15)
         USING CTHREAD,R12
*
         LA    R11,0(,R1)
         USING CTHDTASK,R11
*
* Chain stack with callers save area
         LA    R1,CTHDSTK        => stack for function
         ST    R13,4(,R1)        ... chain stack areas
         ST    R1,8(,R13)        ... chain stack areas
         LR    R13,R1            new stack
         USING STK,R13
*
* Save thread handle in stack
         ST    R11,STKCTHD       A(CTHDTASK)
*
* Set next available byte in stack
         LA    R0,STKNAB         next available byte in stack
         ST    R0,STKSVNAB       next available byte in stack
*
* Allocate CLIBCRT area in PPA
         L     R15,=V(@@CRTSET)
         BALR  R14,R15           Create CLIBCRT in PPA
*
* Save R13 in CRTSAVE
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
* A NULL CLIBCRT would make the ST below a store into low storage;
* fail loudly instead (#81)
         LTR   R15,R15           Did we get a CLIBCRT?
         BNZ   TCRTOK            Yes, continue
         WTO   'CTHREAD - No storage for CLIBCRT'
         ABEND 801,DUMP          Cannot run C code without a CRT
TCRTOK   DS    0H
         ST    R13,CRTSAVE-CLIBCRT(,R15) Save our save area address
*
* Call thread function
         L     R15,CTHDFUNC      get function address from plist
         LA    R1,CTHDARG1       => parameters for function
         BALR  R14,R15           call function
         ST    R15,CTHDRC        save return code from function
*
* Call thread exit
         LA    R1,CTHDRC         => return code
         L     R15,=A(@@CTEXIT)
         BR    R15               exit thread environment
         LTORG
         TITLE '@@CTEXIT - exit C thread environment'
         ENTRY @@CTEXIT
@@CTEXIT DS    0H
         LA    R12,0(,R15)
         USING @@CTEXIT,R12
         L     R9,0(R1)          Get @@EXITB(rc) value
*
* Get save area address from CLIBCRT area
         L     R15,=V(@@CRTGET)
         BALR  R14,R15           Get our CLIBCRT area
         L     R13,CRTSAVE-CLIBCRT(,R15) Restore thread stack
         USING STK,R13
*
* Get thread task control block
         L     R11,STKCTHD       => thread task control block
         USING CTHDTASK,R11
*
* Get return code passed to us
*        L     R9,0(R1)          Get @@EXITB(rc) value
         ST    R9,CTHDRC         save as return code
*
* Do thread cleanup
         WXTRN @@CTCLUP
         ICM   R15,15,=V(@@CTCLUP) Get thread level cleanup
         BZ    THRDDONE
         BALR  R14,R15           Call __ctclup() routine
*
* Deallocate CLIBCRT area
THRDDONE DS    0H
         L     R15,=V(@@CRTRES)
         BALR  R14,R15           release CLIBCRT area from PPA
*
* Get callers save area
         L     R13,STKSV+4       switch back to callers stack
         LR    R15,R9            restore return code
RETURN   RETURN (14,12),RC=(15)
* Note:
* The task level area CTHDTASK persists until the main thread or
* thread manager code calls @@CTDEL() to delete the thread.
*
         LTORG ,
         TITLE 'Dummy Sections'
* Stack for C thread
STK      DSECT
STKSV    DS    18F               00 (0)  callers registers go here
STKSVLWS DS    A                 48 (72) PL/I Language Work Space N/A
STKSVNAB DS    A                 4C (76) next available byte -------+
STKCTHD  DS    A                 50 (80) A(CTHDTASK)                |
STKAVAIL DS    F                 54 (84) unused/available           |
STKNAB   DS    0D                58 stack next available byte <-----+
*
* C thread parameter list
CTHDTASK DSECT
CTHDEYE  DS    CL8               00 eye catcher for dumps
CTHDTCB  DS    F                 08 subtask TCB address
CTHDOTCB DS    F                 0C subtask owner TCB address
CTHDECB  DS    F                 10 posted by MVS when task ends
CTHDRC   DS    F                 14 return code from function
CTHDSSIZ DS    F                 18 stack size in bytes
CTHDFUNC DS    A                 1C subtask function address
CTHDARG1 DS    A                 20 arg1 for subtask function
CTHDARG2 DS    A                 24 arg2 for subtask function
CTHDSTK  DS    F                 28 start of stack for driver
*
         IHAPSA
         END
