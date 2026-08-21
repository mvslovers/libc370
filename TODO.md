# TODO — offene Issues nach Wichtigkeit

Stand: 2026-08-20, 23 offene Issues. Reihenfolge nach *tatsächlicher* Auswirkung
auf laufende Systeme, nicht nach Alter oder Aufwand.

Zwei Befunde, die die Reihenfolge gegenüber dem Issue-Text verschieben:

- **#108 ist nicht mehr „mechanism unlocated".** #109 und #110 sind gemerged,
  der im Abschlusskommentar benannte Hauptverdächtige #111 liegt als `cd66e86`
  in `main`. Alle im Call-Tree genannten Kandidaten sind eliminiert.
- **Die Caller-Tabelle in #80 ist veraltet.** httpds `httpdslp.c` liegt heute
  unter `httpd/tbd/src/` und wird nicht gebaut. Exponiert ist noch ftpd
  (`ftpd#mvs.c:941`, LIST/NLST mit breitem Filter).

Beide Issues sollten entsprechend nachgezogen werden.

---

## Tier 1 — jetzt

### 1. #80 — `__listpd()`: unbounded allocation, unchecked block length, silent truncation

Drei Defekte in einer Datei. Der zweite ist der schlimmste und steht in keiner
Überschrift: `len` kommt aus dem Block-Inhalt und wird ohne Abgleich mit dem
`fread()`-Ergebnis zur Schleifengrenze über einen 256-Byte-Stackpuffer
(`@@listpd.c:32-33`). Das trifft **auch die gebundenen Caller** — mvsmf
`dsapi.c:540`, ftpd `:1191` — der Filter schützt davor nicht.

- Clamping auf das `fread()`-Ergebnis: eine Zeile, sofort machbar.
- `max`-Parameter bzw. Iterator-Form: Signaturänderung → gehört in die
  Relink-Runde (Tier 5). Löst zugleich Defekt 1 für ftpd und lässt mvsMF
  seinen duplizierten Directory-Parser fallen.

### 2. #11 — cthread teardown force-DETACHed einen lebenden Worker (S33E)

Der einzige *beobachtete und wiederkehrende* Produktionsfehler der Liste.
Bestätigt am 2026-07-22 auf einem libc **nach** PR #7; ein Relink hilft nicht,
weil das ungeschützte `DETACH ...,STAE=YES` in jeder Version steckt.

Die offene Frage — warum ein Worker die 5-Sekunden-Fenster verpasst — ist noch
offen; die Fix-Richtung im Issue ist entschieden (`termecb` als einzige Wahrheit,
STUCK markieren statt DETACH, Speicher der Adressraum-Termination überlassen).
Teuer, aber es ist der real brennende Punkt.

### 3. #107 — `cthread_worker_add()` lässt den Manager-Lock stehen

Eine Zeile: `goto quit` → `goto unlock`. In-tree latent (alle drei Caller halten
den Lock bereits), aber die Funktion ist in `clibthdi.h` exportiert. Der Ausfall
ist ein stiller ENQ-Deadlock des ganzen Worker-Pools im Adressraum — kein Abend,
keine Message, nichts im Dump, das hierher zeigt. Bestes Aufwand/Nutzen-
Verhältnis der gesamten Liste.

### 4. #108 — umklassifizieren, nicht jagen

Kein offener Crash-Hunt mehr, sondern eine **Verifikation**: `dd=1`-Probe gegen
einen degradierten Adressraum wiederholen, dann entweder schließen oder mit
neuen Belegen neu aufmachen. Billig — und solange es unverändert dasteht,
überzeichnet es die Lage.

---

## Tier 2 — Kampagne „unchecked allocation"

### 5. #61 — `__listvl()` liefert stillschweigend eine gekürzte Volume-Liste

Dieselbe Form wie #80/Defekt 3. Beide brauchen *eine* Konvention statt zweier
Einzelentscheidungen — die Empfehlung in #61 (Variante 2: komplett scheitern,
NULL zurückgeben) auf beide anwenden. Der Caller muss NULL ohnehin prüfen; eine
kurze Liste, die vollständig aussieht, ist schlimmer als keine.

---

## Tier 3 — Kampagne „der Compiler soll es sehen" (Reihenfolge zählt)

### 6. #104 — `strcpyp()` nimmt ein nicht-`const` `void *source`

Zwei Zeilen, aber ein Public Header (`clibstr.h` geht in den cc370-Sysroot).
Räumt zugleich die eine Warnung weg, die heute die Rauschgrenze bildet —
dieselbe Dynamik, die #99 verdeckt hat.

### 7. #70 — `sleep()` und `__tzset()` ohne Prototyp

Trivial. Danach kann httpd seine lokalen Deklarationen (httpd#140) löschen.

### 8. #39 — 129 von 712 TUs mit impliziten Deklarationen

Der Elternfall. Schritte 1 (14 Routinen ohne Prototyp deklarieren) und 2
(fehlende `#include`s) sind mechanisch und unabhängig voneinander. Die
Auszahlung ist Schritt 3: `-Wall` in `sdk/mklibc.py` — erst das verhindert die
Wiederkehr dieser Klasse.

### 9. #68 — `format(printf)` für `wtof()`/`wtodumpf()`/`wtorf()`

Hier billig, **im Ökosystem teuer**: Consumer klonen libc370 `main` ungepinnt,
das Attribut färbt httpd-, mvsmf- und ftpd-CI rot, und zwar mit der Bruchstelle
in deren Code. Die im Issue vorgegebene Reihenfolge einhalten:

1. die zwei „too many"-Fälle und das `clibsa.h`-Inline hier fixen,
2. Consumer mit lokal angehefteten Attributen sweepen (braucht keine
   libc370-Änderung),
3. erst dann die Attribute in `clibwto.h` landen.

---

## Tier 4 — strukturelle Fallen

### 10. #17 — die zwei `try()`-Wrapper konsolidieren

Die Falle ist **aktiv, nicht ruhend**: seit sie das erste Mal zugeschlagen hat
(#9 härtete die unerreichbare Kopie), wurden #89, #93 und #96 jeweils *zweimal*
angewendet. Jeder Fix am zentralen Recovery-Pfad kostet zwei Edits und eine
Chance, die falsche Datei zu treffen. Braucht eigenes Review und einen
Validierungsplan — nicht als Beifahrer in einem Relink.

### 11. #72 — die PPA-Environment-Flags sagen nicht, was sie behaupten

Kein Absturz, aber eine dokumentierte API, die falsch antwortet: `TSOBG` ist im
TSO-Vordergrund gesetzt, `TIN`/`TOUT`/`TERR` werden nirgends gesetzt. Billigster
ehrlicher Fix: Semantik von `TSOFG`/`TSOBG` korrigieren und die drei toten
Defines entweder setzen (in `@@fpstar.c`, das es weiß) oder löschen. Deklariert
und tot ist die schlechteste der drei Möglichkeiten.

### 12. #105 — `GRTFLAG1_TSO` klebt über `__start()` hinaus

Design-Entscheidung, kein Patch: pro `__start()` neu berechnen (setzen *und*
löschen) oder die TSO-Eigenschaft in die CRT verlagern. Die beiden Lesarten
unterscheiden sich für `fopen.c`/`ropen.c`/`system.c`. Zuerst die Erreichbarkeit
klären — macht überhaupt etwas ein zweites `__start()` im selben Adressraum?

---

## Tier 5 — Consumer warten (koordinierter Relink, am besten in einer Runde)

### 13. #79 — JESJOB trägt keine Submit-Zeit

Zwei Zeilen plus Struct-Feld. Zowe zeigt heute `exec-submitted` leer. Anhängen
bei Offset 0x50 wie im Issue beschrieben, damit 0x00-0x4F stabil bleiben.

### 14. #50 — Katalogname in DSLIST

Gleiche Klasse, mehr Arbeit. Vor dem Implementieren entscheiden: LISTCAT-Ausgabe
scrapen vs. CVTCATP-Kette vs. LOCATE-Rückgabebereich.

### 15. #51 — `inet_addr()` / `inet_ntoa()`

Guter Einstiegs-Issue und ein echter Speichergewinn: spart ftpd das komplette
`sscanf` im Load-Modul — auf einem 24-Bit-Ziel genau die Art Ersparnis, die
zählt. Host-Test trivial, weil beide Funktionen MVS nicht anfassen.

### 16. #71 — `idcams()` verwirft SYSPRINT und die IDCnnnn-Nummer

Ein Store in einem `switch`-Zweig, der heute nichts tut, plus ein
Companion-Accessor. Danach sagt ftpd „IDC3203I" statt „failed". `idcams()`
selbst behält seine Signatur.

---

## Tier 6 — latent, Forschung, Komfort

### 17. #114 — `osbclose()` gibt keinen OPEN-gebauten Buffer-Pool frei

Nach eigener Analyse latent: MACRF=R und kein BUFNO im Prototyp-DCB, also baut
OPEN normalerweise keinen Pool. In-tree-Caller sind ein Member-Rename und ein
nicht gebauter wip-Baum. httpd#195, die Jagd, die das aufgestöbert hat, ist
geschlossen — das hier war Beifang, nicht der Planter. Mitnehmen, wenn ohnehin
am `osb*`-Pfad gearbeitet wird.

### 18. #113 — `CRTOPTS_AUTH` ist tot, autorisierte Task überspringt `__austep()`

Kein Fehlverhalten heute, nur Code, der etwas Unwahres behauptet. Entscheiden:
`crtopts` bei der CRT-Init aus dem JFCB füllen, oder Feld und Konstante löschen.

### 19. #27 — JES-Spool-Unterstützung ist Single-Volume

Latent: das Referenzsystem hat einen Spool-Volume, alle 264 beobachteten MTTRs
haben `M=00`. Wird an dem Tag akut, an dem ein zweiter Volume dazukommt — und
äußert sich dann als „leeres Dataset", nicht als Fehler.

### 20. #52 — z/OS-kompatibles `dynit.h`

Vor dem Bauen entscheiden, *ob* überhaupt: zwei APIs für einen Dienst
(`__dsalc()` mit String, `dynalloc()` mit Struct, beide enden in `__svc99()`).
Lohnt nur, wenn tatsächlich z/OS-Code hereinportiert wird.

### 21. #30 — SYSOUT über PSO/SSI statt über die gecheckpointete IOT

Forschungsprojekt mit billigem erstem Schritt: Held-Class-Selektion in
`jesxwrtr()` ergänzen und einmal messen, was in `SSSODSN` zurückkommt. Ein Job
entscheidet, ob der Rest geradeaus geht. Erst anfangen, wenn jemand es braucht.

### 22. #37 — SDK: die `.c` parallel kompilieren

6,7 s → ~1 s bei 712 TUs. Entwicklerkomfort. Vorher prüfen, ob parallele
`cc370`-Aufrufe sicher sind (cc1-Temp-Dateien), und keine Fehlermeldung
verlieren.

### 23. #75 — `clock()` als echte Task-CPU-Zeit

Das Issue sagt selbst: dormant, niemand wartet, lua370 ist kein Blocker. Route
(a) über TCT/`TCBTCT` wäre der Weg, macht `clock()` aber
SMF-abhängig — vor jeder Zeile Code entscheiden, ob ein bedingt funktionierendes
`clock()` mehr wert ist als ein ehrlich kaputtes.

---

## Drei Kampagnen statt 23 Einzeltickets

- **Unchecked allocation** — #80/Defekt 3 und #61. Eine Konvention für die
  ganze Bibliothek festlegen, nicht zweimal einzeln entscheiden.
- **Compiler-Sichtbarkeit** — #104, #70, #39, #68. Ziel ist `-Wall` im
  SDK-Build; #68 zuletzt und nach seiner eigenen 3-Schritt-Ordnung, sonst
  reddet es die Consumer-CI.
- **Relink-Runde** — #79, #50 und ggf. #80 mit `max`-Parameter. Struct- und
  Signaturwachstum gebündelt landen, mit CHANGELOG-Eintrag und koordiniertem
  Rebuild von httpd, mvsMF und ftpd.
