/*
 * tstdscb4.c - libc370 #173 on MVS: struct dscb4 lays out what __dscbv()
 * returns.
 *
 * __dscbv() reads the format-4 DSCB with OBTAIN SEARCH, which returns the
 * 96-byte data portion: DS4IDFMT at offset 0.  struct dscb4 began with the
 * 44-byte key, so every field was read 44 bytes too far; behind DS4DEVTK
 * it was off by 2 more (DS4DEVOV is a halfword overlaying DS4DEVI and
 * DS4DEVL, not a field of its own) and behind DS4AMTIM by 5 (DS4AMCAT
 * overlays DS4VSIND and DS4VSCRA).
 *
 * For each volume the probe reads the format-4 DSCB and compares every
 * field of struct dscb4 with the bytes at the IECSDSL1 offset (MVS 3.8j
 * macro), then checks the geometry against the known device constants.
 * The volumes are those of SYS1.LINKLIB and of SYS1.PROCLIB, and PUB000
 * when it is mounted.
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * M an origin/main checkout - the change is in the header only):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstdscb4.c \
 *         -o $S/TSTDS4G -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$M/include -L build/sdk \
 *         test/mvs/tstdscb4.c -o $S/TSTDS4R -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTDS4G=TSTDS4G.iebcopy \
 *         TSTDS4R=TSTDS4R.iebcopy -o d4 -xmit --dsn IBMUSER.LIBC370.D4SCR
 *
 * then jcl/recvdscb4.jcl and jcl/tstdscb4.jcl.
 *
 * Own MVS/CE container, 2026-10-20 (MVSRES a 3350, PUB000 a 3380):
 * GREEN CC 0000, 61 of 61; RED CC 0008, 13 of 61 - devtk 256 instead of
 * 19254 on the 3350, sizeof(DSCB4) 146.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <string.h>
#include <mvs/wto.h>
#include <mvs/dscb.h>

static int bad, run;

static void want(int cond, const char *what, unsigned got, unsigned ref)
{
    run++;
    if (cond) return;
    bad++;
    wtof("TSTDSCB4 FAIL: %s is %u, IECSDSL1 says %u", what, got, ref);
}

static unsigned hw(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

/* the device constants of the 3330, 3350, 3375, 3380 and 3390 */
static const struct {
    const char      *dev;
    unsigned short  trk, len;
} geo[] = {
    { "3330", 19, 13165 }, { "3350", 30, 19254 }, { "3375", 12, 36000 },
    { "3380", 15, 47968 }, { "3390", 15, 58786 },
};

static void volume(const char vol[6])
{
    static DSCB         dscb;
    const DSCB4         *d4 = &dscb.dscb4;
    const unsigned char *v = (const unsigned char *)dscb.work;
    const char          *dev = 0;
    int                 rc;
    unsigned            i;

    memset(&dscb, 0, sizeof(dscb));
    rc = __dscbv(vol, &dscb);
    if (rc) {
        wtof("TSTDSCB4   %-6.6s: __dscbv() rc %d, skipped", vol, rc);
        return;
    }
    run++;
    if (v[0] != 0xF4) {
        bad++;
        wtof("TSTDSCB4 FAIL: %-6.6s: no format-4 DSCB", vol);
        return;
    }

    wtof("TSTDSCB4   %-6.6s through struct: fmtid %02X dstrk %u "
         "devtk %u devov %u devk %u devdt %u", vol,
         (unsigned char)d4->fmtid, d4->dstrk, d4->devtk, d4->devov,
         d4->devk, d4->devdt);
    wtof("TSTDSCB4   %-6.6s at IECSDSL1 offsets: dstrk %u devtk %u "
         "devov %u devk %u devdt %u", vol, hw(v + 20), hw(v + 22),
         hw(v + 24), v[26], v[30]);

    want((unsigned char)d4->fmtid == 0xF4, "fmtid", (unsigned char)d4->fmtid, v[0]);
    want(memcmp(d4->hpchr, v + 1, 5) == 0, "hpchr", d4->hpchr[0], v[1]);
    want(d4->dsrec == hw(v + 6), "dsrec", d4->dsrec, hw(v + 6));
    want(d4->noext == v[15], "noext", d4->noext, v[15]);
    want(d4->dscyl == hw(v + 18), "dscyl", d4->dscyl, hw(v + 18));
    want(d4->dstrk == hw(v + 20), "dstrk", d4->dstrk, hw(v + 20));
    want(d4->devtk == hw(v + 22), "devtk", d4->devtk, hw(v + 22));
    want(d4->devov == hw(v + 24), "devov", d4->devov, hw(v + 24));
    want(d4->devi == v[24], "devi", d4->devi, v[24]);
    want(d4->devl == v[25], "devl", d4->devl, v[25]);
    want(d4->devk == v[26], "devk", d4->devk, v[26]);
    want(d4->devfg == v[27], "devfg", d4->devfg, v[27]);
    want(d4->devtl == hw(v + 28), "devtl", d4->devtl, hw(v + 28));
    want(d4->devdt == v[30], "devdt", d4->devdt, v[30]);
    want(d4->devdb == v[31], "devdb", d4->devdb, v[31]);
    want(d4->vsind == v[40], "vsind", d4->vsind, v[40]);
    want(memcmp(d4->f6ptr, v + 56, 5) == 0, "f6ptr", d4->f6ptr[0], v[56]);
    want(memcmp(&d4->extent[0], v + 61, 10) == 0, "extent[0] (DS4VTOCE)",
         d4->extent[0].lower[1], v[61 + 3]);

    for (i = 0; i < sizeof(geo) / sizeof(geo[0]); i++)
        if (hw(v + 20) == geo[i].trk && hw(v + 22) == geo[i].len)
            dev = geo[i].dev;
    run++;
    if (dev)
        wtof("TSTDSCB4   %-6.6s is a %s", vol, dev);
    else {
        bad++;
        wtof("TSTDSCB4 FAIL: %-6.6s: %u tracks of %u bytes, no known device",
             vol, hw(v + 20), hw(v + 22));
    }
}

int main(void)
{
    static const char   *dsn[] = { "SYS1.LINKLIB", "SYS1.PROCLIB" };
    static LOCWORK      loc;
    char                name[44];
    unsigned            i;

    for (i = 0; i < sizeof(dsn) / sizeof(dsn[0]); i++) {
        memset(name, ' ', sizeof(name));
        memcpy(name, dsn[i], strlen(dsn[i]));
        memset(&loc, 0, sizeof(loc));
        if (__locate(name, &loc) == 0)
            volume(loc.volser);
        else
            wtof("TSTDSCB4   __locate(%s) failed", dsn[i]);
    }
    volume("PUB000");

    run++;
    if (sizeof(DSCB4) != 96) {
        bad++;
        wtof("TSTDSCB4 FAIL: sizeof(DSCB4) is %u, the data portion 96",
             (unsigned)sizeof(DSCB4));
    }

    wtof("TSTDSCB4 %s: %d of %d checks passed", bad ? "FAILED" : "PASSED",
         run - bad, run);
    return bad ? 8 : 0;
}
