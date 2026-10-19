# Naming new public functions

The rule for names that enter `include/` from now on (#246). Existing names
stay as they are: renaming them changes the symbols every consumer links
against, so that is a 3.0 topic or a matter of additive aliases. This page
is about not making the mix worse.

## Where the library stands

The extension headers use three styles side by side (#246 has the count):
`__`-prefixed names (`__dsalc`, `__tzget`), `area_verb` names (`ecb_wait`,
`racf_auth`, `tmr_secs`) and single words (`wtof`, `jesjob`, `osbdcb`).
`__` is reserved to the implementation by C99 7.1.3, yet many `__` names
are public API; single words share the program's own namespace with no
mark that they come from the library.

## The rule

1. **A new family takes the form `area_verb`**, all lower case, with an
   object where it helps: `vsam_info`, `jes_open`, `ecb_post`. `area` is the
   topic of the header the name lives in, short and spelled out (`jes`, not
   `j2`). No new single-word names and no new `__` names in a new family.
2. **A name that extends an existing family follows that family.** A
   variant sits beside the function it varies, with the same stem and a
   suffix: `__linkx` beside `__link`, `jesopen_ex` beside `jesopen`,
   `__renamev` beside the rename functions. A caller reads one family in one
   style; the family moves to the new form as a whole, or not at all.
3. **A name another standard prescribes keeps that standard's spelling:**
   C99 and POSIX names (`sqrtl`, `flockfile`), and IBM's z/OS interfaces that
   LIBC/370 provides for portability (`dynalloc()` of `<dynit.h>`).
4. **`__` is for internals only** in a new family: functions that only the
   library or its startup calls, declared in a `src/` header, never in
   `include/`. A public `__` name exists only under rule 2.
5. **One C name per symbol.** No new `asm()` aliases that give one routine
   several C names; an alias set (`arraycount`, `array_count`, ...) is not
   extended. A standard's names for distinct functions (rule 3) are not
   aliases in this sense, even when two of them share an implementation.
6. **Macros and types** follow their header: `UPPER_CASE` for macros with
   the area as prefix (`VSAM_`, `JES_`), `area_thing` or the IBM mapping
   name for types.

Every new public name is entered in `sdk/names.tsv` (`python3 sdk/names.py
render`); `sdk/headermap.py check` fails until it is.

## Applying it to names already decided

The decisions of 2026-10-19 follow the rule as written:
`__scratch`/`__renamev` and `__linkx`/`__loadx` extend `__` families
(rule 2), `jesopen_ex` extends `jesopen` (rule 2), `__vsopenx`/`__vsinfo`
extend the `__vs` family (rule 2), `<dynit.h>` takes z/OS's names (rule 3),
the `long double` math functions take C99's (rule 3).
