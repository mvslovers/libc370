# Waterloo C 3.2 — Ideen für LIBC/370, eine Full-Screen-Bibliothek und CC/370

Stand: 2026-11-03 · Status: Arbeitspapier, nichts davon ist beschlossen

## Worum es geht

Waterloo C 3.2 (WATCOM / University of Waterloo, Februar 1989) war ein
ANSI-C-Compiler mit Laufzeitbibliothek für OS, MVS und MVS/XA. Der Compiler
selbst ist für uns kein Kandidat: kein Quelltext, und die Lizenz der
MUSIC/SP-Demo verbietet die Weitergabe „for use under any other system".
Die **Schnittstellen** der Bibliothek sind dagegen eine gute Ideenquelle.
Dieses Papier hält fest, was sich davon für LIBC/370 (libc370) lohnt, wie es
zu dem passt, was libc370 heute hat, und wie eine einheitliche
Full-Screen-Bibliothek aussehen könnte. Am Ende stehen Notizen für CC/370
(cc370) und einen späteren eigenen Compiler.

Die Vorschläge bleiben innerhalb der Grenzen des Ziels: 24 Bit, C99 (gnu99),
EBCDIC zur Laufzeit, statisches Linken. Wo ein Vorschlag eine POSIX-Form
annimmt (`opendir`, `stat`, `fmemopen`), ist das eine Fassade über
MVS-Funktionen, keine Abhängigkeit von POSIX.

Übernommen werden Ideen und Schnittstellenformen, **kein Code**. Die
Waterloo-Header tragen „Copyright (C) 1989 by the University of Waterloo".

### Quellen

| Quelle | Ort | Inhalt |
|---|---|---|
| 51 Header `clib.*.h` | `~/repos/WATC/watc_ascii/` (ASCII), `watc_ebcdic_binary/` (EBCDIC) | Prototypen, Strukturen, Konstanten |
| User's Guide 3.2 (Auszug, gescannt) | `~/repos/WATC/watc_ugd_part/WatC_UserGd_1..5.pdf` | S. 91–184: Sprache, MVS-Besonderheiten, Optionen, Laufzeit, Linkage |
| MUSIC/SP-Hilfe | `~/repos/WATC/watc_ebcdic_binary/watc_help.htm` | MUSIC-spezifische Einschränkungen |
| MUSIC/SP-Demo 6.2 | mvsdev `~/MUSIC-DEMO`, Userid `$WCC`, `$SUB:SUBLIB.CW` | lauffähiger Compiler, Runtime, 340 Hilfetexte `@#32.*` |
| Produktseite | http://csgwww.uwaterloo.ca/sdtp/watc.html | Feature-Übersicht |

Die eigentliche „Run-time Library Reference" (442 Seiten) liegt nicht vor.
Die Semantik einzelner Funktionen ist hier deshalb aus Prototypen und dem
User's Guide erschlossen. Wo das unsicher ist, steht es dabei.

### Übersicht

| # | Vorschlag | Waterloo | libc370 heute | Nutzen |
|---|---|---|---|---|
| 1 | `stat()` für Data Sets | `stat.h` | Bausteine da (`__locate`, `__dscbdv`, `__listpd`), kein einheitlicher Aufruf | hoch |
| 2 | Treiber-Tabelle für `fopen` | `ddriver.h`, `_drvadd` | `__fpopn()` mit Callbacks, aber kein offenes Register | hoch |
| 3 | Übersetzungshilfen + Standardtabellen | `atoe`, `etoa`, `memxlt`, `atoetbl` | nur Tabellen als Beispielcode in der Doku | mittel–hoch |
| 4 | Öffentliche TSO-Terminal-API | `tsolib.h` | TPUT nur intern in stdio, Primitive privat in brexx370 | mittel |
| 5 | Feinere Program-Check-Signale | `signal.h` | 6 ANSI-Signale, ESTAE/`try()` | mittel |
| 6 | `opendir`/`readdir` als Fassade | `dir.h` | `__listds`, `__listpd`, `__walkpd` | mittel |
| 7 | Positionieren nach Satznummer | `frseek`, `frtell` | `NOTE`/`POINT` intern, `rfile.h` ohne Seek | mittel |
| 8 | Steuerbare Kommandozeile + Umleitung | `setup.h`, `_parms` | Blanks + `"…"`, Hook `__premain()` | mittel |
| 9 | Packed-Decimal-Hilfen | `pdeclib.h` | nur Assembler-Beispiel in der Doku | mittel |
| – | Kleinigkeiten | diverse | fehlen | gering, billig |
| – | Full-Screen-Bibliothek | `fslib.h`, `fattrs.h`, `pfkeys.h`, `ds3270.h` | FSS in brexx370 | eigenes Projekt |
| – | Zeichensatz `(:` `:)` und Compiler-Notizen | User's Guide §6.3 | Kapitel „Character Sets" | Doku, cc370 |

Vorschlag für die Reihenfolge: **2 → 1 → 3**, dann **4** (Voraussetzung für die
Full-Screen-Bibliothek) und **7**. Die übrigen nach Bedarf.

Was es schon gibt und deshalb **nicht** vorgeschlagen wird: DCB-Attribute im
`fopen`-Modus (`"w,recfm=fb,lrecl=80"`, bei Waterloo `"XYZ (BIN LRECL 1024 RECFM F"`),
Katalogzugriff (`__locate`), Data-Set- und Member-Listen, Recovery
(`__estae`, `try()`), `tsocmd()`, der Startup-Hook `__premain()`. Die
fehlenden C99-Mathe-Funktionen (`hypot`, `erf`, `log2` …) stehen bereits in
`docs/c99-audit.md`, Waterloo bringt dort nichts Neues.

Namensgebung: Alle Namen unten sind Platzhalter. Die endgültigen Namen
richten sich nach der Namensregel aus #246 (3.0) und danach, ob etwas in einen
Standard-Header (`<stdio.h>`) oder einen MVS-Header (`<mvs/…>`) gehört.

---

## 1 · `stat()` für Data Sets

### Waterloo

`clib.stat.h` definiert ein `struct stat`, das auf MVS zugeschnitten ist und
mit POSIX nur den Namen teilt:

```c
struct stat {
    char   st_dsname[45];   /* data set name                      */
    char   st_mname[9];     /* PDS member name                    */
    short  st_dsorg;        /* S_ORGPS, S_ORGPO, S_ORGDA, S_ORGAM */
    int    st_size;         /* Anzahl Sätze                       */
    int    st_blocks;       /* Anzahl Blöcke                      */
    int    st_blksize;
    int    st_lrecl;
    time_t st_mtime;        /* letzte Änderung                    */
    time_t st_dctime;       /* Erstellung                         */
    char   st_disp;
    char   st_recfm;        /* S_FIX|S_VAR|S_UND, S_RFB, S_ASA …  */
    lab_t  st_dev_lab;      /* erstes Volume                      */
    char   st_pds_alias;    /* Member ist Alias                   */
};
```

Dazu `stat(name, &st)` und `fstat(fd, &st)`, die DSORG- und RECFM-Bits als
Konstanten (`S_ORGPS = 0x4000`, `S_ORGPO = 0x0200`, `S_FIX = 0x80`,
`S_RFB = 0x10`, `S_ASA = 0x04` …), also im Layout von DSCB und JFCB.

### libc370 heute

Die Bausteine sind vorhanden, nur nicht zusammengeführt:

- `__locate()` (`<mvs/dscb.h>`): Katalog → Volume
- `__dscbdv()`: Format-1-DSCB eines Data Sets auf einem Volume
- `__listpd()` / `__walkpd()` (`<mvs/pds.h>`): Member mit ISPF-Statistik und Load-Modul-Daten
- `ibm/mvs/iefjfcbn.h`: JFCB für schon offene Dateien

Wer heute „Ist das ein PDS, wie lang sind die Sätze?" wissen will, ruft drei
Funktionen und kennt die DSCB-Offsets.

### Vorschlag

Eine Funktion, die Katalog, DSCB und gegebenenfalls das Member zusammenführt,
und eine zweite für offene Streams:

```c
#include <mvs/dsstat.h>

typedef struct dsstat {
    char      dsn[45];
    char      member[9];
    char      volser[7];
    unsigned  dsorg;        /* DSORG_PS, DSORG_PO, DSORG_DA, DSORG_VS     */
    unsigned  recfm;        /* RECFM_F, RECFM_V, RECFM_U, RECFM_B, RECFM_A … */
    int       lrecl;
    int       blksize;
    time_t    created;      /* DS1CREDT                                  */
    time_t    modified;     /* Member: ISPF-Statistik, sonst 0           */
    unsigned  tracks;       /* belegte Spuren (aus den Extents)          */
    int       is_alias;     /* nur für Member                            */
} DSSTAT;

int __dsstat (const char *name, DSSTAT *st);   /* "'A.B.C'", "'A.B(M)'", "DD:SYSUT1" */
int __fdsstat(FILE *fp, DSSTAT *st);            /* aus DCB/JFCB des offenen Streams   */
```

```c
DSSTAT st;
if (__dsstat("'SYS1.MACLIB(SAVE)'", &st) == 0)
    printf("%s(%s) vol=%s %s lrecl=%d blksize=%d\n",
           st.dsn, st.member, st.volser,
           st.dsorg & DSORG_PO ? "PO" : "PS", st.lrecl, st.blksize);
```

Optional darüber ein POSIX-nahes `<sys/stat.h>` mit `stat()`, das `st_mode`
grob füllt (`S_IFREG` für sequenziell und Member, `S_IFDIR` für einen PDS ohne
Member). Das hilft beim Portieren von Code, der nur „Gibt es die Datei?" und
„Ist das ein Verzeichnis?" fragt.

### Offen

- Welche Datumsfelder liefert der Format-1-DSCB unter MVS 3.8j zuverlässig?
  Das Erstelldatum ja, ein Änderungs- oder Referenzdatum vermutlich nicht. Für
  Member gibt es die ISPF-Statistik, wenn sie gepflegt ist.
- `st_size` in Bytes ist für RECFM=V nicht ohne Lesen ermittelbar. Waterloo
  gibt deshalb die Anzahl Sätze an. Vorschlag: Spuren bzw. Blöcke statt Bytes.
- Waterloo unter MUSIC lieferte `st_recfm` und `st_lrecl` als 0
  (`watc_help.htm`). Ein Hinweis, dass `fstat` auf offenen Dateien ohne
  JFCB-Zugriff nichts taugt.

---

## 2 · Treiber-Tabelle für `fopen`

### Waterloo

`fopen` kennt drei Arten von Namen (User's Guide §7.2.3, S. 146–150):

1. **DD-Name** mit Optionen: `fopen("XYZ (BIN LRECL 1024 RECFM F", "w")`
2. **Data-Set-Name**, wird dynamisch allokiert: `fopen("'TEST.FILE'", "w")`
3. **Gerätetreibername**: Beim Start registriert die Laufzeit `_NULL` und
   `_TERMINAL.`. `_TERMINAL.STDOUT` schreibt auf den DD `STDOUT`, wenn er
   allokiert ist, sonst per TPUT auf das TSO-Terminal. So öffnet die Laufzeit
   `stdin`, `stdout` und `stderr`.

Treiber registriert man zur Laufzeit (`clib.ddriver.h`):

```c
typedef struct device_driver_desc {
    struct device_driver_desc *dd_next;    /* verkettete Liste            */
    int (* const (*dd_driver))();          /* Funktionstabelle des Treibers */
    size_t dd_name_length;
    char   dd_name[];                      /* Gerätename                  */
} device_driver_desc;

void _drvadd    (char *devname, size_t len, int (* const (*driver))());
void _drvdisable(char *devname, size_t len);
```

Der Aufbau der Funktionstabelle (welche Einträge, welche Reihenfolge) ist im
vorliegenden Material nicht dokumentiert.

### libc370 heute

`fopen()` ist schon fast so gebaut (`src/stdio/fopen.c`):

```c
FILE *fopen(const char *fn, const char *mode)
{
    return __fpopn(fn, mode, tsoname, dsalloc);
}
```

`__fpopn()` bekommt die Namensbehandlung als Callbacks. Das Muster ist da, aber
geschlossen: Eine dritte Bibliothek kann sich nicht einhängen.

### Vorschlag

Ein kleines Register von Präfixen mit einer Operationstabelle. Treiber
liefern Bytestrom-Semantik, die Pufferung bleibt in stdio.

```c
#include <mvs/fdriver.h>

typedef struct fdriver_ops {
    void   *(*open) (const char *name, const char *mode, int *err);
    ssize_t (*read) (void *h, void *buf, size_t n);
    ssize_t (*write)(void *h, const void *buf, size_t n);
    long    (*seek) (void *h, long off, int whence);   /* NULL: nicht positionierbar */
    int     (*flush)(void *h);
    int     (*close)(void *h);
} FDRIVER_OPS;

int __fdrvadd(const char *prefix, const FDRIVER_OPS *ops);   /* "UFS:", "/", "NULL:" */
int __fdrvdel(const char *prefix);
```

Anwendungsfall **UFSD (ufsd)**: Der Client `libufs` arbeitet heute mit einer
eigenen API (`ufsnew()`, `ufs_signon()`, `ufs_fopen(ufs, path, mode)`,
`ufs_diropen()` …). Ein Treiber für das Präfix `/` würde das hierarchische
Dateisystem zusätzlich mit normalem stdio nutzbar machen. Der Treiber lebt in
`libufs`, nicht in libc370: libc370 stellt nur das Register bereit und hängt
nicht von UFSD ab. Ein Programm, das den Treiber nicht anmeldet, bleibt
unverändert.

```c
/* in libufs: Treiber auf ufs_fopen/ufs_fread/… abbilden */
static void *uopen(const char *name, const char *mode, int *err)
{
    return ufs_fopen(__ufs_default(), name, mode);   /* Sitzung je Task */
}
static const FDRIVER_OPS ufs_fdriver = { uopen, uread, uwrite, useek, uflush, uclose };

int ufs_stdio_enable(void) { return __fdrvadd("/", &ufs_fdriver); }

/* in einer Anwendung, z. B. HTTPD */
ufs_stdio_enable();
FILE *cfg = fopen("/etc/httpd.conf", "r");
while (fgets(line, sizeof line, cfg)) ...
```

Weitere eingebaute Treiber, die sich damit fast von selbst ergeben:

- `NULL:` wie Waterloos `_NULL` (schreiben verwirft, lesen liefert EOF)
- Speicherstreams: `fmemopen()` und `open_memstream()` (POSIX 2008), praktisch für Tests und Formatierung
- `TERM:` wie `_TERMINAL.`: zuerst den DD, sonst das TSO-Terminal

### Offen

- Wie bekommt `FILE` den Treiberzeiger, ohne die bestehende Struktur (DCB-Bezug) aufzublähen?
- Reentranz: Das Register gehört in den WSA bzw. die CLIBPPA, nicht in statische Daten.
- UFSD braucht eine Sitzung mit Benutzer (`ufs_signon()`, ACEE). Welche Sitzung
  nimmt der Treiber: eine pro Task, angelegt beim ersten `fopen`, oder eine,
  die das Programm vorher setzt?
- Record-Semantik: Treiber, die Sätze statt Bytes liefern wollen, brauchen ein Flag.
  Waterloo hat dafür die Option `RAW` (ein `read()` = ein Satz).

---

## 3 · Übersetzungshilfen ASCII ↔ EBCDIC

### Waterloo

`clib.string.h` und `clib.memory.h`:

```c
extern const char atoetbl[], etoatbl[];        /* Standardtabellen         */
void *atoe  (void *buf, size_t buflen);         /* in place, ASCII → EBCDIC */
void *etoa  (void *buf, size_t buflen);         /* in place, EBCDIC → ASCII */
void *memxlt(void *buf, const char *table, size_t buflen);   /* beliebige Tabelle (TR) */
char *strxlt(char *str, const char *table);
```

Dazu hatte der Compiler die Option `ASCIIOUT`, die String- und
Zeichenkonstanten als ASCII erzeugt (User's Guide S. 102, siehe unten).

### libc370 heute

Die Bibliothek übersetzt bewusst nichts von selbst („The library translates
nothing on its own", `docs/books/guide/pg-charset.typ`). Das Kapitel liefert
die Tabellen CP037 ↔ Latin-1 mit der NEL/LF-Korrektur als Beispielcode
(`docs/books/ex/pg-charset/xlate.c`). Folge: Jedes Projekt bringt eigene
Tabellen und Schleifen mit. Eine grobe Suche nach typischen Namen (`e2a`,
`a2e`, `etoa`, `cp037`, `latin1` …) in den versionierten Quellen findet
Treffer in HTTPD (20 Dateien), FTPD (10), mvsMF (9), LUA/370 (4),
OFTPD (3), BREXX/370 (3) und HTTPREXX (1). Die Zahlen sind ein grobes Raster
mit möglichen Fehltreffern. CC/370 (17) ist ein Sonderfall: Der Compiler
übersetzt die Literale selbst.

### Vorschlag

Der Grundsatz bleibt: Die Bibliothek übersetzt nicht automatisch. Sie stellt
aber die eine richtige Tabelle und eine schnelle Funktion bereit.

```c
#include <ext/xlate.h>

extern const unsigned char __cp037_to_latin1[256];   /* X'15' → X'0A', X'25' → X'85' */
extern const unsigned char __latin1_to_cp037[256];   /* exakte Inverse               */

void *__memxlt(void *buf, size_t n, const unsigned char table[256]);
#define __e2a(buf, n) __memxlt((buf), (n), __cp037_to_latin1)
#define __a2e(buf, n) __memxlt((buf), (n), __latin1_to_cp037)
```

Umsetzung mit `TR` in Stücken zu 256 Bytes, der Rest per `EX`. Das ist das
gleiche Muster wie `memclr()` in `<ext/strutil.h>`:

```c
void *__memxlt(void *buf, size_t n, const unsigned char tab[256])
{
    unsigned char *p = buf;
    for (; n >= 256; p += 256, n -= 256)
        __asm__ __volatile__("TR 0(256,%0),0(%1)" : : "a"(p), "a"(tab) : "memory");
    if (n)
        /* EX auf ein TR-Template mit Länge n-1 */ ;
    return buf;
}
```

Typische Nutzung im Netzwerkcode:

```c
char line[256];
int len = snprintf(line, sizeof line, "HTTP/1.0 200 OK\r\n");
__e2a(line, len);
send(sock, line, len, 0);
```

### Offen

- Gehört das in `<ext/…>` oder in `<mvs/…>`? Und reicht CP037 ↔ Latin-1, oder
  braucht es weitere Paare (CP1047, CP273 für deutsche Terminals)?
- Bei weiteren Paaren lohnt sich ein Tabellenregister nach Name
  (`__xlate_table("IBM-1047", "ISO8859-1")`), eine Art Mini-`iconv`.

---

## 4 · Öffentliche TSO-Terminal-API

### Waterloo

`clib.tsolib.h`:

```c
int _tput   (const char *userid, size_t len, const char *buf, int flags);
int _tget   (size_t maxlen, char *buf, int flags);
int _stfson (int initial, int reshow_key);       /* STFSMODE ON  */
int _stfsoff(void);                               /* STFSMODE OFF */
int _stlineno(int line_number, int fsmodeon);     /* STLINENO     */
int _putline(iopl *p);                            /* PUTLINE/GETLINE mit IOPL */
int _getline(iopl *p);

/* Flags */
#define TGETPUT_EDIT     0x00
#define TGETPUT_ASIS     0x01
#define TGETPUT_CONTROL  0x02
#define TGETPUT_FULLSCR  0x03
#define TGETPUT_NOWAIT   0x10
#define TGETPUT_HOLD     0x08
#define TGETPUT_BREAK    0x04
```

### libc370 und brexx370 heute

- libc370 nutzt TPUT/TGET intern für die Terminal-Streams (`src/stdio/…`, `src/internal/fileio.h`), bietet aber keine öffentliche API. Öffentlich ist nur `tsocmd()` in `<mvs/tso.h>`.
- brexx370 hat eigene Primitive in `src/rxtso.c` (SVC 93 und 94):
  `stfsmode()`, `sttmpmd()`, `stlineno()`, `tput_fullscr()`, `tget_asis()`, `tget_nowait()`.
  Darauf setzt FSS auf.

### Vorschlag

Die brexx-Primitive in die libc heben, mit der Flag-Struktur von Waterloo:

```c
#include <mvs/tso.h>

#define TPUT_EDIT     0x00
#define TPUT_ASIS     0x01
#define TPUT_CONTROL  0x02
#define TPUT_FULLSCR  0x03
#define TPUT_HOLD     0x08
#define TPUT_BREAK    0x04
#define TGET_EDIT     0x00
#define TGET_ASIS     0x01
#define TGET_NOWAIT   0x10

int __tput(const void *buf, size_t len, int flags);
int __tget(void *buf, size_t maxlen, int flags);     /* Rückgabe: Länge oder -1 */
int __stfsmode(int on, int reshow_aid);              /* reshow: PA1/PA2 …       */
int __sttmpmd(int on);
int __stlineno(int line);
int __tsofg(void);                                    /* läuft im TSO-Vordergrund? */
```

```c
static const char screen[] = { 0xF5, 0xC3, 0x11, 0x40, 0x40, 0x1D, 0xF0, /* … */ };

__stfsmode(1, 0);
__tput(screen, sizeof screen, TPUT_FULLSCR);
n = __tget(inbuf, sizeof inbuf, TGET_ASIS);    /* AID, Cursor, geänderte Felder */
__stfsmode(0, 0);
```

Danach nutzen die Full-Screen-Bibliothek (unten), REKX/370 (rekx370) und
später BREXX/370 dieselben Funktionen. Für BREXX/370 erst nach 3.0.0: bis zum
Release (TK5 Update 6, Ende November 2026) gibt es dort nur Korrekturen.

---

## 5 · Feinere Program-Check-Signale

### Waterloo

`clib.signal.h` definiert Signale als Bitmasken über Ausnahmecodes. Ein
Signal fasst mehrere Program-Interrupts zusammen:

```c
#define SIGFPE  (_FLOAT_UNDER + _FLOAT_OVER + _FLOAT_DIVIDE + …)
#define SIGFIX  (_INTEGER_OVERFLOW + _INTEGER_DIVIDE)       /* S0C8, S0C9 */
#define SIGDEC  (_DECIMAL_OVERFLOW + _DECIMAL_DIVIDE)       /* S0CA, S0CB */
#define SIGPRO  (_PRIV_INST_CHECK + _PROTECTION_CHECK)      /* S0C2, S0C4 */
#define SIGOUT  (_OUT_OF_MEMORY + _STACK_OVER)
#define SIGALRM  _OUT_OF_TIME
#define SIGINT   _ATTN_TRAP                                 /* TSO-Attention */
#define _ABEND   0x02000000

void (*signal (int sig, void (*f)()))();
void (*ssignal(int sig, void (*f)()))();     /* Software-Signale */
int   gsignal(int sig);
void  psignal(int sig, const char *prefix);
```

Dazu Strukturen für die gesicherten Register und das PSW (`_savedreg`,
`_savedpsw`). Wie ein Handler darauf zugreift, ist im vorliegenden Material
nicht beschrieben.

### libc370 heute

`<signal.h>` hat die sechs ANSI-Signale (`SIGABRT` … `SIGTERM`) mit
`signal()` und `raise()`. Die Recovery läuft über `__estae()` und `try()` in
`<mvs/recovery.h>`. `@@ccall.asm` setzt beim Aufruf die SPIE des Aufrufers aus.
Noch zu prüfen: Auf welchem Weg landet ein S0C9 heute in `SIGFPE`?

### Vorschlag

Zusätzliche Signale außerhalb des strikten ANSI-Modus und ein Zugriff auf den
Unterbrechungszustand im Handler:

```c
#include <signal.h>

#define SIGFIX   7     /* Festkomma: S0C8, S0C9      */
#define SIGDEC   8     /* Dezimal:   S0CA, S0CB, S0C7 */
#define SIGPRO   9     /* Schutz:    S0C2, S0C4      */
#define SIGOUT  10     /* Speicher/Stack erschöpft   */
#define SIGATTN 11     /* TSO-Attention              */

typedef struct siginfo370 {
    int      intcode;     /* Program-Interrupt-Code (0x09 …)  */
    unsigned psw[2];
    unsigned gpr[16];
    unsigned ilc;
} SIGINFO370;

const SIGINFO370 *__siginfo(void);     /* gültig im Handler */
void psignal(int sig, const char *prefix);
```

```c
static void on_fix(int sig)
{
    const SIGINFO370 *si = __siginfo();
    fprintf(stderr, "S0C%X bei %06X\n", si->intcode, si->psw[1] & 0xFFFFFF);
    longjmp(recover, 1);
}

signal(SIGFIX, on_fix);
```

Nutzen vor allem für Interpreter (brexx370, lua370), die eine Division durch
null als Sprachfehler melden wollen statt mit Abend zu enden.

---

## 6 · `opendir`/`readdir` als Fassade

### Waterloo

`clib.dir.h`:

```c
struct direct {
    char d_name[57];   /* "'full.qual.dsn(member)'" */
    char d_ddn[9];     /* DD-Name                   */
};
DIR           *opendir  (const char *fspec);   /* Wildcards in DSN und Member */
struct direct *readdir  (DIR *d);
void           rewinddir(DIR *d);
void           closedir (DIR *d);
```

Laut Produktseite werden Wildcards sowohl in Data-Set-Namen als auch in
PDS-Member-Namen unterstützt.

### libc370 heute

`__listds(level, …)`, `__listpd(dataset, filter)` und
`__walkpd(dataset, filter, fn, arg)` leisten das Gleiche mit eigener API. Das
Porting-Kapitel sagt bewusst: „There are no directories and no paths", ein
PDS wird mit `__walkpd()` gelistet.

### Vorschlag

Eine dünne POSIX-Fassade über die vorhandenen Funktionen, für portierten
Code, der `opendir()` erwartet:

```c
#include <dirent.h>

struct dirent {
    char          d_name[57];   /* Member- oder Data-Set-Name   */
    unsigned char d_type;       /* DT_MEMBER, DT_ALIAS, DT_DS   */
};

DIR *d = opendir("'MIKE.SOURCE(ABC*)'");   /* Member eines PDS          */
/* oder: opendir("'MIKE.**'")                   Data Sets aus dem Katalog */
struct dirent *e;
while ((e = readdir(d)) != NULL)
    printf("%s\n", e->d_name);
closedir(d);
```

Mit dem Treiber-Register aus Punkt 2 könnte `opendir("/etc")` später an
UFSD (`ufs_diropen()`) weitergereicht werden, wenn der Treiber dafür einen
Eintrag mitbringt.

### Offen

Grundsatzfrage: Widerspricht das der klaren Linie „keine Verzeichnisse"? Die
Fassade ändert an den Data Sets nichts, sie spart portiertem Code nur eine
Anpassung.

---

## 7 · Positionieren nach Satznummer

### Waterloo

```c
int frseek(FILE *fp, int offset, int whence);   /* offset in Sätzen */
int frtell(FILE *fp);
long rseek(int fd, long offset, int whence);
long rtell(int fd);
```

Dazu die Dateioptionen `DIRECT` (wahlfreier Zugriff auf feste Sätze, jeder
Block ein Satz, aktualisieren ja, neue Sätze nein) und `RAW` (ein `read()`
liefert genau einen Satz). User's Guide S. 150.

### libc370 heute

Intern `NOTE` und `POINT` (`src/stdio/@@anote.asm`). Record-I/O in
`<mvs/rfile.h>`: `ropen()`, `rread()`, `rwrite()`, `rclose()`, ohne Seek.
`fseek()` und `ftell()` funktionieren nach dem Porting-Kapitel anders als auf
einem Bytestrom.

### Vorschlag

Für RECFM=F(B) lässt sich die Satznummer in TTR umrechnen
(Sätze pro Block, Blöcke pro Spur). Für V und U bleibt nur `NOTE`-Token.

```c
#include <mvs/rfile.h>

int  rseek(RFILE *fp, long recno, int whence);   /* nur RECFM=F/FB        */
long rtell(RFILE *fp);
int  rnote(RFILE *fp, unsigned *token);          /* beliebiges RECFM       */
int  rpoint(RFILE *fp, unsigned token);
```

```c
/* Satz 1000 einer FB-80-Datei lesen und überschreiben */
RFILE *f;
ropen("'MIKE.DATA'", 1, &f);
rseek(f, 999, SEEK_SET);
rread(f, rec, &len);
memcpy(rec + 10, "NEU", 3);
rseek(f, 999, SEEK_SET);
rwrite(f, rec, len);
```

---

## 8 · Steuerbare Kommandozeile und Umleitung

### Waterloo

User's Guide §7.2.1 (S. 140–143). Die globale Konstante `_parms` (aus
`setup.h`) bestimmt, wie der PARM bzw. die CPPL zu `argc`/`argv` wird:

| Wert | Wirkung |
|---|---|
| `UNTOKENIZED` | alles in `argv[0]`, `argc == 1` |
| `EXTEND` | Blanks und Klammern trennen, Klammern als eigene Einträge (CMS-Stil) |
| `EXTEND_NOPARENS` | nur Blanks trennen (Standard) |
| `C_STRINGS` | wie `EXTEND`, plus `"…"` mit Blanks, `\n`, `\xhh` und Trigraphs |
| `NO_REDIRECTION` | Maske: keine `<`/`>`-Umleitung |

Umleitung (S. 150–151):

```
PROG Parm <INFILE >OUTFILE (RECFM F LRECL 132
```

`stdin` liest `INFILE`, `stdout` schreibt `OUTFILE` mit den Optionen,
`main()` bekommt `"PROG"` und `"Parm"`.

```c
#include <setup.h>
const int _parms = C_STRINGS;     /* einmal im Programm definieren */
```

### libc370 heute

`src/mvs/crt/@@start.c` trennt an Blanks und fasst `"…"` zusammen
(höchstens `MAXPARMS`). Es gibt keine Umleitung. Es gibt den Hook
`__premain(parm, pgmname, pgmr1)` (`<mvs/crt.h>`): Er läuft vor `main()` und
bevor ein Standard-Stream geöffnet ist, und ein dort gesetztes `stdin` oder
`stdout` wird übernommen.

### Vorschlag

Dasselbe Muster wie Waterloo: eine schwach referenzierte Konstante, die der
Startup liest, ohne dass Programme ohne sie etwas merken.

```c
#include <mvs/crt.h>

#define PARMS_RAW        0x01   /* alles in argv[1]                  */
#define PARMS_WORDS      0x02   /* Blanks, "..." (heutiges Verhalten) */
#define PARMS_CSTRINGS   0x04   /* zusätzlich \n, \xhh, \\           */
#define PARMS_REDIRECT   0x10   /* <dsn, >dsn, >>dsn, 2>dsn          */
#define PARMS_UPPER_OK   0x20   /* TSO CALL hat großgeschrieben: still */

const int __parms = PARMS_CSTRINGS | PARMS_REDIRECT;
```

```
//STEP  EXEC PGM=GREP,PARM='-i "fehler" <DD:SYSUT1 >''MIKE.TREFFER'''
```

Die Umleitung lässt sich schon heute in einem eigenen `__premain()` bauen.
Eine Bibliotheksvariante spart jedem Programm diese Arbeit.

---

## 9 · Packed-Decimal-Hilfen

### Waterloo

`clib.pdeclib.h`, Operanden als Zeiger plus Länge:

```c
char *_pack  (char *dst, size_t dstlen, const char *zoned, size_t srclen);
char *_unpk  (char *dst, size_t dstlen, const char *packed, size_t srclen);
char *_pdadd (char *dst, size_t dstlen, const char *addend,  size_t len);
char *_pdsub (char *dst, size_t dstlen, const char *subtr,   size_t len);
char *_pdmult(char *dst, size_t dstlen, const char *mult,    size_t len);
char *_pddiv (char *dst, size_t dstlen, const char *divisor, size_t len);
int   _pdcmp (const char *a, size_t alen, const char *b, size_t blen);
char *_pdshrd(char *dst, size_t dstlen, int shift, size_t rounddigit);   /* SRP */
char *_itopd (char *dst, int value);                                      /* CVD */
int   _pdtoi (const char *pd);                                            /* CVB */
```

### libc370 heute

Nur ein Assembler-Beispiel in der Doku (`docs/books/ex/pg-asm/pd2int.asm`).
Packed-Daten kommen aber ständig vor: SMF-Datum und -Uhrzeit, JES2-Felder,
Dateien aus COBOL- und PL/I-Programmen. cobc370 und pl1c370 haben vermutlich
eigene Routinen in ihrer Laufzeit (nicht geprüft).

### Vorschlag

`<ext/packed.h>` mit denselben Operationen, die einfachen als Inline-Assembler
(`AP`, `SP`, `MP`, `DP`, `CP`, `ZAP`, `SRP`, `PACK`, `UNPK`, `CVB`, `CVD`) mit
`EX` für die Längen, dazu Formatierung:

```c
#include <ext/packed.h>

int   __pdtoi (const void *pd, size_t len);               /* CVB, mit Längenprüfung  */
void  __itopd (void *pd, size_t len, int value);          /* CVD + ZAP auf len        */
int   __pdadd (void *dst, size_t dlen, const void *src, size_t slen);   /* AP; rc = CC */
int   __pdcmp (const void *a, size_t alen, const void *b, size_t blen);
char *__pdfmt (char *out, const void *pd, size_t len, int scale);       /* "-1234.56"  */
int   __pdvalid(const void *pd, size_t len);              /* Vorzeichen/Ziffern ok?   */
```

```c
/* SMF-Datum 0cyydddF in ein struct tm */
const unsigned char *d = smf->smfdate;
int jjjttt = __pdtoi(d, 4);             /* z. B. 126307 */
tm.tm_year = jjjttt / 1000;             /* 126 → 2026   */
tm.tm_yday = jjjttt % 1000 - 1;
```

`__pdvalid()` vor jeder Rechnung vermeidet S0C7. Ein Data-Check, der doch
durchrutscht, landet mit Punkt 5 in `SIGDEC`.

---

## Kleinigkeiten, billig zu haben

| Funktion | Waterloo | Zweck | Bemerkung |
|---|---|---|---|
| `random()`, `srandom()` | `stdlib.h` | bessere Zufallszahlen als `rand()` | BSD/POSIX, z. B. als Additive-Feedback-Generator |
| `insque()`, `remque()` | `queue.h` | doppelt verkettete Listen | POSIX, 10 Zeilen |
| `lsearch()`, `lfind()` | `search.h` | lineare Suche, optional einfügen | POSIX, Gegenstück zu `bsearch` |
| `swab()` | `stdlib.h` | Bytepaare tauschen | Binärprotokolle |
| `ffs()` | `stdlib.h` | erstes gesetztes Bit | gehört nach `<strings.h>` |
| `unlink()` | `stdio.h` | Alias für `remove()` | Porting |
| `alloca()` | `stdlib.h` | Stack-Allokation | Makro auf `__builtin_alloca`, wenn cc370 das trägt |
| `memusage()` | `stdlib.h` | Heap-Statistik | Diagnose von Lecks in Servern |
| `stakleft()`, `_staksize` | `stdlib.h`, User's Guide §7.2.4 | freier Stack, Stackgröße pro Programm | `const int _staksize = 50*1024;` |
| `memicmp()`, `memupr()`, `memlwr()` | `memory.h` | Puffer ohne Nullterminator | EBCDIC-korrekt über `toupper()` |
| `strupr()`, `strlwr()`, `strrev()` | `string.h` | Klassiker | Namen nach #246 prüfen, `stricmp` wurde in 2.0 bewusst entfernt (#250) |
| `memskip()`, `strskip()`, `strrtrm()` | `string.h` | Zeichen überspringen, rechts trimmen | praktisch für Sätze mit Blank-Füllung |
| `strerrno()` | `string.h` | Fehlertext in eigenen Puffer | reentrante Variante von `strerror` |
| `itos()`, `itohs()` | `stdlib.h` | Zahl → Dezimal-/Hex-String ohne `printf` | klein und schnell |
| `oscall()` | `stdlib.h` | Assembler-Routine mit OS-Linkage aufrufen | `oscall(fn, nparms, ...)` baut die VL-Parameterliste; libc370 hat `__link()` für Ladmodule, aber keinen Aufruf über Zeiger |

```c
/* oscall: eine geladene Assembler-Routine mit Standard-Linkage aufrufen */
int (*rtn)(void) = __load("MYASMRTN");
int rc = __oscall(rtn, 3, &parm1, buffer, &len);   /* R1 → A(parm1),A(buffer),X'80'+A(len) */
```

---

## Full-Screen-Bibliothek

Waterloo hatte eine eigene „Full-screen panel library", BREXX/370
(brexx370) hat FSS. Beide zeigen, was gebraucht wird. Eine **eigene,
einheitliche Bibliothek** (Arbeitsname `fss370`) wäre ein eigenständiges
Projekt neben LUA/370 & Co., keine libc-Funktion.

### Waterloo: `fslib.h`

Drei Objekte, jedes mit einer 8-Byte-Kennung am Anfang („FSDEVICE",
„FSSCR   ", „FSFIELD ") zur Plausibilitätsprüfung:

```c
/* Gerät: das Terminal */
typedef struct {
    char    fsd_stamp[8];         /* "FSDEVICE"                         */
    size_t  fsd_buflen;
    int     fsd_devaddr, fsd_devtype;
    int     fsd_features;         /* FSDF_COLOR, FSDF_EXT_HIGHLIGHTING,
                                     FSDF_FIELD_VALIDATION, FSDF_FIELD_OUTLINING,
                                     FSDF_CHARACTER_SET                  */
    fsscr  *fsd_scrptr;           /* aktuell angezeigter Bildschirm     */
    char    fsd_rows, fsd_cols, fsd_currow, fsd_curcol;
    ...
} fsdevice;

/* Bildschirm: ein logisches Panel, unabhängig vom Gerät */
struct fsscr {
    char     fss_stamp[8];        /* "FSSCR   "                         */
    fsfield *fss_fields;          /* Feldliste                          */
    char     fss_rows, fss_cols, fss_currow, fss_curcol;
    char     fss_changed;
    char     fss_buffer[];        /* Bildschirminhalt                   */
};

/* Feld */
typedef struct fsfield {
    char     fsf_stamp[8];        /* "FSFIELD "                         */
    struct fsfield *fsf_link;
    fsscr   *fsf_scrptr;
    short    fsf_len, fsf_fldidx;
    char     fsf_row, fsf_col;
    char     fsf_flags;           /* FSF_USER_INPUT, FSF_APPL_OUTPUT    */
    char     fsf_attr;            /* 3270-Attribut                      */
    char     fsf_fillchr;
    char     fsf_color, fsf_outlining, fsf_xhigh, fsf_validation;
} fsfield;
```

Funktionen:

```c
fsdevice *fsstart (int devaddr);                         /* Gerät öffnen        */
void      fsend   (fsdevice *dev);

fsscr    *fsscrdefine(size_t rows, size_t cols, size_t currow, size_t curcol);
void      fsscrfree  (fsscr *scr);
fsscr    *fsscrget   (char *fname);                      /* Panel aus Datei laden */
void      fsscrput   (char *fname, fsscr *scr);          /* Panel in Datei sichern */

fsfield  *fsfldadd     (fsscr *scr, size_t row, size_t col, size_t len, int attr, int fill);
fsfield  *fsfldredefine(fsfield *f,  size_t row, size_t col, size_t len, int attr, int fill);
void      fsflddelete  (fsfield *f);
void      fsfldload    (fsfield *f, char *text);         /* Text ins Feld        */
char     *fsfldunload  (fsfield *f);                     /* Text aus dem Feld    */

void      fsscrwrite  (fsdevice *dev, fsscr *scr);       /* Erase/Write          */
void      fsscrrewrite(fsdevice *dev, fsscr *scr);       /* nur Änderungen       */
int       fsscrread   (fsdevice *dev);                   /* Rückgabe: AID (pfkeys.h) */
```

Ergänzend:

- `fattrs.h`: Feldattribute (`FATTR_PROTECTED`, `FATTR_NUMERIC`, `FATTR_INTENSE_DETECT`,
  `FATTR_NDISPLAY_NDETECT`), Feldprüfung (`FVAL_MANDATORY_FILL`, `FVAL_MANDATORY_ENTRY`,
  `FVAL_TRIGGER`), Hervorhebung (`EH_BLINK`, `EH_REVERSE_VIDEO`, `EH_UNDERSCORE`),
  Farben (`COLOR_BLUE` … `COLOR_NEUTRAL`), Umrandung (`OUTL_UNDERLINE`, `OUTL_OVERLINE` …)
- `pfkeys.h`: AID als kleine Zahlen: `PF_ENTER = 0`, `PF1` … `PF24 = 24`,
  `PF_CLEAR`, `PA1` … `PA3`, `PF_WSF`, `PF_TRIGGER`, `PF_OTHER`
- `ds3270.h`: vollständige Konstanten des 3270-Datenstroms: Kommandos (`EW`, `EWA`, `W`, `RM`, `RB`, `WSF`),
  WCC, Orders (`SF`, `SFE`, `SBA`, `SA`, `MF`, `IC`, `PT`, `RA`, `EUA`, `GE`),
  Structured Fields und Query Replies (`QR_USABLE_AREA`, `QR_COLOR`, `QR_HIGHLIGHTING` …)

Merkmale: mehrere Bildschirme gleichzeitig, das Panel ist vom Gerät
getrennt, Felder werden über Zeiger angesprochen, Panels lassen sich als
Datei speichern und laden, und die Fähigkeiten des Terminals werden
abgefragt (`fsd_features`).

### brexx370: FSS

`brexx370/fss/fss.h` (Tommy Sprinkle, 2012, BSD-Lizenz), auf Basis der
TSO-Primitive aus `src/rxtso.c`:

```c
int   fssInit(void);          int   fssTerm(void);
int   fssReset(void);         int   fssStatic(void);
int   fssTxt(int row, int col, int attr, char *text);                       /* Textkonstante */
int   fssFld(int row, int col, int attr, char *name, int len, char *text);  /* benanntes Feld */
int   fssSetField(char *name, char *text);
char *fssGetField(char *name);
int   fssFieldExists(char *name);
int   fssSetAttr (char *name, int attr);
int   fssSetColor(char *name, int color);
int   fssSetXH   (char *name, int xh);
int   fssSetCursor(char *name);   int fssSetCurPos(int pos);   int fssGetCurPos(void);
int   fssShow(int cls);                           /* senden und auf Eingabe warten */
int   fssRefresh(int expires, int cls);           /* mit Zeitlimit                 */
int   fssGetAID(void);
int   fssGetAlternateScreenWidth(void), fssGetAlternateScreenHeight(void);
/* Attribute kombinierbar: fssPROT|fssNUM|fssHI|fssNON | fssRED … | fssBLINK … */
/* AIDs als 3270-Werte: fssENTER = 0x7D, fssPFK01 = 0xF1 …                     */
```

Merkmale: ein globaler Bildschirm, Felder über Namen, Attribut, Farbe und
Hervorhebung in einem `int` kodiert, Zeitlimit beim Warten auf Eingabe,
Hilfen zur Prüfung (`fssIsNumeric`, `fssIsHex`, `fssIsBlank`, `fssTrim`).
Die REXX-Seite (`rxlib/FSSAPI.rexx`, Dashboards) zeigt, dass das Modell in
der Praxis trägt.

### Vergleich

| Aspekt | Waterloo `fslib` | brexx370 FSS |
|---|---|---|
| Bildschirme | mehrere, unabhängig vom Gerät | einer, global |
| Feldzugriff | Zeiger | Name |
| Attribute | getrennte Felder (Attr, Farbe, XH, Umrandung, Prüfung) | ein kodierter `int` |
| Senden | `write` (alles) / `rewrite` (Änderungen) | `show` / `refresh` mit Zeitlimit |
| AID | eigene kleine Zahlen | 3270-Rohwerte |
| Terminal-Fähigkeiten | `fsd_features` | alternative Größe |
| Panels als Datei | `fsscrget` / `fsscrput` | nein |
| Eingabeprüfung | über das Terminal (Field Validation) | Hilfsfunktionen in C |
| Basis | eigene Laufzeit | `rxtso.c` (SVC 93/94) |

### Vorschlag: `fss370`

Drei Schichten:

```
  REKX/370 · BREXX/370 FSS* · LUA/370 · C-Programme
                     │
              fss370 (C-API)          Panels, Felder, Datenstrom, Query
                     │
         libc370 <mvs/tso.h>          __tput, __tget, __stfsmode (Punkt 4)
```

Das Objektmodell von Waterloo (mehrere Panels, Panel ≠ Gerät) und der
Komfort von brexx (Feldnamen, Zeitlimit) lassen sich verbinden:

```c
#include <fss370.h>

typedef struct fss_term   FSS_TERM;     /* das Terminal                */
typedef struct fss_panel  FSS_PANEL;    /* ein logischer Bildschirm    */
typedef struct fss_field  FSS_FIELD;

/* Terminal */
FSS_TERM  *fss_open (void);                      /* STFSMODE ON, Query Reply lesen */
void       fss_close(FSS_TERM *t);
int        fss_rows (FSS_TERM *t), fss_cols(FSS_TERM *t);
unsigned   fss_caps (FSS_TERM *t);               /* FSS_CAP_COLOR | FSS_CAP_XH | …  */

/* Panel */
FSS_PANEL *fss_panel  (int rows, int cols);      /* 0,0 = Terminalgröße            */
void       fss_free   (FSS_PANEL *p);
FSS_PANEL *fss_load   (const char *name);        /* Panel-Definition aus Data Set  */
int        fss_save   (FSS_PANEL *p, const char *name);

/* Felder */
FSS_FIELD *fss_text   (FSS_PANEL *p, int row, int col, const char *text, unsigned attr);
FSS_FIELD *fss_input  (FSS_PANEL *p, const char *name, int row, int col, int len, unsigned attr);
FSS_FIELD *fss_find   (FSS_PANEL *p, const char *name);
int        fss_set    (FSS_FIELD *f, const char *text);
const char*fss_get    (FSS_FIELD *f);            /* ohne Füllzeichen               */
int        fss_attr   (FSS_FIELD *f, unsigned attr);
int        fss_changed(FSS_FIELD *f);            /* MDT gesetzt?                   */
void       fss_cursor (FSS_PANEL *p, FSS_FIELD *f, int offset);

/* Anzeige und Eingabe */
int        fss_show   (FSS_TERM *t, FSS_PANEL *p, int timeout_ms);   /* Rückgabe: AID   */
int        fss_update (FSS_TERM *t, FSS_PANEL *p, int timeout_ms);   /* nur Änderungen  */

/* Attribute: kombinierbar, aber in getrennten Bitfeldern */
#define FSS_PROT    0x0001
#define FSS_NUM     0x0002
#define FSS_HI      0x0004
#define FSS_HIDDEN  0x0008
#define FSS_BLUE    0x0100          /* … FSS_WHITE: Bits 8–11  */
#define FSS_BLINK   0x1000          /* … Hervorhebung: Bits 12–13 */
#define FSS_MUSTFILL 0x10000        /* Field Validation, falls das Terminal es kann */

/* AIDs: kleine Zahlen wie bei Waterloo */
enum { FSS_ENTER, FSS_PF1, /* … */ FSS_PF24 = 24, FSS_CLEAR, FSS_PA1, FSS_PA2, FSS_PA3, FSS_TIMEOUT };
```

```c
FSS_TERM  *t = fss_open();
FSS_PANEL *p = fss_panel(0, 0);

fss_text (p, 1, 2, "Datenset-Info", FSS_HI | FSS_TURQ);
fss_text (p, 3, 2, "DSN ===>", FSS_PROT);
FSS_FIELD *dsn = fss_input(p, "DSN", 3, 11, 44, FSS_GREEN);
FSS_FIELD *msg = fss_text (p, fss_rows(t), 2, "", FSS_PROT | FSS_RED);
fss_cursor(p, dsn, 0);

for (;;) {
    int aid = fss_show(t, p, 0);
    if (aid == FSS_PF3) break;
    DSSTAT st;                                   /* aus Punkt 1 */
    if (__dsstat(fss_get(dsn), &st) == 0)
        fss_set(msg, "gefunden");
    else
        fss_set(msg, "nicht katalogisiert");
}
fss_close(t);
```

Für BREXX/370 bleibt die REXX-API (`FSSINIT`, `FSSFIELD`, `FSSDISPLAY` …) wie
sie ist, nur die C-Schicht darunter würde ersetzt, und das erst nach 3.0.0.
Der natürliche erste REXX-Nutzer ist REKX/370, der neue Sprachfunktionen
ohnehin aufnimmt.

### Offen

- Panel-Definitionen als Datei: ein einfaches Textformat (ähnlich ISPF-Panels,
  aber kleiner) oder die Binärform wie bei Waterloo?
- Query Reply: Terminals ohne Antwort auf `Read Partition Query` (manche
  Emulatoren, ältere 3277) brauchen sinnvolle Vorgaben (24×80, keine Farbe).
- Lizenz: FSS steht unter BSD, Teile davon dürfen übernommen werden (mit
  Copyright-Vermerk). Von Waterloo nur die Ideen.
- Ein Panel-Editor (selbst ein FSS-Programm) wäre der natürliche erste Nutzer.

---

## Zeichensatz: `(:` und `:)` statt `[` und `]`

### Befund

Die Waterloo-Header schreiben Arrays durchgehend so:

```c
char d_name(:MAXFULLEN+1:);
extern const char atoetbl(::), etoatbl(::);
```

User's Guide §6.3.3 „Special Characters" (S. 99) erklärt das: Manche
Terminals (IBM 3278, 3279) haben die Zeichen nicht, oder sie liegen in der
Code Page an anderer Stelle. Waterloo C bot deshalb Ersatzschreibweisen an:

| Zeichen | ANSI-Trigraph | Waterloo-Ersatz |
|---|---|---|
| `[` `]` | `??(` `??)` | `(:` `:)` |
| `^` (exklusives Oder) | `??'` | `¬` oder `¢` |
| `\|` (inklusives Oder) | `??!` | durchgehender oder unterbrochener Strich `¦` |
| `{` `}` | `??<` `??>` | – |
| `#` | `??=` | – |

C95 hat später für dasselbe Problem die Digraphs eingeführt: `<:` `:>` für
`[` `]`, `<%` `%>` für `{` `}`, `%:` für `#`. Waterloos `(:` ist der
Vorläufer davon.

### Bezug zu uns

- Das Zeichensatz-Kapitel der libc370 (`pg-charset.typ`) beschreibt dasselbe
  Problem: CP037 und CP1047 legen `[` und `]` an verschiedene Stellen, die
  Klassifikationstabelle folgt 1047.
- Wer C-Quelltext auf einem 3270 mit CP037 bearbeitet, sieht die Klammern
  falsch oder gar nicht. Abhilfen in Standard-C: Trigraphs (`??(`) oder Digraphs
  (`<:`). Beide sollte cc370 schon heute verstehen. Das wäre zu prüfen und im
  Kapitel „Character Sets" zu erwähnen.
- Für einen künftigen nativen Compiler: `(:` `:)` als Kompatibilitätsoption
  zu unterstützen ist billig. Außerhalb von Literalen und Kommentaren kann
  `(` direkt gefolgt von `:` in gültigem C nicht vorkommen. Der Lexer muss
  aber `:)` in einem Ausdruck wie `x ? a :)` korrekt als Fehler erkennen,
  nicht als `]`.

### Weitere Compiler-Notizen aus dem User's Guide

Festgehalten für einen eigenen Compiler (cross und nativ):

- **Typen** (§6.3.1): `char` ist **unsigned** (0..255), `int` und `long` 32 Bit,
  `float` und `double` in HFP (Basis 16, Exponent -64..+63).
- **Externe Namen** (§6.3.4): die ersten 8 Zeichen, großgeschrieben, `_` → `$`.
  Externe Namen müssen in den ersten 8 Zeichen eindeutig sein.
- **Variable Argumente**: höchstens 31 unspezifizierte Parameter.
- **Ein Durchlauf, Objektcode direkt**, Assembler optional (`ASM`, `ASMSRC` mit
  C-Quelle als Kommentar).
- **`ASCIIOUT`**: String- und Zeichenkonstanten werden als ASCII erzeugt. Für
  cc370 wäre eine feinere Variante interessant, etwa ein Präfix oder Attribut
  für einzelne Literale (`__ascii("GET / HTTP/1.0\r\n")`), das die
  Übersetzung aus Punkt 3 für Konstanten überflüssig macht.
- **Code- und Datentrennung**: `SPLIT` bzw. `RENT` schreibt Code und
  schreibgeschützte Daten nach `OBJ1`, schreibbare Daten nach `OBJ2`. Mit
  `AUX` adressiert ein globales Basisregister die Read/Write-Daten.
- **Linkage-Varianten**: Standard (eigener Stack), `OS` (OS-Linkage mit
  reserviertem Stack-Register), `OSENTRY` (OS-Linkage ohne Stack), `OSFUNC`
  (OS-Linkage, Stack über einen Aufruf im Prolog).
- **Stack**: Standard 10K, pro Programm über `_staksize` einstellbar,
  `stakleft()` liefert den freien Rest, Option `STACKCHK` prüft auf Überlauf.
- **Initialisierung** (§7.2.2): Stack und Read/Write-Bereiche holen, optional
  eine vorgelinkte Bibliothek aus LPA, JOBLIB oder STEPLIB laden, Signal-Handler
  und Program-Check-Behandlung einrichten, die Treiber `_NULL` und
  `_TERMINAL` registrieren, Parameter zerlegen, Standard-Streams öffnen
  (mit Umleitung), `main()` aufrufen.
- **Bibliotheksvarianten** (§7.1.6): fünf Objektbibliotheken (`OBJ`, `OBJRENT`,
  `OBJALIGN`, `OBJVEC`, `OBJVECXA`), dazu eine vorgelinkte Bibliothek für die
  LPA. Jede Bibliotheksfunktion in einer eigenen CSECT.
