* TSTCCALL - the non-C side of test/mvs/tstccall.c (#502).
*
* OSADD, OSNEST and OSSPIE are OS-linkage routines the way COBOL code
* is: called from C with a VL parameter list, each with a STATIC save
* area of 18 words in the load module - below the C stack, which is
* what @@CCALL has to pass over - and each calls a C function through
* a CCALL stub with R1 as it got it.  OSSPIE establishes a SPIE of its
* own first, as COBOL does, and checks after the call that it is back.
* GETPICA is a C function: the current SPIE's PICA, or 0.
*
* OSDIRECT is the red control: it calls the C function CADD directly,
* without a stub, as COBOL code does today.
*
OSADD    CSECT
         SAVE  (14,12)
         LR    12,15
         USING OSADD,12
         LA    2,SAVEADD
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         L     15,=V(CCADD)
         BALR  14,15
         L     13,4(,13)
         RETURN (14,12),RC=(15)
SAVEADD  DS    18F
         LTORG
*
OSNEST   CSECT
         SAVE  (14,12)
         LR    12,15
         USING OSNEST,12
         LA    2,SAVENST
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         L     15,=V(CCNEST)
         BALR  14,15
         L     13,4(,13)
         RETURN (14,12),RC=(15)
SAVENST  DS    18F
         LTORG
*
* OSSPIE: SPIE for every program interruption, whose exit must never
* be entered (U0999); call CSPIE; then cancel the SPIE, and the PICA
* it reports must be ours.  RC: the C function's, or 98 when the PICA
* that came back is not ours.
OSSPIE   CSECT
         SAVE  (14,12)
         LR    12,15
         USING OSSPIE,12
         LA    2,SAVESPI
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         LR    3,1                the list for CSPIE
         SPIE  MF=(E,MYPICA)      ours; R1 = the previous PICA
         LA    4,MYPICA           the PICA that must come back
         LR    1,3
         L     15,=V(CCSPIE)
         BALR  14,15
         LR    5,15               CSPIE's return code
         SPIE  ,                  cancel; R1 = the PICA in force
         N     1,=X'00FFFFFF'
         N     4,=X'00FFFFFF'
         CLR   1,4
         BE    SPIEOK
         LA    5,98               not ours: @@CCALL did not restore it
SPIEOK   DS    0H
         LR    15,5
         L     13,4(,13)
         RETURN (14,12),RC=(15)
SPIEXIT  DS    0H
         ABEND 999,DUMP           a program check reached OSSPIE's SPIE
MYPICA   SPIE  SPIEXIT,((1,15)),MF=L
SAVESPI  DS    18F
         LTORG
*
* OSDIRECT: the red control, CADD called without @@CCALL
OSDIRECT CSECT
         SAVE  (14,12)
         LR    12,15
         USING OSDIRECT,12
         LA    2,SAVEDIR
         ST    13,4(,2)
         ST    2,8(,13)
         LR    13,2
         L     15,=V(CADD)
         BALR  14,15
         L     13,4(,13)
         RETURN (14,12),RC=(15)
SAVEDIR  DS    18F
         LTORG
*
* GETPICA - void *getpica(void), C convention
GETPICA  CSECT
         STM   14,12,12(13)
         LR    12,15
         USING GETPICA,12
         SPIE  ,                  cancel: R1 = the PICA in force
         LR    2,1
         SPIE  MF=(E,(1))         establish it again
         LR    15,2
         L     14,12(,13)
         LM    2,12,28(13)
         BR    14
         LTORG
*
CCADD    CCALL CADD
CCNEST   CCALL CNEST
CCSPIE   CCALL CSPIE
         END
