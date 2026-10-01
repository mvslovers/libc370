         COPY  MVSMACS
         COPY  PDPTOP
         CSECT ,
         PRINT GEN
*        YREGS
         SPACE 1
*-----------------------ASSEMBLY OPTIONS------------------------------*
SUBPOOL  EQU   0                                                      *
*---------------------------------------------------------------------*
         SPACE 1
***********************************************************************
*                                                                     *
*  ADISC  - Discard whatever output is pending in the DCB work area,  *
*           so that the next @@ACLOSE writes NOTHING (#168).          *
*                                                                     *
*  @@ACLOSE opens with FIXWRITE, and @@ATROUT writes the block when   *
*  IOFLDATA says one is pending.  After an x37 that flag is already   *
*  off - @@ATROUT resets it BEFORE the WRITE - but that is a property *
*  of one failure shape, not a contract.  A caller that abandons a    *
*  FILE while a good partial block is pending means "write nothing    *
*  more", and only clearing the state here delivers that.             *
*                                                                     *
*  IOFLDATA  output buffer has data          -> nothing to write      *
*  IOFLSDW   spanned record incomplete       -> no continuation       *
*  BUFFCURR  current record in the block     -> block is empty        *
*  KEPTREC   record kept for UPDAT rewrite   -> nothing to rewrite    *
*                                                                     *
*  Touches no MVS service, so it needs no save area of its own.       *
*  void __adisc(void *handle)                                         *
*                                                                     *
***********************************************************************
@@ADISC  FUNHEAD IO=YES,AM=YES,US=NO   DISCARD PENDING OUTPUT
         NI    IOPFLAGS,255-IOFLDATA-IOFLSDW  NOTHING IS PENDING
         XC    BUFFCURR,BUFFCURR   THE BLOCK IS EMPTY
         XC    KEPTREC(8),KEPTREC  NO RECORD KEPT FOR REWRITE
         FUNEXIT RC=0
*
         LTORG ,
         SPACE 2
         COPY  CLIBSUPA
*
         SPACE 2
         PRINT NOGEN
         IHAPSA ,            MAP LOW STORAGE
         CVT DSECT=YES
         IKJTCB ,            MAP TASK CONTROL BLOCK
         IKJECT ,            MAP ENV. CONTROL BLOCK
         IKJPTPB ,           PUTLINE PARAMETER BLOCK
         IKJCPPL ,
         IKJPSCB ,
         IEZJSCB ,
         IEZIOB ,
         IEFZB4D0 ,          MAP SVC 99 PARAMETER LIST
         IEFZB4D2 ,          MAP SVC 99 PARAMETERS
         IEFUCBOB ,
MYTIOT   DSECT ,
         IEFTIOT1 ,
         IHAPDS PDSBLDL=YES
         SPACE 1
         IFGACB ,                                               GP14233
         SPACE 1
         IFGRPL ,                                               GP14233
         IEFJESCT ,
         IKJUPT ,
         END
