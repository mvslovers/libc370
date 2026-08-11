#include <clibio.h>
#include <clibos.h>
#include <ctype.h>
#include <modmap.h>

static int      relocate_load(FILE *fp, unsigned lowlp, unsigned highlp, unsigned size);
static int      process_rldr(unsigned char *buf, size_t reclen,
                             unsigned lowlp, unsigned highlp, unsigned size);
static unsigned fetch(unsigned address, unsigned size);
static int      store(unsigned address, unsigned value, unsigned size);

/* load a module into high memory (CSA) */
__asm__("\n&FUNC    SETC '__loadhi'");
int __loadhi(const char *module, void **lpa, void **epa, unsigned *sz)
{
    void        *highep = 0;    /* relocated entry point address    */
    void        *highlp = 0;    /* relocated load point address     */
    void        *lowep  = 0;    /* low entry point address          */
    void        *lowlp  = 0;    /* low load point address           */
    unsigned    size    = 0;    /* size of module                   */
    unsigned    offep   = 0;    /* offset of entry point            */
    FILE        *fp     = 0;    /* file handle, tested at quit:     */
    CDE         *cde;
    void        *dcb;
    char        member[12]   = "        ";
    char        filename[56] = {0};
    int         i;

    for(i=0; i < 8 && module[i]; i++) {
        member[i] = toupper(module[i]);
    }

    /* we only load from the task STEPLIB, so grab the STEPLIB DCB */
    dcb = __steplb();

    /* load module into low storage (private area) */
    lowep = __load(dcb, member, 0, 0);
    if (!lowep) {
        wtof("%s unable to LOAD \"%s\" into private storage", __func__, member);
        goto quit;
    }

    /* get the CDE for this load module */
    cde = clib_find_cde(member);
    if (!cde) {
        wtof("%s CDE not found for \"%s\"", __func__, member);
        goto quit;
    }

    /* get the load point address */
    size  = cde->xtlst->xtlmsbla & 0x00FFFFFF;
    lowlp = (void*)cde->xtlst->xtlmsbaa;

    /* calculate offset of entry point */
    offep = (unsigned)lowep - (unsigned)lowlp;

    /* allocate CSA memory for module */
    highlp = getmain(size, 241);
    if (!highlp) {
        wtof("%s unable to allocate storage for %u bytes from subpool 241", __func__, size);
        goto quit;
    }

    /* copy the module to the getmain'd storage */
    memcpy(highlp, lowlp, size);

    /* note: After the memcpy, the highlp storage has incorrect adcons
    **       values since the lowlp module was loaded by the system routines.
    **       So we'll have to obtain the relocation info from the
    **       load module dataset (STEPLIB) and correct the adcon values
    **       in the highlp storage.
    */

    /* the load module will be read from the STEPLIB dataset to extract the RLD records */
    strcpy(filename, "DD:STEPLIB(");
    strcat(filename, member);
    strcat(filename, ")");
    fp = fopen(filename, "rb");
    if (!fp) {
        wtof("%s unable to open \"%s\" for reading", __func__, filename);
        goto quit;
    }
#if 0
    wtodumpf(lowlp, size, member);
#endif
    /* relocate the lowlp adcons to the highlp adcons.  A module we could not
    ** relocate cleanly must not be published: half-relocated code in CSA is
    ** worse than no module at all, so the caller gets the failure instead.
    */
    if (relocate_load(fp, (unsigned)lowlp, (unsigned)highlp, size) != 0) {
        wtof("%s relocation of \"%s\" failed, module not loaded", __func__, member);
        goto quit;
    }

    /* calculate high entry point address */
    highep = (void*)((unsigned)highlp + offep);

quit:
    /* cleanup resources we used */
    if (fp)     fclose(fp);
    if (lowep)  __delete(module);

    if (highep) {
        /* success */
        if (lpa) *lpa = highlp;
        if (epa) *epa = highep;
        if (sz)  *sz  = size;
        return 0;
    }
    else {
        /* failure */
        if (highlp) freemain(highlp);
        return 4;
    }
}

__asm__("\n&FUNC    SETC 'relocate_load'");
static int
relocate_load(FILE *fp, unsigned lowlp, unsigned highlp, unsigned size)
{
    int             rc      = 0;
    size_t          read    = 0;
    unsigned char   *dptr   = NULL;
    MMOD            *mmod   = NULL;
    int             loadtext= 0;
    int             bad     = 0;

    /* read each record from the load module */
    for(;;) {
        /* we use __aread() for record oriented i/o */
        if (__aread(fp->dcb, &dptr, &read) != 0) {
            /* end of file */
            fp->flags |= _FILE_FLAG_EOF;
            break;
        }

        if (loadtext) {
            loadtext = 0;
            continue;
        }

        mmod = (MMOD*)dptr;

        /* check for control record that are always followed by a text record */
        if ((mmod->id & MMOD_ID_CTL) == MMOD_ID_CTL) {
            /* next record will be a text record */
            loadtext = 1;
        }

        /* process RLD records we've read */
        if ((mmod->id & MMOD_ID_RLD) == MMOD_ID_RLD) {
            bad = process_rldr(dptr, read, lowlp, highlp, size);
            if (bad) {
                wtof("%s %d RLD item(s) address outside the %u byte module "
                     "and were not relocated", __func__, bad, size);
                rc = 4;
            }
        }
    }

    return rc;
}

static __inline unsigned fetch(unsigned address, unsigned size)
{
    unsigned    value   = 0;
    unsigned    n;

    for(n = 0; n < size; n++) {
        value <<= 8;
        value |= (unsigned) (*(unsigned char *)(address+n));
    }

#if 0
    wtof("FETCH(%08X,%u) VALUE(%08X)", address, size, value);
#endif
    return value;
}

static __inline int store(unsigned address, unsigned value, unsigned size)
{
    char    *addr   = (char *)address;
    char    *val    = (char *)(((unsigned)&value + sizeof(unsigned)) - size);

    memcpy(addr, val, size);

    return 0;
}

/* Walk one RLD record and relocate the adcons it names.
**
** The RLD data is a stream of items.  A full item is 8 bytes -- the relocation
** and position ESD ids, then a flag byte and a 3 byte offset.  When an item's
** T bit (RLD_FLAG_SAME) is set the NEXT item shares those two ids and omits
** them, so it is only 4 bytes.
**
** The walk is bounded by the record's own RLD byte count, never by the T bit.
** That distinction is the whole of #100: an ld370 before mvslovers/cc370#42
** left the inherited T bit set on the item that ended up last in a record,
** claiming a continuation item the record does not contain.  Following it read
** 4 bytes past the RLD data -- and for RECFM=U __aread() hands back a whole
** BLKSIZE buffer, so what sits there is the tail of the previous, longer
** record.  That residue reads as a perfectly plausible item whose 3 byte
** offset then lands anywhere at all.  Program fetch bounds its own walk by the
** byte count and is unaffected; we now do the same, which also keeps every
** module an older ld370 already produced working.
**
** Returns the number of items refused because their offset is not inside the
** module -- 0 when the record relocated cleanly.
*/
__asm__("\n&FUNC    SETC 'process_rldr'");
static int
process_rldr(unsigned char *buf, size_t reclen,
             unsigned lowlp, unsigned highlp, unsigned size)
{
    int             rejected = 0;
    int             same    = 0;    /* previous item set T: this one is short */
    MMRLDR          *rldr   = (MMRLDR*)buf;
    unsigned char   *p;
    unsigned char   *end;
    unsigned char   flag;
    unsigned        address;
    unsigned        value;
    unsigned        isize;
    unsigned        offset;
    unsigned        count;

    /* How many bytes of RLD data this record claims -- capped at what __aread()
    ** actually handed us before it ever becomes a pointer, so a garbage count
    ** cannot produce an out of range address just by being added.
    ** ld370 emits control records and RLD records separately, so ctlcnt is 0 on
    ** every record that gets here; a combined control+RLD record would hold its
    ** control data in this range and the items read out of it are caught by the
    ** offset check below.
    */
    count = rldr->rldcnt;
    if (reclen > sizeof(MMRLDR) && count > reclen - sizeof(MMRLDR)) {
        count = (unsigned)(reclen - sizeof(MMRLDR));
    }

    p   = (unsigned char*)rldr->data;
    end = p + count;

    while (p + 4 <= end) {
        if (!same) {
            /* this item carries its own relocation/position ids first */
            if (p + 8 > end) break;     /* truncated -- stop, do not guess */
            p += 4;
        }

        flag    = p[0];
        offset  = GET3(p + 1);
        p      += 4;
        same    = (flag & RLD_FLAG_SAME) != 0;   /* governs the NEXT item */

        /* don't process unresolved relocation entries */
        if (flag & RLD_FLAG_UNRES) continue;

        /* get the size of the target address */
        isize = flag & RLD_FLAG_LL;
        if (isize == RLD_FLAG_LL2) {
            isize = 2;
        }
        else if (isize == RLD_FLAG_LL3) {
            isize = 3;
        }
        else if (isize == RLD_FLAG_LL4) {
            isize = 4;
        }
        else {
            /* should never happen, but just in case */
            continue;
        }

        /* an adcon has to live inside the module we just copied.  Without this
        ** a bad offset is a store into whatever the arithmetic produced -- a
        ** silent S0C4 with nothing to point at its cause.
        */
        if (offset + isize > size) {
            rejected++;
            continue;
        }

        /* get current relocated value from module in memory (lowlp) */
        address = lowlp + offset;                       /* address of adcon in private area (lowlp) */
        value = fetch(address, isize);                  /* fetch the current adcon value */

        /* remove the relocation value from the adcon value (lowlp) */
        value -= lowlp;

        /* adjust the adcon value in the CSA storage (highlp) */
        value += highlp;                                /* new adcon value */
        address = highlp + offset;                      /* address of adcon in CSA */
        store(address, value, isize);                   /* store adcon with relocated value */
    }

    return rejected;
}
