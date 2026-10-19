/* @@ARDEL.C
** Dynamic array
*/
#include "ext/array.h"

__asm__("\n&FUNC    SETC 'arraydel'");
void *
arraydel( void * varray, unsigned index )
{
    void  *vitem    = NULL;
    char  ***carray = varray;
    ARRAY *array;

    if (carray) {
        if (*carray) {
            array = (ARRAY *)(((char *)*carray) - sizeof(ARRAY));
            if (memcmp(array->eye, ARRAY_EYE, 4) == 0) {
                if ((index > 0) && (index <= array->count)) {
                    index--;
                    vitem = (*carray)[index];
                    /* now shift everything left.  Up to the last item,
                       not past it: [count] is the word after a full
                       array's allocation (#583) */
                    for( ; index + 1 < array->count; index++ ) {
                        (*carray)[index] = (*carray)[index+1];
                    }
                    array->count--;
                    (*carray)[array->count] = NULL;
                }
            }
        }
    }

    return vitem;
}
