/* @@USEREX.C */
#include "stdlib.h"
#include "signal.h"
#include "string.h"
#include "ctype.h"
#include "stddef.h"

/* the name <stdlib.h> declares; both were @@USEREX to the linker (#420) */
void (*__userExit[__NATEXIT])(void) = {0};
