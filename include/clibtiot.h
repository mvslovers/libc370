#ifndef CLIBTIOT_H
#define CLIBTIOT_H

#include "ibm/mvs/ieftiot1.h"

/* __tiot() - retieve TIOT address */
TIOT *__tiot(void)										asm("@@TIOT");

/* __jobname() - retrieve address of 8 character job name from TIOT */
const char *__jobname(void)								asm("@@JOBNAM");

#endif
