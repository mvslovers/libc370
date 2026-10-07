#include <stdio.h>
#include <mvs/lock.h>
#include <mvs/enq.h>

/* Serialize the tasks of this program on one account.            */
int update_account(int acct)
{
    char name[32];
    int  rc;

    snprintf(name, sizeof name, "MYAPP.ACCT.%d", acct);
    rc = lock_res(name, LOCK_EXC);
    if (rc != 0 && rc != 8)
        return -1;

    /* ... read, change and write the account ...                 */

    if (rc == 0)                 /* 8: the caller already held it */
        unlock_res(name, LOCK_EXC);
    return 0;
}

/* Serialize with every address space: take the master file only  */
/* when nobody else has it.                                       */
int try_master(void)
{
    int rc;

    rc = ENQ("MYAPP", "PAYROLL.MASTER", ENQ_SYSTEM | ENQ_EXC | ENQ_USE);
    if (rc == 4)
        return 1;                /* busy: try again later         */
    if (rc != 0)
        return -1;

    /* ... work on the master file ...                            */

    DEQ("MYAPP", "PAYROLL.MASTER", ENQ_SYSTEM);
    return 0;
}
