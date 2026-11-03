# Dignus Systems/C — Ideen für LIBC/370 und CC/370

Stand: 2026-11-03 · Status: Arbeitspapier, nichts davon ist beschlossen ·
Gegenstück zu [`watc-ideen.md`](watc-ideen.md)

## Worum es geht

Dignus Systems/C ist ein kommerzieller C-Compiler (Dignus LLC) für OS/390,
z/OS, z/TPF, CMS und **MVS 3.8**, mit einer Laufzeitbibliothek, die zum
großen Teil auf FreeBSD aufbaut. Dieses Papier prüft wie das Waterloo-Papier,
welche Schnittstellen der Dignus-Bibliothek für LIBC/370 (libc370) lohnen, und
hält Compiler-Ideen für CC/370 (cc370) fest.

**Lizenz:** Systems/C ist lizenzpflichtig („The use of this product requires a
valid License Agreement", `COPYRIGHT.C`). Grundlage hier sind ausschließlich
die Handbücher und Header. Übernommen werden Ideen und Schnittstellenformen,
kein Code. Wo Dignus selbst BSD-Code verwendet (Zeichenketten, `getopt`,
`sys/queue.h` …), ist der Weg für uns das **FreeBSD-Original** unter
BSD-Lizenz, nicht die Dignus-Fassung. LIBC/370 steht unter einer BSD-artigen
Lizenz und enthält schon BSD-Code (`src/net/@@inaton.c`, `@@inntoa.c`).

### Quellen

| Quelle | Ort | Umfang |
|---|---|---|
| C Library Reference (`clib.pdf`) | `~/repos/mvs/dignus/dignus/doc/` | rund 1.000 Seiten, mit Textebene |
| C Compiler (`sysc.pdf`) | dito | Optionen, Erweiterungen, Builtins |
| Utilities (`util.pdf`) | dito | PLINK, DPDSLIB, GOFF2XSD |
| Header | `~/repos/mvs/dignus/dignus/include/` | FreeBSD-Struktur plus `machine/…` für MVS |

### Was Dignus unter MVS 3.8 kann

Dignus liefert eigene MVS-3.8-Objekte, die beim Linken vor die normale
Bibliothek gestellt werden (`MVS38_objs_rent`, `LIBCR38`). Laut Handbuch
(„MVS 3.8 runtime restrictions") läuft dann „the entire Systems/C run-time
library", mit diesen Ausnahmen:

- nur 24-Bit-Adressen
- keine dynamische Allokation über `O_CREAT` in `open()`
- kein TCP/IP (MVS 3.8 hat keines)
- keine BPX-Dienste, also keine POSIX-Funktionen (die liefern einen Fehler)

Lange externe Namen löst Dignus mit seinem Prelinker `PLINK -px`, der sie für
den MVS-3.8-Linker auf kurze Namen abbildet (siehe Compiler-Notizen).

Für den Vergleich zählt also der Teil, der ohne USS auskommt. pthreads,
`fork`/`exec`, `mmap`, System-V-IPC, HFS-Dateien, CEEPIPI und VSAMIO (nur z/OS,
AMODE 31) fallen weg.

### Übersicht

| # | Vorschlag | Dignus | libc370 heute | Bezug Waterloo | Nutzen |
|---|---|---|---|---|---|
| 1 | `funopen()` und Speicher-Streams | `funopen`, `fropen`, `fwopen` | fehlen | konkretisiert W2 | hoch |
| 2 | Dateiinfo zu offenem Stream und DD | `fldata`, `osddinfo`, `ddnfind` | Bausteine (`__locate`, JFCB) | ergänzt W1 | hoch |
| 3 | BSD/POSIX-Helfer | `getopt`, `getline`, `strlcpy`, `sys/queue.h` … | fehlen | ergänzt W-Kleinigkeiten | hoch |
| 4 | Startup über globale Variablen | `__fd1nm`, `__fd1atr`, `__argvc`, `__runopt` | `__premain()`, SYSENV | ergänzt W8 | mittel |
| 5 | Abend als Signal, Trap abschaltbar | `SIGABND`, `__abendcode()`, `TRAP(ON/OFF)` | ESTAE, `try()` | ergänzt W5 | mittel |
| 6 | Traceback mit Callback | `__traceback(FILE*)`, `__tbfrom()` | `wto_traceback()` | – | mittel |
| 7 | Job-, Step-, Benutzername | `__stepname`, `__procname`, `__userid`, `getprogname` | `__jobname`, `__jobid` | – | gering, billig |
| 8 | Übersetzung: Kopieren beim Übersetzen, Code Page wählbar | `__bcopy_etoa`, Tabelle 1047 | – | ergänzt W3 | mittel |
| 9 | Regeln für fehlende DCB-Attribute | `open()`-Attribute, `preopen` | Modus-Optionen | – | prüfen |
| 10 | Koroutinen | `getcontext`, `makecontext`, `swapcontext` | fehlen | – | optional |
| – | Compiler-Notizen | Optionen, Pragmas, PLINK | – | ergänzt W-Compiler | – |

Vorschlag für die Reihenfolge: **1** (macht Waterloo-Punkt 2 zu einer
Standard-API), **3** (billig, hilft jedem Port), **2**, dann **5** und **6**
zusammen.

---

## 1 · `funopen()` und Speicher-Streams

### Dignus

`funopen()` stammt aus 4.4BSD und verbindet einen `FILE` mit eigenen
Funktionen:

```c
FILE *funopen(const void *cookie,
              int    (*readfn) (void *cookie, char *buf, int n),
              int    (*writefn)(void *cookie, const char *buf, int n),
              fpos_t (*seekfn) (void *cookie, fpos_t off, int whence),
              int    (*closefn)(void *cookie));
FILE *fropen(void *cookie, int (*readfn)(void *, char *, int));          /* nur lesen   */
FILE *fwopen(void *cookie, int (*writefn)(void *, const char *, int));   /* nur schreiben */
```

Fehlt eine Funktion, schlägt die zugehörige Operation fehl. Ohne `closefn`
wird beim Schließen nur gepuffert Geschriebenes ausgegeben. Die Pufferung
übernimmt stdio.

### libc370 heute

Nichts Vergleichbares. Das Waterloo-Papier (Punkt 2) schlägt eine
Treiber-Tabelle mit Präfixen vor.

### Vorschlag

`funopen()` als **Basis** einbauen, weil es eine bekannte Schnittstelle ist
(BSD, macOS; glibc hat das ähnliche `fopencookie()`). Das Präfix-Register aus
Waterloo-Punkt 2 wird dann eine dünne Schicht darüber: Ein registrierter
Treiber ist eine Funktion, die zu einem Namen ein `funopen()`-Quartett liefert.

```c
#include <stdio.h>

/* Zähler-Stream: zählt Bytes, schreibt nichts */
static int count_write(void *c, const char *buf, int n)
{
    *(long *)c += n;
    return n;
}

long bytes = 0;
FILE *fp = fwopen(&bytes, count_write);
fprintf(fp, "%s: %d\n", name, value);
fclose(fp);                          /* bytes enthält die Länge */
```

Damit ergeben sich die POSIX-2008-Speicher-Streams fast von selbst:

```c
FILE *fmemopen(void *buf, size_t size, const char *mode);    /* fester Puffer       */
FILE *open_memstream(char **bufp, size_t *sizep);            /* wachsender Puffer   */
```

```c
char  *text;  size_t len;
FILE  *m = open_memstream(&text, &len);
fprintf(m, "HTTP/1.0 %d %s\r\n", code, reason);
fprintf(m, "Content-Length: %zu\r\n\r\n", body_len);
fclose(m);                          /* text, len sind gesetzt */
send(sock, text, len, 0);
free(text);
```

Für UFSD (`libufs`) wäre `funopen()` der Weg, `ufs_fopen()` hinter einem
normalen `FILE *` zu verstecken, auch ohne Präfix-Register:

```c
FILE *ufs_stdio(UFS *ufs, const char *path, const char *mode)
{
    UFSFILE *uf = ufs_fopen(ufs, path, mode);
    return uf ? funopen(uf, uread, uwrite, useek, uclose) : NULL;
}
```

### Offen

- Wie verträgt sich `funopen()` mit der heutigen `FILE`-Struktur, die eng an
  DCB und Record-I/O gebunden ist? Wahrscheinlich als eigene Stream-Art mit
  einem Funktionszeiger-Block.
- Text/Binär: Ein Funktions-Stream ist immer ein Bytestrom. `\n` bleibt `\n`
  (X'15'), es gibt keine Satzgrenzen.

---

## 2 · Dateiinfo zu offenem Stream und DD

### Dignus

Drei Funktionen in `<machine/syscio.h>`:

```c
/* zu einem offenen Deskriptor: ursprünglicher Name und Attribute */
int fldata(int fd, char *buf, int bufsize, fldata_t *info);

typedef struct __fileData {
    unsigned __recfmF:1, __recfmV:1, __recfmU:1, __recfmS:1,
             __recfmBlk:1, __recfmASA:1, __recfmM:1;
    unsigned __dsorgPO:1, __dsorgPDSmem:1, __dsorgPDSdir:1,
             __dsorgPS:1, __dsorgConcat:1, __dsorgMem:1, …;
    /* dazu u. a. LRECL, BLKSIZE, Data-Set-Name */
} fldata_t;

/* zu einem DD-Namen: DSN, Member, RECFM, LRECL, BLKSIZE */
int osddinfo(char *ddname, char dsname[45], char member[9],
             char *recfm_p, int *lrecl_p, int *blksize_p);

/* alle Data Sets einer Verkettung */
void *ddnfind(char *ddn, char *dsn);
void *ddnnext(void *token, char *dsn);     /* NULL am Ende, gibt token frei */
```

`osddinfo()` holt nur, was angefragt wird: Ein `NULL`-Zeiger spart den
zugehörigen Systemaufruf. `ddnfind()` liest die JFCBs der DD-Verkettung.

### libc370 heute

`__locate()`, `__dscbdv()`, JFCB-Strukturen (`ibm/mvs/iefjfcbn.h`). Kein
Aufruf für „Welche Attribute hat dieser offene Stream?" und keiner für
„Welche Data Sets stehen hinter `STEPLIB`?".

### Vorschlag

Zusammen mit `__dsstat()` aus dem Waterloo-Papier eine kleine Familie, alle mit
derselben `DSSTAT`-Struktur:

```c
#include <mvs/dsstat.h>

int   __dsstat (const char *name, DSSTAT *st);          /* W1: Name → Attribute        */
int   __fdsstat(FILE *fp, DSSTAT *st);                  /* wie fldata(): offener Stream */
int   __ddstat (const char *ddname, DSSTAT *st);        /* wie osddinfo(): DD → erstes DS */
void *__ddfirst(const char *ddname, DSSTAT *st);        /* wie ddnfind(): Verkettung    */
void *__ddnext (void *token, DSSTAT *st);
```

```c
/* Welche Bibliotheken stehen in STEPLIB? */
DSSTAT st;
for (void *t = __ddfirst("STEPLIB", &st); t; t = __ddnext(t, &st))
    printf("%-44s %s\n", st.dsn, st.volser);
```

Die Felder, die nur mit zusätzlichem I/O zu haben sind (Spuren, Änderungsdatum),
sollten wie bei `osddinfo()` optional sein, z. B. über ein Flag-Argument.

---

## 3 · BSD/POSIX-Helfer

### Befund

LIBC/370 fehlen viele kleine, verbreitete Funktionen, die Dignus aus FreeBSD
mitbringt. Gegengeprüft gegen `include/` und `src/` von libc370 (2026-11-03):

| Gruppe | Funktionen | Nutzen | Aufwand |
|---|---|---|---|
| Zeichenketten | `strnlen`, `strndup`, `stpcpy`, `strlcpy`, `strlcat`, `strsep`, `strtok_r`, `memmem` | sicheres Kopieren, Zerlegen ohne globale Zustände | klein |
| Formatierte Ausgabe | `asprintf`, `vasprintf` | Speicher passend zum Ergebnis | klein (über `vsnprintf`) |
| Zeilen lesen | `getline`, `getdelim`, `fgetln` | beliebig lange Zeilen ohne festen Puffer | klein |
| Kommandozeile | `getopt`, `getopt_long`, `getsubopt` | Standardzerlegung von Optionen | klein |
| Fehlerausgabe | `err`, `errx`, `warn`, `warnx`, `getprogname`, `setprogname` | einheitliche Meldungen | klein |
| Datenstrukturen | `<sys/queue.h>` (`LIST`, `SLIST`, `STAILQ`, `TAILQ`), `<bitstring.h>` | Listen ohne eigenen Code, nur Makros | sehr klein |
| Suchen | `tsearch`/`tfind`/`tdelete`/`twalk`, `hcreate`/`hsearch`/`hdestroy`, `lsearch`/`lfind` | Bäume und Hashes aus der Bibliothek | klein–mittel |
| Sortieren | `mergesort`, `heapsort`, `radixsort` | stabil sortieren, ohne Rekursionstiefe | klein |
| Zufall | `random`/`srandom`, `arc4random` | bessere Zufallszahlen | klein |
| Zeit | `strptime` | Gegenstück zu `strftime` | mittel |
| Zahlen | `strtonum`, `reallocf` | Bereichsprüfung, `realloc` ohne Leck | sehr klein |
| Muster | `fnmatch` | Shell-Muster | klein; libc370 hat `__patmat()` |
| Reguläre Ausdrücke | `regcomp`, `regexec`, `regfree`, `regerror` | | groß; Speicherbedarf beachten |

Hinweise:

- Alles ist C99-tauglich und braucht nichts vom Betriebssystem.
- Rekursion: `tsearch` und `twalk` sind rekursiv über die Baumtiefe. Bei
  balancierten Bäumen ist das logarithmisch, sollte aber dokumentiert werden.
- `getopt` muss mit der Zerlegung der Kommandozeile zusammenpassen (Punkt 4,
  Waterloo 8): Unter TSO kommen die Argumente oft großgeschrieben an.
- `regex`: Die FreeBSD-Implementierung (Henry Spencer) ist gut, braucht aber
  viel Speicher pro Ausdruck. Eher als eigene kleine Bibliothek oder später.
- Namensregel #246 beachten: Diese Funktionen gehören unter ihrem
  POSIX/BSD-Namen in die Standard-Header, nicht mit `__`-Präfix.

```c
#include <unistd.h>
#include <err.h>

int main(int argc, char **argv)
{
    int c, verbose = 0;
    const char *out = "DD:SYSPRINT";
    while ((c = getopt(argc, argv, "vo:")) != -1)
        switch (c) {
        case 'v': verbose = 1; break;
        case 'o': out = optarg; break;
        default:  errx(8, "usage: %s [-v] [-o out] file", getprogname());
        }
    FILE *in = fopen(argv[optind], "r");
    if (!in) err(8, "%s", argv[optind]);

    char *line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, in)) > 0)
        ...
    free(line);
}
```

```c
#include <sys/queue.h>

struct job { char name[9]; TAILQ_ENTRY(job) link; };
TAILQ_HEAD(joblist, job) jobs = TAILQ_HEAD_INITIALIZER(jobs);

TAILQ_INSERT_TAIL(&jobs, j, link);
TAILQ_FOREACH(j, &jobs, link)
    printf("%s\n", j->name);
```

---

## 4 · Startup über globale Variablen

### Dignus

Vieles am Programmstart steuert eine globale Variable, die das Programm
definiert und die Laufzeit liest (Handbuch „Controlling the runtime
environment"):

| Variable | Wirkung |
|---|---|
| `char *__fd0nm`, `__fd1nm`, `__fd2nm` | Name für stdin, stdout, stderr (Vorgabe `//DDN:STDIN` usw.) |
| `char *__fd0atr`, `__fd1atr`, `__fd2atr` | DCB-Attribute dafür, z. B. `"recfm=fb,lrecl=80,blksize=800"` |
| `char __argvc` | Trennzeichen für argv (Vorgabe Komma; Blank = mehrere Blanks zählen als eines) |
| `int __runopt` | Laufzeitoptionen im PARM auswerten (Vorgabe 0) |
| `char __rochar` | Trenner zwischen Laufzeitoptionen und Argumenten (Vorgabe `\`) |

Laufzeitoptionen im PARM vor dem Trenner: `ENVAR(name=wert)` setzt eine
Umgebungsvariable, `TRAP(ON|OFF)` schaltet die Program-Check-Behandlung.

```
//STEP EXEC PGM=MYPGM,PARM='ENVAR(TZ=CET-1) TRAP(ON)\eingabe ausgabe'
```

Dazu pro Funktion über `#pragma prolkey`: `ISTK=n` (Anfangsgröße des Stacks),
`ESTK=n` (Mindestgröße einer Stack-Erweiterung), `SP=n` (Subpool), `KEY=n`
(Speicherschlüssel), `TRAP=ON`, `DCALL=YES` (eigene C-Umgebung beim Aufruf aus
Assembler).

### libc370 heute

- `__premain(parm, pgmname, pgmr1)`: eigener Code vor `main()`, kann Streams setzen.
- Standard-Streams öffnen beim ersten Gebrauch (#564).
- Umgebungsvariablen aus dem `SYSENV`-DD.
- `CCALL`/`@@CCALL`: C aus Assembler oder COBOL aufrufen, baut bei Bedarf eine C-Umgebung (#502, #507).
- argv: Blanks trennen, `"…"` fasst zusammen.

### Vorschlag

Das Muster „schwach referenzierte globale Variable mit Vorgabe" passt gut zu
`__premain()`: Für die häufigen Fälle braucht man dann keine eigene Funktion.
Zusammen mit Waterloo-Punkt 8 (`__parms`):

```c
#include <mvs/crt.h>

const char *__stdout_name = "DD:REPORT";                 /* statt SYSPRINT       */
const char *__stdout_mode = "w,recfm=fba,lrecl=133";
const int   __parms       = PARMS_CSTRINGS | PARMS_REDIRECT;
const int   __runopts     = 1;      /* ENVAR(...) und TRAP(...) vor dem Trenner */
const char  __runopts_sep = '\\';
```

Ein `ENVAR()` im PARM ergänzt `SYSENV` sinnvoll: Eine einzelne Einstellung für
einen Lauf, ohne einen DD anzulegen.

### Offen

- Stackgröße pro Programm ist schon Thema im Waterloo-Papier (`_staksize`). Ob
  zusätzlich `ESTK` pro Funktion nötig ist, hängt davon ab, wie LIBC/370 den
  Stack heute erweitert (`pg-startup-stack`).

---

## 5 · Abend als Signal, Trap abschaltbar

### Dignus

- Die Program-Check-Behandlung per ESTAE ist **wählbar**: `TRAP=ON` im Prolog
  von `main()` oder `TRAP(ON)` im PARM. Vorgabe ist OFF.
- Mit ESTAE wird ein ABEND zum Signal `SIGABND`. Der Handler fragt
  `__abendcode()` und `__rsncode()` ab. Kehrt er zurück, wird der ABEND erneut
  ausgelöst. Wer weitermachen will, springt mit `longjmp` heraus.
- Das Handbuch zeigt als Beispiel, wie man ABEND 978 (Stack erschöpft) abfängt.

### libc370 heute

ESTAE und `try()` in `<mvs/recovery.h>`, `__abrpt()`. Die sechs ANSI-Signale.

### Vorschlag

Zu den feineren Program-Check-Signalen aus Waterloo-Punkt 5:

```c
#define SIGABND  12

unsigned __abendcode(void);    /* 0x0C4000 für S0C4, 0x00000C für U0012 … */
unsigned __rsncode(void);
```

```c
static jmp_buf again;

static void on_abend(int sig)
{
    fprintf(stderr, "Abend %03X, Reason %X\n", __abendcode() >> 12, __rsncode());
    longjmp(again, 1);
}

signal(SIGABND, on_abend);
if (setjmp(again) == 0)
    run_request();         /* ein fehlerhafter Request beendet nicht den Server */
```

Ob man das über die Signal-Schnittstelle anbietet oder nur über `try()`, ist
eine Designfrage. Die Signal-Form hilft portiertem Code und Interpretern.

---

## 6 · Traceback mit Callback

### Dignus

```c
void __traceback(FILE *f);                         /* Aufrufkette auf einen Stream */
void __tbfrom(void *stack_ptr,                     /* Start: meist R13             */
              void (*message)(char *line, void *user),
              void *user);
```

Die Namen stehen in einem Bereich vor dem Prolog jeder Funktion.

### libc370 heute

`wto_traceback(SAVEAREA *sa)` schreibt auf die Konsole. `__linkt()` nutzt
den Traceback bei einem Abend. `s370/savearea.h` formatiert ohne `sprintf`
(#564).

### Vorschlag

Die Ausgabe vom Ziel trennen:

```c
void __traceback(FILE *fp);                                       /* auf einen Stream    */
void __tbwalk(const SAVEAREA *from, void (*fn)(const char *line, void *u), void *u);
```

```c
/* Traceback ins Fehlerprotokoll des Servers statt auf die Konsole */
static void to_log(const char *line, void *u) { log_write((LOG *)u, line); }

__tbwalk(__savearea(), to_log, server_log);
```

`wto_traceback()` wird dann eine von mehreren Senken.

---

## 7 · Job-, Step- und Benutzername

### Dignus

`__jobname()`, `__jobname_r(buf)`, `__stepname()`, `__procname()` (leer ohne
Prozedur), `__userid()`, dazu `getprogname()`/`setprogname()`. Alle ohne
nachfolgende Blanks.

### libc370 heute

`__jobname()` und `__jobid()`.

### Vorschlag

Ergänzen um `__stepname()`, `__procname()` (aus TIOT bzw. JSCB), `__userid()`
(TSO: PSCB, Batch: ACEE bzw. JCT) und `getprogname()` (aus dem `pgmname`, das
der Startup schon hat, siehe `__premain()`). `getprogname()` braucht die
Fehlerausgabe aus Punkt 3.

---

## 8 · Übersetzung: Kopieren beim Übersetzen, Code Page wählbar

### Dignus

```c
unsigned __atoe(unsigned c);                          /* ein Zeichen               */
unsigned __etoa(unsigned c);
void __stratoe (unsigned char *s);                     /* nullterminiert, in place  */
void __strnatoe(unsigned char *s, int len);            /* höchstens len             */
void __bcopy_atoe(unsigned char *src, unsigned char *dst, int len);   /* kopieren + übersetzen */
void __bcopy_etoa(unsigned char *src, unsigned char *dst, int len);
```

Die Tabelle ist **IBM-1047**, dieselbe wie im Dignus-Compiler.

### Bezug zu LIBC/370

- LIBC/370 und CC/370 arbeiten mit **CP037**. Code, der zwischen Dignus und
  LIBC/370 portiert wird, übersetzt bei `[ ] ^ ¬ |` unterschiedlich.
- Die Variante „kopieren und übersetzen" (`__bcopy_etoa`) fehlt im
  Waterloo-Vorschlag. Sie ist der häufigere Fall im Netzwerkcode: Der
  Quellpuffer bleibt unverändert.

### Vorschlag

Waterloo-Punkt 3 um eine Kopiervariante und eine wählbare Tabelle ergänzen:

```c
void *__memxlt (void *buf, size_t n, const unsigned char tab[256]);              /* in place */
void *__memxltc(void *dst, const void *src, size_t n, const unsigned char tab[256]); /* kopieren */

extern const unsigned char __cp037_to_latin1[256], __latin1_to_cp037[256];
extern const unsigned char __cp1047_to_latin1[256], __latin1_to_cp1047[256];
```

`__memxltc` ist ein `MVC` plus `TR` je 256 Bytes.

---

## 9 · Regeln für fehlende DCB-Attribute

### Dignus

`open()` nimmt eine Attributzeichenkette (`recfm=`, `lrecl=`, `blksize=`,
`primary=`, `secondary=`, `blks`/`trks`/`cyls`, `rlse`, `unit=`, `vol=`,
`type=record` …) und hat klare Regeln, wenn Angaben fehlen (Handbuch
`OPEN(2)`, „Creating //DSN: files"):

- Ausgabe ohne RECFM: Terminal → `U`, sonst `FBA`; binär → `FB`.
- RECFM=FB mit LRECL ohne BLKSIZE → größtes Vielfaches von LRECL, das auf das Gerät passt.
- RECFM=F mit nur BLKSIZE → LRECL = BLKSIZE.
- Konflikt zwischen JCL und Programm (JCL LRECL=133, Programm BLKSIZE=8000 für FB80):
  das Attribut `preopen` lässt die Programmwerte vor dem OPEN gewinnen.

### libc370 heute

Modus-Optionen `"w,recfm=fb,lrecl=80"`, für `DD:`-Namen gelten `recfm=`,
`lrecl=`, `blksize=` (`std-stdio.typ`). Die Ableitungsregeln habe ich nicht im
Detail verglichen.

### Vorschlag

Prüfen, ob die Ableitung bei LIBC/370 genauso vollständig und dokumentiert ist,
insbesondere der Konfliktfall JCL gegen Programm. Gegebenenfalls ein Kapitel
oder eine Tabelle in der Referenz.

---

## 10 · Koroutinen

### Dignus

`getcontext`, `setcontext`, `makecontext`, `swapcontext` (`<ucontext.h>`):
mehrere Ausführungsstränge in einem Task, jeder mit eigenem Stack, die sich
gegenseitig die Kontrolle übergeben.

### Bewertung

Interessant für Server, die viele Verbindungen halten, ohne für jede einen
Subtask anzulegen, und für Generatoren in Interpretern. Bei LIBC/370 hängt
der Stack an der Kette der Save Areas mit NAB. Jeder Kontext bräuchte ein
eigenes Stück davon. Bei knappem Speicher unter 16 MB ist das teuer.
**Optional**, erst wenn ein konkretes Projekt es braucht.

---

## Nicht übernehmen

| Bereich | Grund |
|---|---|
| pthreads, `thrd_create` | POSIX; LIBC/370 hat eigene Threads über Subtasks (`mvs/thread.h`) |
| `fork`, `exec*`, `posix_spawn`, `popen`, Pipes | USS; Regel „No POSIX" |
| `mmap`, System-V-IPC (`msg*`, `sem*`, `shm*`) | USS |
| HFS-Funktionen (`opendir` auf HFS, `chmod`, `symlink` …) | USS; für hierarchische Dateien gibt es UFSD |
| VSAMIO (`kopen` …) | bei Dignus nur z/OS und AMODE 31; LIBC/370 hat `mvs/vsam.h` |
| CEEPIPI | Language Environment, gibt es auf MVS 3.8 nicht |
| `dlopen` | braucht Dignus-eigene Shared Objects; LIBC/370 hat `__load()`/`__link()` |
| `__malloc24`/`__malloc31` | unter MVS 3.8 ist alles unterhalb 16 MB |
| `_Decimal32/64/128`, `__to_b*` | Decimal Floating Point ab z9 |
| Locale und Wide Characters über das Vorhandene hinaus | großer Umfang, kein Bedarf erkennbar |

---

## Compiler-Notizen für CC/370 und einen eigenen Compiler

Aus dem Compiler-Handbuch (`sysc.pdf`), ergänzend zu den Waterloo-Notizen:

- **Zeichensatz der Quelle:** Dignus erwartet ein „modifiziertes IBM-1047",
  erkennt aber die Zeichen von **IBM-037** ebenfalls, sodass beide Kodierungen
  gehen. `-fcodepage500` für IBM-500, `#pragma filetag` ändert die Annahme pro
  Datei. Das ist die moderne Antwort auf das Klammerproblem, das Waterloo mit
  `(:` `:)` löste. Für einen nativen Compiler: beide Klammerkodierungen als
  `[` `]` akzeptieren.
- **`char` ist unsigned**, wie bei Waterloo.
- **`-fasciiout`:** Zeichen- und Stringkonstanten als ASCII, das Makro
  `__SYSC_ASCIIOUT` wird gesetzt. Gilt nicht für Präprozessor- und
  Pragma-Konstanten. Die Bibliothek selbst ist dann nicht ASCII-fähig. Ein
  feinerer Mechanismus pro Literal wäre nützlicher (siehe Waterloo-Notizen).
- **Lange externe Namen:** Die Bibliothek liegt im erweiterten Objektformat
  (XSD) vor. Der Prelinker `PLINK -px` bildet lange Namen auf eindeutige kurze
  ab und erzeugt Objekte für den MVS-3.8-Linker. CC/370 kürzt heute auf 8
  Zeichen, gleiche Präfixe kollidieren, und die Abhilfe sind `static` oder
  `asm`-Labels (`ug-compile.typ`). Da ld370 die Load Modules selbst baut, könnte
  eine Prelink-Phase dort ansetzen. Voraussetzung: as370 schreibt die langen
  Namen in einer Form ins Objekt, die ld370 lesen kann.
- **`$$HDRMAP`:** Eine Abbildungsdatei für `#include`-Namen, getrennt für
  System- und Benutzer-Header, z. B. `<sys/stat.h>` → Member `SYSSTAT`. Dazu
  `-faddh`, `-flowerh`. Pflicht für einen Compiler, der auf MVS aus PDS-Membern
  übersetzt.
- **Quelle aus 80-Byte-Sätzen:** `-fmargins=m,n` (nur bestimmte Spalten
  lesen), `-ftrim` (Blanks am Satzende entfernen).
- **`#pragma prolkey(f, "…")`:** Einstellungen pro Funktion im Prolog:
  `DCALL=YES`, `ISTK=`, `ESTK=`, `SP=`, `KEY=`, `TRAP=`. Dazu `-fprol=` und
  `-fepil=` für eigene Prolog- und Epilog-Makros, `-fep=` für den Einstiegspunkt.
- **`-fpatch`:** ein Patch-Bereich `@@PATCH_AREA` am Ende jeder Code-Sektion,
  von jeder 4K-Seite aus adressierbar. Klassische Wartungshilfe für Zaps.
- **Builtins:** `__builtin_bswap16/32/64`, `__builtin_popcount`, `__builtin_clz`,
  `__builtin_ctz`, `__builtin_ffs`, die atomaren `__atomic_*`. GCC 3.4.6 (Basis
  von CC/370) kennt `popcount`, `clz`, `ctz` und `ffs`, `bswap` erst ab GCC 4.3.
  Zu prüfen, was CC/370 davon inline erzeugt.
- **Spracherweiterungen:** Binärkonstanten `0b…`, Statement Expressions,
  `typeof`, Qualifizierer `__rent`/`__norent` pro Variable, `-fc99`, `-fc11`.
