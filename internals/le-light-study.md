# Ein "Language Environment light" für MVS 3.8j? Studie zu Nutzen, Umfang und Kosten

> Kopie aus pl1c370 `internals/research/le-light-study.md` (Stand `e053469`, 2026-10-15), die libc370-Teile von libc370 durchgesehen; Abschnitt 7 steht nur hier. Ein Rechercheergebnis, kein Plan: kein Issue, keine Umsetzung.

Stand: 2026-10-15. Reine Studie: kein Repository geändert, nichts auf MVS
gestartet. Jede Aussage ist markiert als **gelesen** (Quelle/Datei genannt),
**gemessen** (Messung mit Herkunft) oder **gefolgert**.

## 0. Kurzfassung

- **LE ist vier Dinge in einem:** ein Programm-/Enclave-Modell, ein
  sprachübergreifendes Condition-Modell, eine Speicherverwaltung mit
  Laufzeitoptionen, und eine Bibliothek von CEE*-Diensten. Dazu kommt eine
  feste Aufrufkonvention (CAA in R12, DSA mit NAB), die ILC erst möglich macht.
- **Unsere Compiler brauchen davon wenig, und das Wenige ist nicht "LE",
  sondern älter.** Was pl1c370 für das Sprachniveau PL/I for MVS & VM fehlt
  (Laufzeitoptionen vor `/`, PLIXOPT, SPIE/STAE-Verhalten, REPORT/ISASIZE,
  FETCH), hatte schon der OS PL/I Optimizing Compiler **ohne** LE
  (SC33-0006-7, Kap. 2 "Execution-Time Options"). Es kann im eigenen Laufzeitsystem
  bzw. in libc370 gebaut werden.
- **Der stärkste konkrete Grund für einen gemeinsamen Kern ist kein
  Feature, sondern eine Kollision:** einen SPIE gibt es nur einmal pro
  Task (ein neuer ersetzt den alten), und ein aktiver SPIE bekommt eine
  Programmunterbrechung vor jedem ESTAE. ESTAEs dagegen stapeln sich
  (LIFO); jeder `try()`-Aufruf richtet seinen eigenen ein und baut ihn
  wieder ab. cobc370 setzt einen SPIE pro Modul, pl1c370 einen in
  `rt/pl1pie.asm`, libc370 baut seine Recovery auf ESTAE (`try()`).
  `@@CCALL` muss den COBOL-SPIE heute schon für jeden C-Aufruf abbauen und
  wieder aufbauen (libc370 `src/mvs/crt/@@ccall.asm`, Kopfkommentar,
  gemessen in libc370#502: `try()` sah U3001 statt S0C1, weil der
  COBOL-SPIE zuerst zugriff). Ein gemischtes Programm C + PL/I + COBOL hat
  heute drei Besitzer für den einen SPIE-Platz; bei ESTAE geht es um
  Reihenfolge und Weitergabe zwischen den Exits der Sprachen, nicht um
  einen Platz (korrigiert nach Durchsicht durch libc370, 2026-10-15).
- **Footprint wird durch eine gemeinsame Laufzeit nicht kleiner.** Bei
  statischem Linken per Autocall bekommt jedes Modul seine Kopie; gemeinsamer
  Code spart nur Quellcode, keine Bytes im Lademodul. Kleiner würde es nur mit
  einer residenten, reentranten Bibliothek in der LPA über Transfervektoren
  (wie die "PL/I shared library" des Optimizers, SC33-0006-7 Anhang C) – ein
  eigenes Produkt mit FMID, IPL zur Aktivierung, und ein reentranter Umbau von
  pl1rt. Die 141-KB-gegen-27-KB-Lücke ist überwiegend stdio und Laufzeitumfang
  (Abschnitt 4.1), und dort (pl1c370#177) ist sie billiger zu schließen.
- **Empfehlung: kein "LE light" als eigenes Projekt.** Stattdessen ein
  kleiner gemeinsamer **Laufzeit-Kern in libc370** (Autocall-Members): eine
  Recovery-Instanz pro Task mit Handler-Registrierung je Sprache, ein
  Laufzeitoptions-Parser (PARM mit `/`, eingebaute Optionsliste), eine
  Meldungsroutine, eine Terminierungs-Policy. Etwa 4–7,5 Personenwochen. ILC
  zwischen PL/I und COBOL/FORTRAN erst, wenn eine echte gemischte Anwendung
  da ist; CEE*-Dienste und eine LPA-Bibliothek nicht jetzt.

---

## 1. Was LE tatsächlich bereitstellt (nach IBMs Handbüchern)

**Quellenlage (wichtig).** Die LE-for-MVS-&-VM-Kernbücher der 1990er
(Concepts Guide GC26-4786, Programming Guide SC26-4818, Programming Reference
SC26-3312, Debugging Guide and Run-Time Messages SC26-4829, Writing ILC
Applications SC26-8351, Run-Time Migration Guide SC26-8232; Form-Nummern
**gelesen** in der Bibliographie von SC26-8499-00, S. 128) sind weder auf
bitsavers noch auf archive.org zu finden. Gelesen wurden daher:
- **1995, original:** *LE for MVS & VM Fortran Run-Time Migration Guide*
  SC26-8499-00 (Dez. 1995, `ceem3000.pdf`, verlinkt von
  https://www.ibm.com/support/pages/node/735707) – im Folgenden **FMG**;
  dazu die PL/I-Bücher SC26-3113-01/-3114-01/-3118-01.
- **1999, Nachfolgeausgaben** "LE for OS/390 & VM" (OS/390 V2R8), unter
  `https://publibfp.dhe.ibm.com/epubs/pdf/`: Concepts Guide GC28-1945-07
  (`ceea8010.pdf`, **CG**), Programming Guide SC28-1939-07 (`ceea2010.pdf`,
  **PG**), Programming Reference SC28-1940-07 (`ceea3010.pdf`, **PR**),
  Debugging Guide SC28-1942-07 (`ceea1010.pdf`, **DG**), Writing ILC
  Applications SC28-1943-07 (`ceea4010.pdf`, **ILC**); COBOL Compiler and
  Run-Time Migration Guide GC26-4764-05 (`igymg202.pdf`, **CMG**).
- Kapitel- und Seitenangaben beziehen sich auf die 1999er Ausgaben. Dass sie
  den SC26-Büchern von 1995 inhaltlich entsprechen, ist **gefolgert**
  (Titelgleichheit); was erst mit OS/390 kam (CEEROPT, HEAPPOOLS, POSIX,
  RTLS), ist hier weggelassen. Die Begriffe "Tier 1/Tier 2" kommen in keinem
  der acht Bücher vor; sie sind keine IBM-Terminologie.

### 1.1 Programm-Management: Prozess, Enclave, Thread

Ein **Prozess** enthält Enclaves, eine **Enclave** ist die Laufeinheit
"Hauptroutine plus Unterroutinen" mit eigenem Heap und eigenen
Laufzeitoptionen, ein **Thread** ist der Ausführungsstrang mit eigenem
Stack und eigenem Condition-Zustand. Die erste Routine der Enclave ist die
Hauptroutine; je Enclave genau eine. Einstieg ist `ENTRY CEESTART`;
Assembler wird LE-konform über die Makros `CEEENTRY` (MAIN=, NAB=),
`CEETERM`, `CEECAA`, `CEEDSA`, `CEEPPA`. Mit **CEEPIPI** (Preinitialisierung,
Funktionscodes init_main=1, call_main=2, init_sub=3, call_sub=4, term=5,
add_entry=6 …) hält ein Nicht-LE-Treiber eine Umgebung über viele Aufrufe.
*Quellen:* CG Kap. 3 "The Language Environment Program Management Model",
S. 11–16; PG Kap. 14 "Program Management Model", S. 167–170; PG Kap. 30
"Assembler Considerations" (CEEENTRY, CEEPPA, CEESTART), S. 477–494; PG
Kap. 31 "Using Preinitialization Services", Tab. 68, S. 505–511. PL/I hatte
vor LE eine eigene Form: PLICALLA/PLICALLB (SC33-0006-7 Fig. 135/136;
SC26-3113-01 Kap. 18, S. 389–421).

**Unser Gegenstück:** PPA+GRT in libc370 ≈ Enclave, CRT ≈ Thread, keine Prozess-Ebene;
`@@CCALL`+`CCTERM` ≈ CEEPIPI init_sub/call_sub/term (Abschnitt 2.1).

### 1.2 Condition-Management

Das Modell ist **rahmenbasiert**: Jeder Stackrahmen kann Handler haben
(Sprach-Handler wie PL/I-On-Units, C-`signal`, und Benutzer-Handler per
**CEEHDLR**/abgemeldet per **CEEHDLU**). Ein *handle cursor* läuft vom
Rahmen der Ursache nach außen; ein *resume cursor* sagt, wo es weitergeht
(versetzbar mit **CEEMRCR**/CEEMRCE). Drei Schritte:
1. **Enablement** – ist die Bedingung überhaupt eingeschaltet (TRAP,
   Programmmaske, PL/I-Präfixe)?
2. **Condition** – Handler je Rahmen fragen; Antworten 10 resume,
   20/21 percolate, 30/31/32 promote, 60 fix-up-and-resume.
3. **Termination imminent** – unbehandelt mit Schwere ≥ 2 wird zu
   *T_I_U* (CEE066, Schwere 3), STOP-artige Anweisungen melden *T_I_S*
   (CEE067, Schwere 1); beide sieht PL/I als FINISH.
Bedingungen werden als **12-Byte-Condition-Token** übergeben: Condition_ID
(Schwere + Meldungsnummer), Case, Severity, Control, 3-stellige
Facility-ID (CEE, IBM, IGZ …), 4-Byte-ISI; derselbe Token ist der optionale
Feedback-Code jedes CEE*-Dienstes. **CEESGL** signalisiert selbst.
ERRCOUNT begrenzt die Zahl behandelter Fehler (Überlauf: U4091 RC 11).
**TRAP(ON,SPIE)** ist Vorgabe: LE nimmt Programmunterbrechungen per ESPIE,
Abends per ESTAE; ein eigener ESTAE/ESPIE des Benutzers stört LE.
*Quellen:* CG Kap. 3 "Language Environment Condition-Handling Model",
S. 16–21; PG Kap. 16 (Handle Cursor, Enablement/Condition/Termination
Imminent Step, Tab. 35–37, ERRCOUNT), S. 199–212; PG Kap. 18 "Coding a
User-Written Condition Handler", Tab. 41, S. 245f; PG Kap. 19 "Using
Condition Tokens", Fig. 85, S. 287–291; PR Kap. 2 "TRAP", S. 79–81; FMG
Kap. 10, S. 48–57 (1995, bestätigt dasselbe Modell).

**Für uns entscheidend:** Auch LE löst das SPIE/ESTAE-Problem durch *einen*
Besitzer je Task und erklärt fremde Exits für unverträglich – genau die
Kollision, die libc370 #502 gemessen hat.

### 1.3 Storage-Management

Stack (`STACK`, Segmente mit Inkrement, KEEP/FREE, ANY/BELOW), Heap
(`HEAP`, Heap 0 = Anfangsheap), `ANYHEAP`/`BELOWHEAP` für Bibliotheksdaten,
`LIBSTACK` für Bibliotheksroutinen (unter 16 MB). Dienste: **CEECRHP**
(Heap anlegen), **CEEGTST**/**CEEFRST** (holen/freigeben), CEECZST (Größe
ändern), CEEDSHP (Heap verwerfen). Vorgaben 1999: STACK(128K,128K,BELOW,KEEP),
HEAP(32K,32K,ANYWHERE,KEEP,8K,4K), BELOWHEAP(8K,4K,FREE), LIBSTACK(4K,4K,FREE).
CAA und Steuerblöcke werden vor Stack und Heap angelegt. `RPTSTG(ON)`
meldet den Verbrauch, damit man STACK passend setzt.
*Quellen:* CG Kap. 3 "Storage Management Model", S. 22f; PG Kap. 15
"Stack and Heap Storage", S. 175–180; PR Kap. 2 STACK (S. 65f), HEAP,
ANYHEAP, BELOWHEAP, LIBSTACK.

### 1.4 Laufzeitoptionen

Eine gemeinsame Optionsliste für alle Sprachen (PG Tab. 17), mit
Vorgabemodulen **CEEDOPT** (Installation) und **CEEUOPT** (pro
Anwendung, eingebunden), beide mit dem Makro CEEXOPT erzeugt. PL/I setzt
sie im Quelltext mit `DCL PLIXOPT CHAR(n) VAR INIT('…') STATIC EXTERNAL`,
woraus der Compiler eine CEEUOPT-CSECT macht. Rangfolge: NONOVR-Optionen >
Exit CEEBXITA > Aufruf (PARM) > Quelle/CEEUOPT > CEEDOPT. PARM-Format
`'[Laufzeitoptionen/][Programmparameter]'`; **CBLOPTS(ON)** dreht die
Reihenfolge für ein COBOL-Hauptprogramm um; **NOEXECOPS** macht die ganze
Zeichenkette zum Programmparameter (PL/I: `OPTIONS(MAIN NOEXECOPS)`).
*Quellen:* PG Kap. 10 "Using Run-Time Options" (Tab. 17, "Order of
Precedence"), S. 119–126; PG Kap. 4 "Specifying Run-Time Options in the EXEC
Statement", S. 50f; PR Kap. 2 "CBLOPTS", S. 20; FMG Kap. 8 (Tab. 5,
VS-FORTRAN→LE), S. 36–40; SC26-3118-01 Tab. 7/12 (OS PL/I→LE).

### 1.5 Meldungen

Laufzeitmeldungen gehen nach **MSGFILE** (Vorgabe
`MSGFILE(SYSOUT,FBA,121,0,NOENQ)`), Benutzerausgabe bleibt in SYSPRINT.
Format: Präfix + vierstellige Nummer + Schwerebuchstabe I/W/E/S/C (0–4),
z. B. `CEE3206S`; Präfixe CEE (LE), EDC (C), IBM (PL/I), IGZ (COBOL),
FOR (Fortran). Dienste: **CEEMOUT** (Text ausgeben), **CEEMSG** (Meldung zu
einem Token), **CEEMGET**, CEECMI, CEENCOD/CEEDCOD (Token bauen/zerlegen).
*Quellen:* DG Kap. 2 "Classifying Errors", S. 29f; PG Kap. 20 "Using and
Handling Messages", S. 313ff; CG Kap. 3 "Message Handling Model", S. 21;
für PL/I SC26-3118-01 "Differences in Run-Time Messages", S. 14.

### 1.6 Datum/Zeit und Mathematik

Datum/Zeit auf Basis des **Lilian-Tags** (Tag 1 = 15. Oktober 1582):
CEEDAYS (Datum → Lilian), CEEDATE (zurück), CEESECS (Zeitstempel →
Sekunden), CEELOCT (lokale Zeit), weitere. Mathematik als **CEESxfff**
(x = Typ I/S/D/Q/T/E/R, fff = Funktion), z. B. CEESDSQT; der Feedback-Code
ist der vorletzte Parameter.
*Quellen:* PG Kap. 21 "Using Date and Time Services", S. 345–347; PG
Kap. 25 "Math Services", S. 421–423.

### 1.7 Interlanguage Communication (ILC)

Eine Hauptroutine je Enclave; C, C++, COBOL, PL/I, Fortran und Assembler
können sich rufen. Registerkonvention an Aufrufstellen: **R12 = CAA**
(außer bei Aufrufen von COBOL, nicht-LE-Assembler und NAB=NO-Routinen);
R13 zeigt auf eine Sicherungsfläche mit erstem Halbwort 0 und gültiger
Rückwärtskette; NAB=YES-Routinen erwarten den NAB des Aufrufers.
Datentypen werden je Paar gemappt (z. B. C `int` = PL/I `FIXED BIN(31)`
über `#pragma linkage(…,PLI)`). Bedingungen: Steht PL/I auf dem Stack,
gilt PL/I-Semantik auch für Bedingungen aus anderen Sprachen; unbekannte
mit Schwere ≥ 2 werden ERROR. Fortran ruft CEE-Dienste nur über
AFHCEEF/AFHCEEN. Ungeklärt: SC26-3113-01 (1995) sagt, PL/I for MVS & VM
habe **kein** ILC mit Fortran, das ILC-Buch 1999 hat ein Kapitel Fortran–PL/I.
*Quellen:* ILC Kap. 1, S. 1f; Kap. 7 "Communicating between C and PL/I"
(Datentypen S. 130, Conditions S. 138f); Kap. 12 (mehrere HLLs), S. 227;
Kap. 13 (Assembler), S. 235; PG Kap. 30 "Register Conventions", S. 475f.

### 1.8 DSA, CAA und Fußabdruck

DSA (Stackrahmen): +4 Rückwärtskette, +8 Vorwärtskette, +C Register,
**+X'4C' (76) CEEDSANAB**, +X'50' NAB am Prologende. Die CAA hat ihren
Eyecatcher bei −X'18', bei +X'2C8' den NAB des Bibliotheksstacks, bei
+X'2D8' den aktuellen Condition-Block (1999er Offsets, für 1995 nicht
geprüft). *Quellen:* DG Fig. 7/8; CMG Fig. 12/13.

**Bemerkenswert (gelesen in beiden Quellen):** cc370s Rahmen trägt den NAB
ebenfalls bei **+76** (`PDPPRLG`: `L 15,76(,13)`; cc370
`internals/metalc.md` §1). Unsere C-, PL/I- und (über `MVC 76(4,15),76(13)`)
FORTRAN-Rahmen folgen also schon der LE-Rahmenkonvention; was fehlt, ist
die CAA in R12 – und R12 ist bei cc370 Basisregister.

**Fußabdruck:** IBM nennt keinen Mindestbedarf. Beispiel in CMG Kap. 3
"Assess storage requirements", Fig. 12, S. 19f: ein einfaches COBOL-Programm
unter LE R5 mit IBM-Vorgaben und Bibliothek aus STEPLIB belegte **268 KB
unter und 1032 KB über 16 MB** – als Illustration, nicht als Zusage. Unter
MVS 3.8j (nur 24 Bit) wäre schon der Unter-der-Linie-Teil ein großer Teil
einer typischen Region.

### 1.9 Was die LE-Compiler von LE verlangten

- **PL/I for MVS & VM 1.1:** braucht LE zum Binden und Laufen; Einstieg
  CEESTART; PLIXOPT wird CEEUOPT; Bibliothek als LIBPACK mit LE; wahlweise
  LE- oder OS-PL/I-Mathematik (SC26-3113-01 Kap. 5, S. 80; "What Is New",
  S. xv f; SC26-3113-01 "Selecting Math Results at Link-Edit Time").
- **COBOL for MVS & VM:** "IBM COBOL compiled programs require Language
  Environment" (CMG); COBOL/370 muss bei jedem Dienstaufruf den
  Feedback-Code übergeben (PG Kap. 19). Das COBOL Programming Guide
  SC26-4767 selbst wurde nicht gefunden.
- **Fortran:** es gab **keinen** LE-Fortran-Compiler; LE führte auf MVS
  Code von FORTRAN IV G1/H Extended und VS FORTRAN V1/V2 aus. Verloren
  gingen u. a. Parallelprogramme, EC-COMMON, die erweiterten
  Fehlerbehandlungs-Unterprogramme und Link-Mode; Fortran-Routinen machen
  ein Lademodul nicht reentrant und können kein CEEPIPI-Ziel sein (FMG
  Kap. 1–3, S. 2–10; Kap. 11, S. 82).

Das bestätigt die Hauptaussage von Abschnitt 2: Was die *Sprachen* an
LE-Abhängigkeit zeigen, ist im Kern Start, Optionen, ein
Condition-Besitzer und eine Aufrufkonvention – nicht die Dienstbibliothek.

---

## 2. Was unsere Compiler davon wirklich brauchen

### 2.1 Bestand: was es heute schon gibt

**libc370 (gelesen: `docs/startup.md`, `src/mvs/crt/`, `src/mvs/recovery/`,
`include/mvs/recovery.h`, `src/signal/`, `src/stdlib/malloc.c`,
`src/mvs/storage/@@getm.asm`, `src/mvs/wsa/@@wsaget.c`)**

| LE-Bereich | Was libc370 heute hat | Datei |
|---|---|---|
| Enclave/Prozess/Thread | Drei Anker, ohne globale Variable vom TCB aus gefunden: **PPA** (je Programmaufruf, in `8(TCBFSAB)`), **CRT** (je TCB), **GRT** (`__grtset()` in `@@grtset.c` legt bei jedem `@@CRT0` einen neuen an; seine Threads (CTHREAD) und crtm-Module teilen ihn, ein zweites C-Hauptprogramm auf einem anderen TCB im selben Adressraum hat einen eigenen). Auf LE abgebildet: PPA+GRT ≈ Enclave, CRT ≈ Thread; eine Prozess-Ebene gibt es nicht (`docs/startup.md` sagt "je Adressraum", der Code nicht). | `@@crt0.asm`, `@@ppaget.asm`, `@@crtset.c`, `@@grtset.c` |
| Start/Ende | `@@CRT0` (voll), `crtm.o` (verschachtelt, erbt), `@@CCALL` (Laufzeit für einen Nicht-C-Hauptprogramm-Aufrufer, gebaut beim ersten Aufruf, `CCTERM` baut ab); Abbau über `@@EXITA`. Hook `__premain(parm, pgmname, pgmr1)` vor `main()`. | `@@crt0.asm`, `@@crtm.asm`, `@@ccall.asm`, `@@exita.asm`, `@@start.c` |
| Stack | NAB-Schema: Rahmenkette im per GETMAIN geholten Bereich, NAB bei **+76** der Sicherungsfläche (`PDPPRLG`: `L 15,76(,13)`). Größe über `@@STKLEN`/`__stklen`, Vorgabe 256 KB (`MAINSTK DS 65536F`). | `@@crt0.asm`; `PDPPRLG` gehört cc370 (`<sysroot>/macros`, seit libc370 2.1 / cc370 1.1, libc370 `internals/releasing.md` "Who owns what") |
| Heap | `malloc` = `__getm` = ein GETMAIN je Anforderung, auf 64 Byte gerundet, Subpool aus `PPAHEAPS`; keine Unterzuteilung, keine Heap-Segmente. | `malloc.c`, `@@getm.asm`, `heapsp.c` |
| Reentranz | schreibbare Statics über `__wsaget()` im GRT | `@@wsaget.c` |
| Recovery | **opt-in**: `__estae()`, `try()`/`___try()`, `abendrpt()`, `tryinfo()`. `@@CRT0` setzt **weder ESTAE noch SPIE** (gelesen: kein Treffer für ESTAE/STAE in `@@crt0.asm`, `@@start.c`, `@@rtinit.c`; SPIE nur in `@@ccall.asm`). | `recovery.h`, `@@estae.c`, `@@@try.c` (`try()`/`___try()`, `tryinfo()`; `@@try.c` ist das alte, unbenutzte `__try()`) |
| Signale | rein softwareseitig: `signal()`/`raise()` pflegen eine Tabelle; keine Hardware-Ausnahme wird zu SIGFPE/SIGSEGV. | `src/signal/*.c` |
| ILC | COBOL/Assembler → C über `CCALL`-Stubs und `@@CCALL` (#502, #507); `@@CCALL` hebt den SPIE des Aufrufers für die Dauer des Aufrufs auf. | `@@ccall.asm`, `docs/consumer-notes.md` "Calling C from COBOL" |
| Meldungen | `wtof()`, `wto_traceback()` – keine Meldungsdatei, kein Meldungskatalog | – |

Weitere Bausteine in cc370: `internals/metalc.md` (#482) beschreibt einen
Prolog für C-Funktionen, die von fremden Aufrufern (ohne +76) betreten werden
– das ist das Gegenstück zu `@@CCALL` auf Compilerseite.

**Wichtig für jede LE-Anlehnung (gelesen, cc370 `internals/metalc.md` §1):**
cc370 benutzt **R12 als Code-Basisregister** (`LA 12,0(,15)` im Prolog). LEs
Konvention "R12 = CAA" ist damit mit jeder cc370-Funktion unvereinbar. Der
Anker muss weiter vom TCB aus gefunden werden (`@@PPAGET`), nicht in einem
Register mitlaufen. Ein echtes LE-kompatibles ILC (fremde LE-Objekte linken)
ist deshalb ausgeschlossen – und auf MVS 3.8j ohnehin gegenstandslos.

**pl1c370 (gelesen: `CLAUDE.md`, `TODO.md`, `docs/language-levels.md`,
`rt/*`, `internals/research/pli/tier82-measurement.md`)**

| Bereich | Heute | Datei |
|---|---|---|
| Start | libc370 `@@CRT0`; `__premain` merkt sich PARM und R1; `pl1_main_parm()` (PL/I (F)-Form: SDV der PARM-Fläche), `pl1_main_plist()` (`-plist` / `OPTIONS(MAIN NOEXECOPS)`) | `rt/pl1parm.c` |
| Conditions | Eigenes Modell: ON-Einrichtungen als 28-Byte-Records im Rahmen, verkettet (neueste zuerst), `pl1_on`, `pl1_revert`, `pl1_block_exit`, `pl1_goto` (Abbau beim GO TO aus Blöcken, nach Adressen, Stack wächst aufwärts), `pl1_signal`, Systemaktionen, ONCODE/ONSOURCE/ONCHAR/ONKEY/ONLOC, FINISH mit Code 2000 | `rt/pl1cond.c` (≈12,6 KB Code) |
| Programmunterbrechungen | eigener **SPIE** für Codes 12, 15 (Exponentenüberlauf, Gleitkommadivision), Rückkehr über absichtliche Division an `PL1PIRT` | `rt/pl1pie.asm` |
| Abends | **kein ESTAE/STAE** (gelesen: kein Treffer in `rt/` mit `LC_ALL=C /usr/bin/grep`, keine Datei mit X'AC' übersprungen). Ein S0C4 oder S80A erreicht weder ERROR- noch FINISH-On-Units. | – |
| Stack/Rahmen | auf libc370s NAB-Stack (`L 15,76(,13)` in `pl1pie.asm`); PL/I- und C-Rahmen teilen sich eine Kette | `rt/pl1pie.asm`, `src/gen.c` |
| Reentranz | **nicht reentrant**: alle Module `rent = false` (`mbt.toml`), ≈28 schreibbare Statics in `rt/*.c` (z. B. `chain`, `acts` in `pl1cond.c`, `files` in `pl1io.c`), schreibbare DS-Felder in den Assembler-Teilen (z. B. `PIESAVE`) | `rt/*.c`, `mbt.toml` |
| Meldungen | Systemaktions-Meldungen direkt nach SYSPRINT (`message()`, `pl1i_errmsg()`) | `rt/pl1cond.c` |
| Datum/Zeit | `DATE`, `TIME` | `rt/pl1date.c`, `rt/pl1time.asm` |

**cobc370 (gelesen über den Teilbericht; Quelle `src/cobc370.c`, `docs/MEASUREMENTS.md`)**
– erzeugt IFOX00/as370-Assembler; die Laufzeit `COBRT` (848 Byte) wird von
`emit_runtime()` (`cobc370.c:9794`) in jedes Deck geschrieben, kein libc370.
Statische Sicherungsflächen, **nicht reentrant** ("Not reentrant: MVS 3.8j
batch does not require it"), keine NAB-Konvention. **SPIE** (`COBSPIE`,
Codes 1–7, 9, 11, 12, 15) einmal pro Lademodul, mappt die PSW-Adresse auf
die COBOL-Zeile und endet mit `ABEND U3000+Code`; kein ESTAE. PARM nur für
`/UPSI(nnnnnnnn)` (`COBUPSI`). Dynamisches CALL über `LOAD EPLOC` (`COBDCAL`).
C aufrufen nur über libc370 `CCALL`.

**fort370 (gelesen über den Teilbericht; GitHub brazilofmux/fort370,
`runtime/s370/`, `docs/BACKEND.md`, `docs/INSTALL-MVS.txt`)** – FORTRAN 77
(NIST FCVS 2.1 bestanden), Assembler-Ausgabe, Laufzeit in C auf libc370
(`f77rt.c`, `f77rec.c`, `f77math.c`, `f77main.c` mit `main()` → `F77MAIN`).
Statische Bereiche je Programmeinheit, nicht reentrant; jeder Prolog reicht
das C-Stackwort weiter (`MVC 76(4,15),76(13)`), damit jede Einheit C rufen
kann. **Kein SPIE/ESTAE, keine IHO-artige Fehlerüberwachung**; E/A-Fehler →
`exit(2)`. PARM wird ignoriert (`main(void)`). Etwa **290 KB Laufzeit** in
jedem Programm (`INSTALL-MVS.txt`).

### 2.2 Bedarf je Compiler

**cc370/libc370 – fast nichts von LE, aber zwei Lücken:**
- Hardware-Ausnahmen kommen nicht als Signale an (`SIGFPE`, `SIGSEGV`
  bleiben softwareseitig). Ein gemeinsamer Recovery-Kern würde das als
  Nebenprodukt liefern.
- Die PARM-Zerlegung in Laufzeitoptionen und Programmargument fehlt; für
  C heißt das LE-Äquivalent `EXECOPS`/`NOEXECOPS`. Für C selbst gering
  wichtig.

**pl1c370 – der eigentliche Abnehmer.** Was das Sprachziel PL/I for MVS & VM
an Laufzeitunterstützung verlangt, und ob es LE braucht:

| Merkmal | Quelle | Schon vor LE? | Bewertung |
|---|---|---|---|
| PARM: Laufzeitoptionen vor `/`, Argument dahinter; `NOEXECOPS` | SC26-3113-01 Kap. 16 "Passing MAIN Procedure Parameters", Fig. 93 (S. 376f); SC26-3118-01 "Differences in Run-Time Options" (S. 17) | **ja**: SC33-0006-7 Kap. 2 "Specifying Execution-Time Options and Main Procedure Parameters in the EXEC Statement" (S. 30f) | nötig, `TODO.md` Punkt 0 nennt es; ohne LE baubar |
| `PLIXOPT` (STATIC EXTERNAL CHAR VAR) | SC26-3114-01 "PLIXOPT Variable" (Kap. 2 "Data Elements", S. 31); SC26-3113-01 S. 305 | **ja**: SC33-0006-7 "Specifying Execution-Time Options in the PLIXOPT String" (S. 29) | billig: ein externes Symbol, schwach referenziert |
| Optionen ISASIZE/STACK, HEAP, SPIE/STAE (→TRAP), REPORT (→RPTSTG), COUNT, FLOW | SC26-3118-01 Tab. 7 und 12 (S. 17f) | **ja**, SC33-0006-7 Fig. 7 (S. 30) | STACK = `__stklen` zur Laufzeit; TRAP = Recovery an/aus; RPTSTG = Hochwassermarke des Stacks; COUNT/FLOW optional |
| ERROR/FINISH bei Abend (STAE) | SC33-0006-7 Fig. 7 "STAE"; SC26-3118-01 Tab. 12 | **ja** | **fehlt heute** (kein ESTAE in `rt/`); braucht ESTAE – die Kollisionsfrage |
| Meldungen IBMnnnnx nach MSGFILE | SC26-3118-01 "Differences in Run-Time Messages" (S. 14) | nein, LE | nicht übernehmen; PL/I (F)-Form (`IHEnnnI`-Stil nach SYSPRINT) bleibt das Orakel |
| U4038/U4039 bei unbehandelter Bedingung, ABTERMENC | SC26-3118-01 S. 14, 17 | nein, LE | nicht übernehmen; Code 2000 wie PL/I (F)/Optimizer |
| FETCH/RELEASE, FETCHABLE | SC26-3114-01 Kap. 5 "FETCH Statement" (S. 114), ONCODE 9250–9255 | FETCH ja (Optimizer) | braucht: geladenes Modul nutzt die Laufzeit des Aufrufers (kein zweites `@@CRT0`). libc370 `crtm` kommt dem nahe, schließt aber beim Ende die Dateien des Elternteils (`docs/startup.md` "Caveat"). Hier liegt echter Bedarf an einem gemeinsamen Kern |
| PLIDUMP, PLIRETV/PLIRETC | SC26-3114-01 "Built-In Subroutines" | PLIRETC/PLIRETV ja | PLIRETC/V trivial (Rückgabecode im Anker); PLIDUMP optional |
| Multitasking (TASK, EVENT, WAIT, PRIORITY) | SC26-3113-01 Kap. 19 | **ja**, Optimizer (TASKHEAP) | baubar auf libc370 `cthread` (CRT je TCB existiert); in `TODO.md` §3 |
| Preinitialisierung (PLICALLA/PLICALLB, CEEPIPI) | SC26-3113-01 Kap. 18; SC33-0006-7 Fig. 135/136 | PLICALLA/B ja | entspricht libc370 `@@CCALL` + `CCTERM`; für "PL/I aus COBOL/Assembler rufen" und für wiederholte REXX-Funktionsaufrufe (TODO "LINKed case") |
| ILC mit COBOL/C (`OPTIONS(COBOL)`, `OPTIONS(ASSEMBLER)`) | SC26-3114-01 "OPTIONS Attribute" | teils (OPTIONS(COBOL) im Optimizer) | erst bei Bedarf |
| SYSTEM(CICS/IMS/TSO), DBCS | SC26-3113-01 Tab. 5 | – | ausgeschlossen bzw. irrelevant |

Fazit pl1c370: Die "LE-Merkmale" der Sprachstufe sind in Wahrheit
Optimizer-Merkmale. Ausgenommen werden sollten nur LE-eigene Formen
(Meldungsformat, U4039, CEE*-Aufrufe, CEEUOPT). Die Ausnahmeliste in
`CLAUDE.md` ("what needs Language Environment") lässt sich damit enger
fassen: PARM-Optionen, PLIXOPT, STAE/SPIE und FETCH gehören ins Ziel.

**cobc370 – braucht für COBOL-74 nichts von LE.** COBOL-74 kennt weder
Laufzeitoptionen noch Condition-Handling über ON SIZE ERROR hinaus. Bedarf
entsteht erst durch ILC: der SPIE pro Modul ist heute der einzige
Berührungspunkt, und er kollidiert (`@@CCALL`). Ein gemeinsamer Kern müsste
cobc370 erlauben, statt eines eigenen SPIE einen Handler zu registrieren –
aber nur, wenn libc370 ohnehin im Programm ist. cobc370s Stärke ist gerade
die libc370-freie 848-Byte-Laufzeit; ein Zwang zum Kern würde sie zerstören.
Also: optional, "wenn vorhanden, dort registrieren" (schwache Referenz).

**fort370 – braucht für FORTRAN 77 wenig:** eine Fehlerüberwachung für
Programmunterbrechungen (Überlauf, Division; IBM-FORTRAN hatte das im
Error Monitor) und vielleicht PARM. Da fort370 ohnehin auf libc370 läuft,
ist es der natürlichste zweite Abnehmer eines Recovery-Kerns. Es ist aber
nicht unser Repository (brazilofmux); Übernahme nur als Angebot.

---

## 3. Vorschlag: Umfang in Stufen

### 3.1 Wo es leben soll

**In libc370, als Autocall-Members – kein neues Repository.** Gründe:
- Start (`@@CRT0`, `@@CCALL`, `__premain`), Anker (PPA/CRT/GRT), Recovery
  (`__estae`, `try`) und WSA liegen schon dort. Ein neues Repo müsste in
  diese Interna greifen oder sie verdoppeln.
- cc370 `internals/runtime-and-release-plan.md` legt `@@CRT0` als
  dokumentierte libc370-Schnittstelle fest.
- Root-`CLAUDE.md`: "Prefer libc370 or in-repo code before adding a
  dependency"; ein neues Repo wäre ein `[dependencies]`-Eintrag für jeden
  Compiler.
- Autocall-Granularität bleibt: ein Programm bekommt nur die Members, die es
  referenziert (pl1c370 `CLAUDE.md`: "keep rarely used parts in files of
  their own").

Gegenargument: libc370 ist eng an cc370-Versionen gekoppelt
(`internals/releasing.md`, Root-`CLAUDE.md` "cc370 and libc370 versions go
together"); jeder Kern-Fix wird ein libc370-Release. Das ist hinnehmbar,
weil die Abnehmer (pl1c370, fort370) libc370 ohnehin mitlinken.

Nur Stufe 4 (residente LPA-Bibliothek) wäre ein eigenes Produkt mit FMID.

### 3.2 Stufe 0 – PARM und Laufzeitoptionen (ohne gemeinsames Condition-Modell)

- `__rtopts()`: zerlegt die PARM am ersten `/` (Optionen links, Argument
  rechts), respektiert NOEXECOPS, mischt drei Quellen in Optimizer-Reihenfolge:
  eingebaute Vorgabe < Programmquelle (PLIXOPT bzw. eine schwach referenzierte
  CSECT `@@RTOPT`, Gegenstück zu CEEUOPT) < PARM (SC33-0006-7 S. 30: "filled in
  successively from the system defaults, the PLIXOPT string, and the PARM").
- Optionen zunächst: `STACK(n)` (ersetzt `__stklen` zur Laufzeit). Achtung
  Reihenfolge: ein C-Parser kann erst laufen, wenn der C-Stack existiert.
  STACK muss daher vor dem Stack-GETMAIN in `@@CRT0` **in Assembler**
  gelesen werden – aus der Quelle (`@@RTOPT`/PLIXOPT als CSECT mit
  Halbwortlänge, per WXTRN wie `@@STKLEN`) und mit einem kleinen
  Assembler-Scan nach `STACK(` im PARM-Teil vor `/`. Alle anderen Optionen
  wertet der C-Teil danach aus. Das ist der einzige invasive Punkt.
  Genauer (libc370, 2026-10-15): `@@CRT0` liest die Stacklänge gleich am
  Eingang über `WXTRN @@STKLEN` (unter 4K oder fehlend: STACKLEN, die
  STACK-DSECT mit `MAINSTK DS 65536F` = 256 KB plus Sicherungsfläche) und
  holt PPA und Stack dann in einem bedingten `GETMAIN RC` (U0801 bei
  Mangel, libc370#108). Die PARM ist dort nur über R11 (R1 am Eingang)
  erreichbar. Ein Scan nach STACK(n) gehört also genau zwischen das ICM
  von `@@STKLEN` und das GETMAIN, in Assembler, ohne eigenen schreibbaren
  Speicher (die CSECT wird in RENT-Module gebunden).
- Weitere Optionen: `TRAP(ON|OFF)` mit Aliasen `SPIE/NOSPIE/STAE/NOSTAE`,
  `RPTSTG(ON|OFF)` / `REPORT`, `MSGFILE(dd)`. Unbekannte Optionen: Meldung,
  wie der Optimizer ("invalid options").
- Schnittstelle: `struct rtopts *__rtoptget(void)` im PPA; Sprache liest,
  was sie braucht.
- Abnehmer: pl1c370 (`rt/pl1parm.c` statt eigener Zerlegung), optional
  fort370.

### 3.3 Stufe 1 – der Kern: eine Recovery-Instanz pro Task, Meldungen, Ende

- **Ein Besitzer für den SPIE je Task**, und für die ESTAE-Exits der
  Sprachen eine feste Reihenfolge mit Weitergabe (ESTAEs stapeln sich,
  ein SPIE ersetzt den vorigen), eingerichtet von `@@CRT0`
  bzw. `@@CCALL` wenn `TRAP(ON)`. Darin eine Handler-Kette pro CRT
  (Registrierung/Abmeldung, LIFO, mit Rahmenzeiger als Gültigkeitsbereich –
  das ist im Kern LEs "stack frame based"-Modell, auf das Nötigste reduziert):
  `__rthdlr_push(fn, frame, mask)`, `__rthdlr_pop(frame)`.
- Ablauf bei Unterbrechung/Abend: Ereignis in einen kleinen Condition-Block
  (Quelle SPIE/ESTAE, Code, PSW, Register, Sprache des Rahmens) übersetzen,
  Handler von innen nach außen fragen; Antworten "behandelt, weiter an
  Adresse/mit Registern" (Resume), "nicht meins" (Percolate), "abbrechen"
  (Promote zu Terminierung). Wiederaufnahme wie in `pl1pie.asm` (PSW im PIE
  umsetzen) bzw. ESTAE-Retry über `SETRP`.
- Sprachanschlüsse: pl1c370 registriert `pl1_program_check` (heute in
  `pl1pie.asm` als eigener SPIE) und neu einen ESTAE-Pfad für ERROR/FINISH
  bei Abend; libc370 bildet auf `SIGFPE`/`SIGSEGV` ab und belässt `try()`
  darauf; cobc370 kann optional seinen Zeilen-Mapper registrieren statt
  eines eigenen SPIE.
- **Meldungsroutine** `__rtmsg(fac, nr, sev, text)`: Ziel nach `MSGFILE`
  (SYSPRINT, SYSOUT-DD, WTO), Format sprachseitig bestimmt (pl1c370 behält
  sein PL/I-(F)-Format – das Orakel entscheidet).
- **Terminierung**: eine Reihenfolge für `atexit`, FINISH, Dateischluss
  über Sprachen hinweg; Rückgabecode-Policy (PLIRETC, `exit()`, COBOL
  RETURN-CODE in einem Feld des PPA).
- **Storage**: STACK-Option, RPTSTG als Hochwassermarke (Stackbereich beim
  Start mit Muster füllen oder NAB-Maximum mitführen). Heap bleibt
  `@@GETM`; ein Heap-Unterzuteiler ist eine eigene, getrennte Frage.

### 3.4 Stufe 2 – ILC

- **C ↔ PL/I**: gleicher NAB-Stack, gleiche Anker – Aufrufe sind fast frei.
  Zu klären sind nur Argumentformen (cc370 übergibt Werte in der Liste,
  PL/I Adressen und Deskriptoren) und `OPTIONS(ASSEMBLER)`/`BYVALUE`.
- **COBOL → C** gibt es (`@@CCALL`). **COBOL/Assembler → PL/I** braucht ein
  `PLICALLA`-Gegenstück: dieselbe Mechanik wie `@@CCALL`, plus PL/I-Init
  (SYSPRINT, Condition-Kette). Das deckt auch TODO "LINKed case" ab
  (wiederholte Aufrufe als REXX-Funktion ohne Neuaufbau).
- **FORTRAN ↔ C** gibt es (Wort 76). **FORTRAN ↔ PL/I** über Adresslisten.
- **Conditions über Sprachgrenzen**: ein PL/I-GO-TO aus einer On-Unit über
  COBOL- oder FORTRAN-Rahmen hinweg (statische Sicherungsflächen, keine
  NAB-Rahmen) – `pl1_goto()` vergleicht Rahmenadressen auf dem C-Stack und
  versteht fremde Rahmen nicht. Das ist der schwierigste Teil und ohne
  Vorbild in unseren Repos.
- **FETCH**: ein per LOAD geholtes Modul ohne eigenes `@@CRT0`, das die
  Anker des Aufrufers nutzt und beim RELEASE nur Eigenes abbaut – `crtm`
  weiterentwickeln (sein Ende schließt heute die Dateien des Elternteils).

### 3.5 Stufe 3 – CEE*-artige Dienste

Teilmenge als dünne C-Hüllen: Datum/Zeit (Lilian-Tage, CEEDAYS/CEEDATE/
CEESECS/CEELOCT-artig), CEEHDLR-artig (auf Stufe 1), CEEMOUT-artig (auf
`__rtmsg`), CEE3ABD, CEEGTST/CEEFRST. Nutzen nur, wenn jemand LE-Quellcode
portieren will. Auf MVS 3.8j gibt es keinen solchen Code; die Namen CEE*
sollten wir ohnehin nicht nachbauen (IBM-Namensraum, wie bei `REXX/370`).

### 3.6 Stufe 4 – residente, geteilte Laufzeit (LPA)

Das Modell des Optimizers: die Bibliotheksroutinen in LPA-Modulen mit
Transfervektor, ins Programm nur ein Adressmodul (`INCLUDE SYSLIB(PLISHRE)`),
SC33-0006-7 Anhang C. Voraussetzungen: alle Laufzeitteile reentrant (pl1rt:
≈28 Statics und Assembler-DS-Felder in den Anker verlagern), libc370-Teile
ebenso, ein Versionsvertrag für den Vektor ("A load module created for use
with one shared library will not execute with a different shared library",
ebd.), Installation per SMP mit FMID, Aktivierung per IPL (MLPA/PLPA). Ein
`LOAD` aus der LINKLIB statt LPA teilt nichts über Adressräume hinweg; es
teilt aber innerhalb eines Adressraums: mehrere PL/I-REXX-Funktionen, die in
derselben TSO-Sitzung per LINK laufen, würden eine JPA-Kopie der Laufzeit
über den Benutzungszähler gemeinsam nutzen statt je ≈150 KB mitzubringen.
Das ist genau der Fall hinter #177, setzt aber dieselbe Reentranz und
denselben Vektor voraus – nur ohne IPL und ohne LPA-Pflege.

### 3.7 Übernahme je Compiler

| Compiler | Stufe 0 | Stufe 1 | Stufe 2 |
|---|---|---|---|
| pl1c370 | `rt/pl1parm.c` ruft `__rtopts()`; PLIXOPT → schwache Referenz | `pl1pie.asm` entfällt, `pl1_program_check` wird Handler; neu ESTAE-Pfad für ERROR/FINISH bei Abend | PLICALLA-Gegenstück, FETCH |
| libc370 selbst | `@@CRT0` liest STACK vor dem GETMAIN | SIGFPE/SIGSEGV aus dem Kern; `try()` darauf | `crtm` für FETCH |
| cobc370 | – (UPSI bleibt) | optional: Zeilen-Mapper als Handler, nur wenn libc370 gelinkt ist | COBOL → PL/I |
| fort370 | optional PARM | Fehlerüberwachung für Unterbrechungen | – |

---

## 4. Kosten und Nutzen

### 4.1 Footprint – das Negativergebnis

Gemessen (Linker-Map `pl1c370/build/tz77.map`, ld370, ausgewertet
2026-10-15, gebunden gegen die `libc.a` im Sysroot vom 2026-10-14,
libc370 2.10.3; von libc370 nachgezählt): Modul TZ77, 159 148 Byte
Sektionen, davon

| Teil | Byte | Anteil |
|---|---|---|
| pl1rt (pl1c370 `rt/`) | 76 364 | 48 % |
| libc370 | 74 520 | 47 % |
| Programm selbst 5 812 + libcc370rt 2 452 (`@@divdi3` ist cc370s, nicht libc370s) | 8 264 | 5 % |

Größte Einzelteile: `pl1io.o` 17,4 K, `pl1cond.o` 12,6 K, `pl1flt.o` 10,3 K,
`pl1rt.o` 9,5 K, `pl1ioget.o` 9,2 K, `pl1ioed.o` 7,9 K; in libc370 die
stdio-Familie (`@@examin` 4,7 K, `fopen`, `@@fpnew`, `@@aopen`, `@@fflush`,
`@@fptmp`, `@@fpopen`, `@@fpswt`, `@@fpstar`, je ≈2 K). Auch der
handgeschriebene Seam-Test P0HELLO hat 138 KB (Dateigröße des Lademodulbilds `build/P0HELLO` auf dem Host, keine Map-Länge). Zum
Vergleich: RXPI 141 KB / 420K VIRT gegen PL/I (F) 27 KB / 56K (pl1c370
`TODO.md` #177, JOB01277); 420K ist vor allem der 256-KB-Stack
(`tier82-measurement.md`). fort370: ≈290 KB Laufzeit je Programm;
cobc370: 848 Byte.

Folgerung: Eine gemeinsame Laufzeit macht kein Modul kleiner, solange
statisch gelinkt wird – Autocall teilt schon heute denselben Code, nur eben
als Kopie je Modul. Kleiner wird es auf zwei Wegen, und keiner ist "LE":
(a) weniger Laufzeit ziehen (SYSPRINT ohne stdio, Stack aus dem Compiler,
`__stklen`/STACK-Option – pl1c370#177), (b) eine residente LPA-Bibliothek
(Stufe 4). (a) ist billiger und wirkt sofort; Stufe 0 (STACK-Option) hilft
dabei für den Regionsbedarf.

### 4.2 Aufwand je Stufe

Bezugsgröße: pl1c370 ging vom ersten Commit (2026-10-12) bis Tier 83
(2026-10-15) – 148 Commits in vier Kalendertagen, im Arbeitsstil dieses
Ökosystems (Sitzungen, Orakelmessungen auf MVS). Die Zahlen unten sind
klassische Personenwochen konzentrierter Arbeit eines Entwicklers, der
libc370 kennt; in diesem Arbeitsstil schrumpft die Kalenderzeit stark, der
Engpass bleibt die MVS-Verifikation und das Gegenlesen.

| Stufe | Inhalt | PW | Begründung |
|---|---|---|---|
| 0 | PARM `/`, Optionsparser, PLIXOPT/`@@RTOPT`, STACK vor GETMAIN (Assembler) | 1–1,5 | reines C plus eine Stelle in `@@CRT0`; Optimizer-Handbuch ist die Spezifikation; testbar host-seitig + ein MVS-Lauf |
| 1 | SPIE-Besitzer, Reihenfolge der ESTAE-Exits, Handler-Kette, Resume/Percolate, Meldung, Ende, RPTSTG | 3–6 | Assembler im Unterbrechungspfad (wie `pl1pie.asm`, `@@estae`), Wechselwirkung mit `try()` und `@@CCALL`; jeder Fall braucht MVS-Messung (Retry, Register, Programmmaske); breite Spanne, weil kein Vorbild für "ein ESTAE, mehrere Sprachen" im Bestand |
| 2 | PLICALLA-Gegenstück, FETCH, PL/I↔COBOL/FORTRAN, Conditions über fremde Rahmen | 4–8 | PLICALLA ist `@@CCALL`-analog (≈1 PW); FETCH 1–2; Unwinding über statische Sicherungsflächen ist das Risiko |
| 3 | CEE*-artige Dienste | 2–4 | dünne Hüllen, aber jede will dokumentiert und getestet sein; ohne Abnehmer |
| 4 | LPA-Bibliothek, reentranter Umbau pl1rt + libc370-Teile, Vektor, SMP-Paket | 6–12 | Umbau aller Statics, Versionsvertrag, IPL-Tests auf mehreren Ständen, FMID |

Stufen 0+1 zusammen: **etwa 4–7,5 PW**. Alles: 15–30 PW.

### 4.3 Risiken

- **Recovery-Pfad**: Fehler im SPIE/ESTAE-Code sind die teuersten
  überhaupt (Schleifen, verlorene Dumps, falscher Retry). Der vorhandene
  Messbefund aus #502 (COBOL-SPIE fängt C-Unterbrechungen, `try()` sieht
  U3001 statt S0C1) zeigt, dass die Kollision real ist – und dass jede
  Änderung gemessen werden muss, nicht abgeleitet.
- **Speicher, 24 Bit**: ESTAE-Arbeitsbereiche, Handler-Kette, Optionsblock
  – alles klein (≤ 1 KB), aber unterhalb der Linie und je Task.
- **Reentranz**: Der Kern selbst muss reentrant sein (Zustand im CRT/PPA),
  auch wenn pl1rt/COBRT es nicht sind – sonst bricht er libc370s
  bestehende Eigenschaft.
- **Kompatibilität**: pl1c370 vergleicht SYSPRINT byte-genau mit PL/I (F);
  der Kern darf Meldungsformate nicht vorgeben.
- **Kopplung**: cobc370 darf durch den Kern nicht an libc370 gebunden
  werden (848-Byte-Laufzeit ist sein Profil).
- **Kein Ende in Sicht bei "LE-Kompatibilität"**: wer CEE*-Namen, CAA und
  LE-Meldungen nachbaut, baut ein Produkt ohne Abnehmer auf MVS 3.8j.

### 4.4 Nutzen

- PL/I-Sprachstufe vollständiger: PARM-Optionen, PLIXOPT, STAE (ERROR bei
  Abend), FETCH – alle in `TODO.md`/`language-levels.md` als offen geführt.
- Gemischte Programme ohne SPIE-Kampf; C-Signale aus Hardware-Ausnahmen.
- Eine Stelle für Stackgröße und Speicherbericht – hilft beim Footprint
  (Region), nicht beim Modul.
- Vorbereitung für ILC und FETCH, ohne sie vorwegzunehmen.

### 4.5 Alternativen

1. **Nichts Gemeinsames, alles je Sprache** (Status quo): billig, aber die
   SPIE/ESTAE-Kollision bleibt, und pl1c370 baut PARM/Optionen/STAE als
   Insellösung, die fort370 nicht nutzen kann.
2. **Nur Start- und Condition-Hooks in libc370 normieren** (Stufe 0 + eine
   schlanke Stufe 1): eine Recovery-Instanz, Handler-Registrierung, Optionen.
   Sprachlaufzeiten bleiben getrennt. **Das ist die Empfehlung.**
3. **Volles "LE light" als neues Repo** mit Enclave-Modell, CEE*-Diensten,
   ILC und LPA-Bibliothek: 15–30 PW, kein Abnehmer für den größten Teil.
4. **PL/I (F)-Bibliothek SYS1.PL1LIB als Laufzeit** (pl1c370#179): löst
   Footprint für `-lang=F` (IHE-Module sind klein und z. T. ladbar), aber
   nicht die Optimizer/MVS-Stufe und nichts für andere Sprachen.

---

## 5. Empfehlung

1. **Kein eigenständiges "LE light".** Was LE über die Optimizer-Laufzeit
   hinaus bietet (sprachneutrales Condition-Token-Modell, CEE*-Dienste,
   LE-Meldungen, CAA-Konvention) hat auf MVS 3.8j keinen Abnehmer und
   kollidiert mit cc370s Registerkonvention.
2. **Jetzt: Stufe 0 in libc370** (1–1,5 PW): `__rtopts()` mit PARM-`/`,
   NOEXECOPS, Quelle `@@RTOPT`/PLIXOPT, Optionen STACK/TRAP/RPTSTG/MSGFILE.
   pl1c370 schließt damit den offenen Punkt "the PARM: the part before `/`
   is run-time options" (`TODO.md` §0) – und die Ausnahmeliste "what needs
   Language Environment" in pl1c370 `CLAUDE.md` sollte enger gefasst werden:
   PARM-Optionen, PLIXOPT, STAE/SPIE und FETCH sind Optimizer-Sprache.
3. **Danach: Stufe 1 als Issue in libc370** (3–6 PW), Auslöser sind zwei
   Befunde: die gemessene SPIE-Kollision (#502) und das fehlende STAE in
   pl1c370 (ERROR/FINISH bei Abend). Erster Schritt: Design-Note in
   libc370 `internals/`, dann Messplan auf MVS (Retry, Programmmaske,
   verschachtelte Handler, `try()` darin).
4. **Footprint getrennt verfolgen** über pl1c370#177 (stdio-freie
   SYSPRINT-Ausgabe, Stack aus dem Compiler). Eine LPA-Bibliothek (Stufe 4)
   erst, wenn danach noch ein messbarer Bedarf bleibt.
5. **Stufe 2 (ILC, FETCH) und 3 (Dienste) nur auf Anforderung** einer
   konkreten gemischten Anwendung; PLICALLA/"LINKed case" ist der
   wahrscheinlichste erste Fall (REXX-Funktionen in PL/I).
6. cobc370 und fort370 (brazilofmux) nur als **Angebot**: schwache
   Referenz auf den Kern, keine Pflicht.

## 6. Stand und Aufsatzpunkt (2026-10-15)

**Entscheidung des Maintainers:** Die Studie ist ein Rechercheergebnis,
kein Plan. Es gibt kein Issue in libc370; der gemeinsame Kern wird nicht
gebaut, bevor jemand ihn ausdrücklich will.

**Was seit der Studie in pl1c370 geschehen ist:**
- Die PARM-Zerlegung am `/` (Stufe 0, PL/I-Teil) hat pl1c370 selbst
  gebaut, in `rt/pl1parm.c` (Tier 84, pl1c370#189): ohne NOEXECOPS
  bekommt ein CHARACTER VARYING-Parameter den Teil nach dem ersten `/`.
  Die Optionen davor werden nicht ausgewertet; Laufzeitoptionen gibt es
  noch keine.
- Der LE-Ausschluss in pl1c370 `CLAUDE.md` ist enger gefasst
  (pl1c370#192): Laufzeitoptionen, PLIXOPT, ISASIZE, STAE/SPIE, REPORT
  und FETCH gehören zur Sprache; ausgeschlossen bleibt nur, was es allein
  in LE gibt. Die Liste mit Begründungen: pl1c370
  `docs/language-levels.md`, "Excluded from the default".
- Die fehlende STAE/ESTAE-Behandlung in pl1c370 (Befund 3) wird als
  Nächstes **innerhalb von pl1c370** geschlossen: erst eine Probe auf
  MVSTK5-BLD, was PL/I (F) bei einem Systemabbruch tut (IHESAP setzt
  STAE, GC28-6594-7 S. 204f; Meldung, ERROR/FINISH, Abschlusscode), dann
  ein Tier, der es nachbildet. Dabei muss pl1c370 mit libc370s
  ESTAE-gestützter Recovery (`try()`) auskommen -- genau die Kollision
  aus 2.1, im Kleinen.

**Wenn das Thema wieder aufgenommen wird, in dieser Reihenfolge:**
1. Den Stand oben prüfen: Was hat pl1c370 inzwischen selbst gebaut (PARM,
   ESTAE), was davon gehört eigentlich in einen gemeinsamen Kern?
2. Stufe 0 in libc370: Laufzeitoptionen aus PARM und `@@RTOPT`/PLIXOPT;
   STACK muss in `@@CRT0` vor dem Stack-GETMAIN gelesen werden (3.2).
3. Stufe 1 zuerst als Design-Note in libc370 `internals/`, mit Messplan
   auf MVS: ein SPIE-Besitzer pro Task, eine feste Reihenfolge der
   ESTAE-Exits der Sprachen, Handler-Kette je Sprache,
   Retry, Programmmaske, verschachtelte Handler, `try()` darin
   (libc370#502 als Prüfstein). pl1c370s ESTAE ist dann der erste
   Abnehmer.
4. Footprint getrennt: pl1c370#177.
5. Stufen 2 bis 4 nur auf eine konkrete Anwendung hin; cobc370 und
   fort370 als Angebot.

---

## 7. Bewertung aus libc370-Sicht (2026-10-15)

Dieser Abschnitt steht nur in der libc370-Kopie. Er hält fest, wie libc370
die Studie liest, was bei der Durchsicht herauskam und wie libc370 vorgehen
würde, wenn das Thema aufgenommen wird. Er ist kein Beschluss: Die
Entscheidung des Maintainers aus Abschnitt 6 gilt weiter.

### 7.1 Ergebnis der Durchsicht

Die libc370-Teile (2.1, 3.1, 3.2, 4.1) wurden gegen libc370 `main`
(2.11.1-dev) und die Map `pl1c370/build/tz77.map` geprüft; die Korrekturen
sind in pl1c370 #195–#197 eingegangen und in dieser Kopie enthalten:

- **SPIE und ESTAE sind verschieden knapp.** Einen SPIE gibt es einmal pro
  Task, ein neuer ersetzt den alten, und er bekommt eine
  Programmunterbrechung vor jedem ESTAE. ESTAEs stapeln sich (LIFO). Die
  Kollision aus libc370#502 (`try()` sah U3001 statt S0C1) ist eine des
  Vorrangs, nicht eines einzigen ESTAE-Platzes.
- **Der GRT ist nicht je Adressraum**, wie `docs/startup.md` sagt, sondern
  je `@@CRT0`-Lauf (`__grtset()` legt jedes Mal einen neuen an); Threads und
  crtm-Module teilen ihn. PPA+GRT entsprechen damit der LE-Enclave, der CRT
  dem Thread; eine Prozess-Ebene gibt es nicht. `docs/startup.md` ist an
  dieser Stelle noch zu berichtigen.
- `PDPPRLG` gehört cc370, `try()`/`tryinfo()` liegen in `@@@try.c`.
- Die Reihenfolgefalle 3.2 und die Footprint-Zahlen 4.1 stimmen
  (nachgezählt: libc370 74 520, pl1rt 76 364, Programm 5 812 + libcc370rt
  2 452 Byte).

### 7.2 Wir haben schon ein LE – ohne Vertrag

Alle Sprachen dieses Ökosystems außer cobc370 setzen auf libc370 und
Assembler auf. Damit gibt es heute schon eine gemeinsame Laufzeit; sie heißt
nur nicht so, und sie trägt, weil alle dasselbe benutzen, nicht weil eine
Schnittstelle abgemacht ist.

| LE | libc370 / cc370 heute |
|---|---|
| Enclave | PPA + GRT, je `@@CRT0`-Lauf |
| Thread | CRT je TCB |
| Prozess | – (nicht nötig) |
| CEEPIPI | `@@CCALL` + `CCTERM` |
| DSA mit NAB bei +76 | gleich (`PDPPRLG`) |
| `STACK`-Option | `@@STKLEN`/`__stklen`, nur zur Bindezeit |
| Condition-Management | `try()`/`tryinfo()`, `__estae()`, opt-in |

Zwei Grenzen bleiben:
- **cobc370** hat eine eigene 848-Byte-Laufzeit ohne libc370. Es gehört nur
  dazu, wenn es sich freiwillig anmeldet, falls libc370 ohnehin im Programm
  ist (schwache Referenz); ein Zwang würde sein Profil zerstören.
- **R12 = CAA** bekommen wir nie, weil R12 bei cc370 Basisregister ist.
  Unsere Anker findet man vom TCB aus (`8(TCBFSAB)`); fremden LE-Code, zu
  dem wir kompatibel sein müssten, gibt es auf MVS 3.8j nicht.

Ob rexx370 so auf libc370 aufsetzt wie pl1c370, ist nicht geprüft
(**gefolgert** aus dem Bau mit cc370).

### 7.3 Was nachzubessern wäre, nach Gewicht

1. **Wem der SPIE gehört.** Der einzige gemessene Schaden (#502). Ein SPIE
   je Task, bei dem sich die Sprachen mit Handlern anmelden, statt dass
   jede einen eigenen setzt; dazu eine feste Reihenfolge der ESTAE-Exits
   der Sprachen.
2. **Ein Ende für alle.** Reihenfolge von `atexit`, PL/I-FINISH und
   Dateischluss über Sprachen hinweg, und eine Rückgabecode-Regel
   (`exit()`, PLIRETC, COBOL RETURN-CODE).
3. **Verschachtelte Module.** `crtm` schließt beim Ende die Dateien des
   Aufrufers (`docs/startup.md`, "Caveat"). Für PL/I-FETCH und für
   rexx370-Funktionen in einer TSO-Sitzung darf es nur abbauen, was ihm
   gehört.
4. **Laufzeitoptionen**, zuerst `STACK(n)` zur Laufzeit (3.2).
5. Als Nebenprodukt von 1: Hardware-Ausnahmen als `SIGFPE`/`SIGSEGV`.

Dazu kommen kleine Lücken, die eine Sprachlaufzeit an libc370 heute schon
merkt und die ohne Kern lösbar sind. Beispiel aus pl1c370 (2026-10-15):
PL/I (F) endet bei vollem Data Set mit SD37, libc370s x37-Exit macht daraus
`ENOSPC`. Welcher Code es war, sieht der Exit nicht: IFG019RA übergibt ihm
nur R1 = DCB, IFG0554T hält den internen Code (184/185/186 = E37-4/B37-4/
D37-4) in seinem eigenen R12 (**gelesen**, mvs38src). Bei 16 Extents und
bei VIO ruft IFG0554T den Exit gar nicht, sondern treibt SYNAD mit X'42'
im höchsten Byte von R1 – libc370 meldet dann `EIO` statt `ENOSPC`.
Beides geht als eigene Issues an libc370.

### 7.4 Vorgehen, wenn das Thema aufgenommen wird

1. **Erst den Vertrag aufschreiben, dann Code.** Eine Design-Note in
   `internals/`: welche Anker eine Sprachlaufzeit lesen und schreiben darf
   (PPA, CRT, GRT, WSA), wie sie sich für Recovery und Ende anmeldet, was
   sie nie tun darf (eigenen SPIE setzen, `8(TCBFSAB)` verändern). Das
   macht den heutigen stillen Konsens prüfbar.
2. **Messplan für den SPIE-Besitzer, bevor er gebaut wird.** Fälle:
   Programmunterbrechung mit und ohne angemeldeten Handler; `try()` darin;
   verschachtelte Handler zweier Sprachen; Retry und Programmmaske;
   `@@CCALL` aus COBOL mit aktivem COBOL-SPIE (#502 als Prüfstein). Jeder
   Fall mit roter Kontrolle auf dem heutigen Stand.
3. **pl1c370 als erster Abnehmer.** pl1c370 setzt ab jetzt ein
   programmweites ESTAE (PL/I (F) setzt STAE). Weil ESTAEs sich stapeln,
   liegt es unter jedem `try()` und stört es nicht. Zu prüfen ist nur, dass
   ein Retry dieses ESTAE `8(TCBFSAB)` nicht auf eine abgebaute PPA
   zurücksetzt – `try()` stellt das Wort nach einem Abend selbst wieder
   her (libc370#89, #93).
4. **Stufe 0 (Optionen) unabhängig davon**, sobald ein Abnehmer sie
   braucht; die einzige heikle Stelle ist `@@CRT0` zwischen dem Laden von
   `@@STKLEN` und dem GETMAIN.
5. **Kleine Lücken sofort, ohne Kern** (wie x37), damit sich keine
   Sprachlaufzeit um libc370 herumbaut.

Das Ziel ist kein "LE light" als Produkt, sondern ein dokumentierter
Laufzeitvertrag, den libc370 schon fast erfüllt.

## Anhang: Quellen

Lokal gelesen: libc370 `docs/startup.md`, `docs/consumer-notes.md`
("Calling C from COBOL"), `include/mvs/recovery.h`, `src/mvs/crt/@@ccall.asm`
(Kopf, Z. 40–75), `src/signal/*.c`, `src/stdlib/malloc.c`,
`src/mvs/storage/@@getm.asm`, `src/mvs/wsa/@@wsaget.c`; cc370
`internals/metalc.md`, `internals/runtime-and-release-plan.md`; pl1c370
`CLAUDE.md`, `TODO.md`, `docs/language-levels.md`, `mbt.toml`,
`rt/pl1parm.c`, `rt/pl1cond.c`, `rt/pl1pie.asm`,
`internals/research/pli/tier82-measurement.md`, `build/tz77.map`; cobc370
und fort370 über einen Teilbericht (Dateien in 2.1 genannt).

PL/I-Handbücher (lokal, `pl1c370/internals/manuals/`, Text per pdftotext):
SC26-3113-01 PL/I for MVS & VM Programming Guide (Kap. 18 Preinitializable
Programs; Kap. 16 "Passing MAIN Procedure Parameters", Fig. 93, S. 376f; Kap. 19 Multitasking; Tab. 5 SYSTEM Option; S. 305
PLIXOPT/RPTSTG); SC26-3114-01 Language Reference ("PLIXOPT Variable",
"FETCH Statement" S. 114, "OPTIONS Attribute", FINISH, ONCODE 9250–9999,
PLIDUMP); SC26-3118-01 Compiler and Run-Time Migration Guide ("Differences
in Run-Time Options" S. 17f, Tab. 7 und 12; "Differences in Run-Time
Messages" S. 14); SC33-0006-7 OS PL/I Optimizing Compiler Programmer's Guide
(1985; Kap. 2 Execution-Time Options, Fig. 7, S. 29–31; PLICALLA/PLICALLB
Fig. 135/136; Anhang C Shared Library S. 404).

LE-Handbücher: Abschnitt 1, "Quellenlage" (die Auszüge und das englische
Rechercheprotokoll der Studie wurden nicht aufbewahrt).
