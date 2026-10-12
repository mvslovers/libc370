/*
 * tstksds.c - libc370 #514 on MVS: a KSDS opened for dynamic access.
 *
 * ISSUE #514 (measured by the oftp370 side, dev1 JOB02091/02094):
 *   - vswrite() on a KSDS opened VSACCESS_DYNAM, VSMODE_UPD issued a
 *     keyed-sequential PUT (the RPL keeps OPTCD=SEQ), so a key below the
 *     current position failed with feedback 12 (out of sequence), and so
 *     did a duplicate key - the two could not be told apart.  It now
 *     puts direct: the lower key is inserted, a duplicate fails with 8.
 *   - vsread() with a key that is not there returned -2 with feedback 88,
 *     the GET's after the failed POINT; the reference says 16.  It now
 *     returns the POINT's feedback, 16, without a GET.
 *
 * DD KSDS is an empty KSDS, KEYS(8 0) RECORDSIZE(80 80), that
 * jcl/tstksds.jcl defines and deletes again.  The test loads it
 * sequentially first (a never-loaded cluster cannot be opened for
 * update), then works on it with dynamic access.  Built twice: TSTKSDS
 * against this tree's libc.a, TSTKSDSR against 2.10.0's - the red
 * control (the lower key, the duplicate's reason and the missing key's
 * reason fail).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Iinclude -L build/sdk \
 *                test/mvs/tstksds.c -o TSTKSDS -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Iinclude -L <2.10.0 sysroot>/lib \
 *                test/mvs/tstksds.c -o TSTKSDSR -flinker-output=iebcopy
 *          ld370 --pack TSTKSDS=TSTKSDS.iebcopy \
 *                --pack TSTKSDSR=TSTKSDSR.iebcopy \
 *                -o tstksds -xmit --dsn IBMUSER.LIBC370.KSSCR
 * Install: jcl/recvksds.jcl.   Run: jcl/tstksds.jcl.
 *
 * (mvs/vsam.h fails -Werror on #pragma pack, #415, so no -Werror here.)
 *
 * dev1 JOB02108, 2026-10-12 (RECEIVE JOB02107): GREEN CC 0000, 13/13;
 * RED (2.10.0) CC 0001, 7/13: lower key, key in between and duplicate
 * rc 8 reason 12, missing key reason 88.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <mvs/vsam.h>

#define RECLEN 80

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static char rec[RECLEN];

static void mkrec(const char *key, const char *text)
{
    memset(rec, ' ', RECLEN);
    memcpy(rec, key, 8);
    memcpy(rec + 8, text, strlen(text));
}

int main(void)
{
    static const char *load[] = { "T0000001", "T0000003", "T0000005" };
    VSFILE  *vs = NULL;
    char    buf[RECLEN + 1];
    char    msg[96];
    char    order[7][9];
    int     rc, i, n;

    printf("=== tstksds: a KSDS with dynamic access (#514) ===\n");

    /* load the empty cluster sequentially */
    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_OUT, &vs);
    CHECK(rc == 0 && vs, "vsopen() for the initial load");
    if (rc != 0 || !vs) goto done;
    for (i = 0, n = 0; i < 3; i++) {
        mkrec(load[i], "LOADED");
        if (vswrite(vs, rec, RECLEN, NULL, 0) == 0) n++;
    }
    CHECK(n == 3, "three records loaded");
    vsclose(vs);

    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_DYNAM, VSMODE_UPD, &vs);
    CHECK(rc == 0 && vs, "vsopen() for update, dynamic access");
    if (rc != 0 || !vs) goto done;

    /* a key above every record: a keyed-sequential PUT positions after
       it, which is what made every lower key "out of sequence" before
       #514 (the oftp370 measurement inserted in ascending order first) */
    mkrec("T0000007", "HIGHEST");
    rc = vswrite(vs, rec, RECLEN, "T0000007", 8);
    sprintf(msg, "vswrite() of the highest key: rc %d reason %d", rc, vs->rsn);
    CHECK(rc == 0, msg);
    vsclear(vs);

    /* a key below every record in the cluster */
    mkrec("E0000001", "LOWER");
    rc = vswrite(vs, rec, RECLEN, "E0000001", 8);
    sprintf(msg, "vswrite() of a lower key inserts it: rc %d reason %d", rc, vs->rsn);
    CHECK(rc == 0, msg);
    vsclear(vs);

    /* a key between two records */
    mkrec("T0000002", "BETWEEN");
    rc = vswrite(vs, rec, RECLEN, "T0000002", 8);
    sprintf(msg, "vswrite() of a key in between inserts it: rc %d reason %d", rc, vs->rsn);
    CHECK(rc == 0, msg);
    vsclear(vs);

    /* a key that is there already */
    mkrec("T0000003", "DUPLICATE");
    rc = vswrite(vs, rec, RECLEN, "T0000003", 8);
    sprintf(msg, "vswrite() of a duplicate: rc %d reason %d (8, 8)", rc, vs->rsn);
    CHECK(rc == 8 && vs->rsn == 8, msg);
    vsclear(vs);

    /* a key that is not there */
    rc = vsread(vs, buf, RECLEN, "X0000000", 8);
    sprintf(msg, "vsread() of a missing key: rc %d reason %d (-2, 16)", rc, vs->rsn);
    CHECK(rc == -2 && vs->rsn == 16, msg);
    vsclear(vs);

    /* the inserted record reads back by key, and the duplicate left the
       original alone */
    rc = vsread(vs, buf, RECLEN, "T0000002", 8);
    CHECK(rc == RECLEN && memcmp(buf + 8, "BETWEEN", 7) == 0,
          "the record inserted in between reads back by key");
    rc = vsread(vs, buf, RECLEN, "T0000003", 8);
    CHECK(rc == RECLEN && memcmp(buf + 8, "LOADED", 6) == 0,
          "the duplicate did not replace the record");

    /* and the update of the record just read still works */
    mkrec("T0000003", "UPDATED");
    CHECK(vsupdate(vs, rec, RECLEN) == 0, "vsupdate() of the record just read");
    vsclose(vs);

    /* everything in key order after a reopen */
    rc = vsopen("KSDS", VSTYPE_KSDS, VSACCESS_SEQ, VSMODE_IN, &vs);
    CHECK(rc == 0 && vs, "vsopen() for input");
    if (rc != 0 || !vs) goto done;
    n = 0;
    while (n < 7 && vsread(vs, buf, RECLEN, NULL, 0) == RECLEN) {
        memcpy(order[n], buf, 8);
        order[n][8] = 0;
        n++;
    }
    vsclose(vs);
    sprintf(msg, "six records in key order after the reopen: %d", n);
    CHECK(n == 6 && strcmp(order[0], "E0000001") == 0
          && strcmp(order[1], "T0000001") == 0
          && strcmp(order[2], "T0000002") == 0
          && strcmp(order[3], "T0000003") == 0
          && strcmp(order[4], "T0000005") == 0
          && strcmp(order[5], "T0000007") == 0, msg);

done:
    printf("tstksds %d run, %d passed, %d failed\n", mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
