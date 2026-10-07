         COPY  PDPTOP
         CSECT
* Program data area
         DS    0F
@V1      EQU   *
         DC    F'10'
* Program text area
@@LC0    EQU   *
         DC    C'hello'
         DC    X'0'
* X-var msg
         ENTRY MSG
* Program data area
         DS    0F
MSG      EQU   *
         DC    A(@@LC0)
* Program text area
         DS    0F
* X-func next prologue
NEXT     PDPPRLG CINDEX=0,FRAME=88,BASER=12,ENTRY=YES
         B     @@FEN0
         LTORG
@@FEN0   EQU   *
         DROP  12
         BALR  12,0
         USING *,12
@@PG0    EQU   *
         LR    11,1
         L     10,=A(@@PGT0)
* Function next code
         SLR   15,15
         L     2,=A(@V2)
         L     3,0(2)
         A     3,=F'1'
         ST    3,0(2)
         L     2,=A(@V1)
         C     3,0(2)
         BNL   @@L2
         LA    15,1(0,0)
@@L2     EQU   *
         L     12,0(,10)
* Function next epilogue
         PDPEPIL
* Function next literal pool
         DS    0F
         LTORG
* Function next page table
         DS    0F
@@PGT0   EQU   *
         DC    A(@@PG0)
         DS    0F
@V2      EQU   *
         DS    XL4
         END
