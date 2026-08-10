         COPY  MVSMACS
         COPY  PDPTOP
         CSECT ,
         PRINT GEN
         SPACE 1
***********************************************************************
*                                                                     *
*  FREEM - FREE MEMORY                                                *
*                                                                     *
*  The header word at block-8 is SP||LV since #89: the subpool the    *
*  block came from in the high byte, the rounded length below - which *
*  is exactly the R0 the R-form FREEMAIN wants, so the storage goes   *
*  back to the subpool it was obtained from without @@FREEM having    *
*  to know the ambient value.  Blocks written by a pre-#89 @@GETM     *
*  carry 0 in the high byte and decode as subpool 0, unchanged.       *
*                                                                     *
***********************************************************************
@@FREEM  FUNHEAD ,
*
         L     R1,0(,R1)
         S     R1,=F'8'
         L     R0,0(,R1)          SP IN BYTE 0, LENGTH IN BYTES 1-3
*
         FREEMAIN R,LV=(0),A=(1)
*
         FUNEXIT RC=0
         LTORG ,
         END
