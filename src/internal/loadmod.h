#ifndef SRC_INTERNAL_LOADMOD_H
#define SRC_INTERNAL_LOADMOD_H
/* loadmod.h - internal: the record layouts of a load module on disk
** (CESD, RLD, CTL, IDR), as __loadhi() reads them.
**
** libc370 2.0 keeps this half of 1.x modmap.h; the module-map API it also
** declared (modmap_new(), modmap_open(), ...) was never implemented (#274).
*/

/* Load module mapping records */
typedef struct mmcesdr      MMCESDR;/* Module Map CESD record           */
typedef struct mmrldr       MMRLDR; /* Module Map RLD record            */
typedef struct mmidr        MMIDR;  /* Module Map IDR record            */
typedef struct mmctl        MMCTL;  /* Module Map CTL record            */

/* Module Map mapping internals */
typedef struct mmod         MMOD;   /* Load module record id            */
typedef struct mmesd        MMESD;  /* Module Map ESD                   */
typedef struct mmrld        MMRLD;  /* Relocation Dictionary            */
typedef struct mmrldf       MMRLDF; /* Relocation Dict Flag and Address */
typedef struct mmctld       MMCTLD; /* Control Record Data              */

#if !defined(GET3)
/* Use GET3 to convert a 3 byte character array to a value.
** Example: int addr = (int) GET3(esd->address);
*/
#define GET3(s) ( \
    ((unsigned char*)(s))[0] << 16 | \
    ((unsigned char*)(s))[1] <<  8 | \
    ((unsigned char*)(s))[2] )
#endif

/* Module record identifier */
struct mmod {
    unsigned char   id;             /* 00 ID                            */
#define MMOD_ID_CTL         0x01    /* ... Control record               */
#define MMOD_ID_RLD         0x02    /* ... Relocation record            */
#define MMOD_ID_CTLRLD      0x03    /* ... Control Relocation record    */
#define MMOD_ID_CTLEOS      0x05    /* ... Control EOS record           */
#define MMOD_ID_RLDEOS      0x06    /* ... Relocation EOS record        */
#define MMOD_ID_CTLRLDEOS   0x07    /* ... Control Relocation EOS rec   */
#define MMOD_ID_CTLEOM      0x0D    /* ... Control EOM record           */
#define MMOD_ID_RLDEOM      0x0E    /* ... Relocation EOM record        */
#define MMOD_ID_CTLRLDEOM   0x0F    /* ... Control Relocation EOM rec   */
#define MMOD_ID_SCAT        0x10    /* ... Scatter/Translation record   */
#define MMOD_ID_CESD        0x20    /* ... CESD record                  */
#define MMOD_ID_SYM         0x40    /* ... SYM record                   */
#define MMOD_ID_IDR         0x80    /* ... IDR record                   */
};

/* Module Map ESD entry */
struct mmesd {
    unsigned char   symbol[8];      /* 00 symbol name                   */
    unsigned char   type;           /* 08 type                          */
#define ESD_TYPE_SD     0x00        /* ... Section Definition           */
#define ESD_TYPE_ER     0x02        /* ... External Reference           */
#define ESD_TYPE_LR     0x03        /* ... Label Reference              */
#define ESD_TYPE_PC     0x04        /* ... Private Code                 */
#define ESD_TYPE_CM     0x05        /* ... Common                       */
#define ESD_TYPE_PR     0x06        /* ... Pseudo Register              */
#define ESD_TYPE_NULL   0x07        /* ... NULL                         */
#define ESD_TYPE_WX     0x0A        /* ... Weak External Reference      */
#define ESD_TYPE_QSD    0x0D        /* ... Quad Aligned SD              */
#define ESD_TYPE_QPC    0x0E        /* ... Quad Aligned PC              */
#define ESD_TYPE_QCM    0x0F        /* ... Quad Aligned CM              */
#define ESD_TYPE_DPC    0x14        /* ... Deleted PC                   */

    unsigned char   address[3];     /* 09 address (3 bytes)             */
    unsigned char   flag;           /* 0C flags                         */
#define ESD_FLAG_RMODE64    0x20    /* ... RMODE 64 if 1                */
#define ESD_FLAG_AMODE64    0x10    /* ... AMODE 64 if 1                */
#define ESD_FLAG_RSECT      0x08    /* ... RSECT if 1                   */
#define ESD_FLAG_RMODE31    0x04    /* ... RMODE 31 if 1, 24 if 0       */
#define ESD_FLAG_AMODE      0x03    /* ... 00=24, 01=24, 10=31, 11=ANY  */

    unsigned char   id[3];          /* 0D Length(SD,PC,CM,PR), ID=(LR)  */
};

/* Module Map Consolidated ESD record */
struct mmcesdr {
    unsigned char   id;             /* 00 MMOD_ID_CESD (0x20)           */
    unsigned char   flag;           /* 01 flag byte                     */
    unsigned char   spare[2];       /* 02 binary zeros                  */
    unsigned short  esdid;          /* 04 ESD ID of first ESD item      */
    unsigned short  count;          /* 06 count in bytes of ESD data    */
    MMESD           esd[1];         /* 08 start of ESD data             */
};

/* Module Map Relocation Dictionary Flag and Address */
struct mmrldf {
    unsigned char   flag;           /* 00 x'xxxxLLST'                   */
#define RLD_FLAG_UNRES  0x80        /* ... unresolved external          */
#define RLD_FLAG_ADD4   0x40        /* ... add 4 to the LL value        */
#define RLD_FLAG_TYPE   0x30        /* ... RLD type bits                */
#define RLD_FLAG_TYPEA  0x00        /* ... DC A(name)                   */
#define RLD_FLAG_TYPEV  0x10        /* ... DC V(name)                   */
#define RLD_FLAG_TYPEPR 0x20        /* ... Pseudo register disp         */
#define RLD_FLAG_TYPECP 0x30        /* ... Cumulative pseudo register   */
#define RLD_FLAG_LL     0x0C        /* ... LL mask                      */
#define RLD_FLAG_LL2    0x04        /* ... LL two byte address          */
#define RLD_FLAG_LL3    0x08        /* ... LL three byte address        */
#define RLD_FLAG_LL4    0x0C        /* ... LL four byte address         */
#define RLD_FLAG_NEG    0x02        /* ... S negative relocation dir    */
#define RLD_FLAG_SAME   0x01        /* ... T on next item is MMRLDF     */
                                    /*       off next item is MMRLD     */
    unsigned char   address[3];     /* 01 address (3 bytes)             */
};

/* Module Map Relocation Dictionary */
struct mmrld {
    unsigned short  relid;          /* 00 entry id of relocation symbol */
    unsigned short  posid;          /* 02 entry id of position symbol   */
    MMRLDF          mmrldf[1];      /* 04 RLD flags and address         */
};

/* Module Map Relocation Dictionary Record */
#if 0
struct mmrldr {
    unsigned char   id;             /* 00 MMOD_ID_RLD[[EOS|EOM]]        */
    unsigned char   spare[2];       /* 01 binary zeros                  */
    unsigned char   count;          /* 03 number of RLD following text  */
    unsigned char   count2[2];      /* 04 binary zeros                  */
    unsigned short  bytes;          /* 06 number of RLD in bytes        */
    unsigned char   spare8[8];      /* 08 binary zeros                  */
    MMRLD           rld[1];         /* 10 start of RLD data             */
};
#else
struct mmrldr {
    unsigned char   id;             /* 00 MMOD_ID_CTL[RLD]              */
    unsigned char   spare[3];       /* 01 always zero                   */
    unsigned short  ctlcnt;         /* 04 length of control data        */
    unsigned short  rldcnt;         /* 06 length of relocation data     */
    unsigned char   ccw[8];         /* 08 CCW or zeros                  */
    unsigned char   data[0];        /* 10 start of data                 */
} __attribute__((packed));
#endif

/* Control Record */
struct mmctl {
    unsigned char   id;             /* 00 MMOD_ID_CTL[RLD]              */
    unsigned char   spare[3];       /* 01 always zero                   */
    unsigned short  ctlcnt;         /* 04 length of control data        */
    unsigned short  rldcnt;         /* 06 length of relocation data     */
    unsigned char   ccw[8];         /* 08 CCW or zeros                  */
    unsigned char   data[0];        /* 10 start of data                 */
} __attribute__((packed));

/* Control Record Data */
struct mmctld {
    unsigned short  esdid;          /* 00 ESD ID                        */
    unsigned short  count;          /* 02 ESD size in bytes             */
} __attribute__((packed));

/* Identification Record */
struct mmidr {
    unsigned char   id;             /* 00 MMOD_ID_IDR 0x80              */
    unsigned char   bytes;          /* 01 byte count of IDR data (6-255)*/
    unsigned char   type;           /* 02 sub-type of IDR data          */
#define IDR_TYPE        0x0F        /* ... mask for type values         */
#define IDR_TYPE_ZAP    0x01        /* ... data supplied by HMASPZAP    */
#define IDR_TYPE_LINK   0x02        /* ... linkage editor data          */
#define IDR_TYPE_TRAN   0x04        /* ... translator supplied data     */
#define IDR_TYPE_USER   0x08        /* ... user (system) supplied data  */
#define IDR_LAST        0x80        /* ... indicates last IDR record    */
    unsigned char   data[1];        /* 03 start of data                 */
};


#endif  /* SRC_INTERNAL_LOADMOD_H */
