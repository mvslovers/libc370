# Socket provider: native stack or X'75'

Status: 2026-08-23. Design note, not a decision. Supersedes the 2026-08-22 version:
the questions it left open about the native stack are now answered, and several of its
assumptions have been verified against both code bases rather than assumed. Companion
document: `nsf370-provider-contract.md`.

Context: a real TCP/IP stack for MVS 3.8j (**NSF370**) is being built in parallel. libc370
must eventually decide **at run time** whether to use that stack or go through the Hercules
X'75' instruction.

---

## Starting point: `socket.c` is not a foreign body

The `socket.c` reviewed on 2026-08-22 comes from a libc that ran both on z/OS (with a real
stack) and on MVS 3.8j. Its `__libc_oarch` is exactly the switch we need — only as a
link-time global rather than determined at run time.

**But the reviewed code refines what that switch actually selects.** It is *not*
"X'75' versus native":

- `__libc_oarch == 0` (3.8j) calls **`EZASOKET("CLOSE   ", …)`** — call-by-name.
- `__libc_oarch != 0` (z/OS) calls **`_C_Sockets(EZASOH03, plist)`** — the module loaded
  via `_L_Sockets()`, parameter list built by hand.

**Both arms speak EZA.** On 3.8j the call-by-name arm resolves to Shelby Beach's EZASOKET,
which uses X'75' underneath — so X'75' is reached only *indirectly*, through an EZA surface.

Two consequences:

1. **Favourable.** NSF370 offers all three EZA surfaces (EZASOKET call-by-name, EZASOH03
   plist, the EZASMI macro). **Both arms of this file would work against NSF370.** The share
   of "transcription" over "design" in reusing it is larger than assumed, and the
   `#ifdef NOTDEF` block (`socket.c:1778-1909`) remains what it was: a compact specification
   of the EZASMI parameter lists, and the most valuable part of the file.
2. **Cautionary.** The switch this file demonstrates is **not our switch.** It discriminates
   the *EZA calling convention*; we must discriminate *which stack*. The two-arm **structure**
   transfers; the **discriminator** is separate work — and that is the genuinely open part
   (see "Detection ladder").

---

## Open decisions

1. **~~EZA-compatible or a private interface?~~ — ANSWERED: EZA-compatible.** NSF370 is
   EZASOKET/EZASOH03/EZASMI compatible by design; unmodified MVS software running
   **relink-only** is the stack's stated goal. `socket.c` therefore supplies usable
   parameter lists.
2. **~~Does the stack register as a subsystem (SSCT)?~~ — ANSWERED: no.** See "Correction"
   below. The detection ladder is rebuilt accordingly.
3. **Override mechanism.** Development needs "force dyn75 even though the native stack is
   up" and the converse. Candidates: DD card, environment variable (`grtenv` exists), CRT
   option (`crtopts`, `clibcrt.h:37`). **Still open.**
4. **Precedence when both are present.** Proposal: native stack wins, override trumps.
   **Still open, proposal unchanged.**
5. **Footprint** — see "Dispatch form". The only decision with direct costs for
   httpd/ftpd/mvsMF. **Still open, and the most expensive.**

---

## Correction: NSF370 does not register as a subsystem

The previous version made the cheapest detection rung `ssct_find("TCPIP")` and hung the
whole ladder on it.

**NSF370 registers no SSCT.** Its SSI transport (ADR-0036) was superseded by a **private
SVC** (ADR-0038, proven live), and the SSI probe was retired. The reason is precisely our
use case: `IEFSSREQ` is **authorized-only**, and unmodified relink-only programs are
unauthorized problem-state code; an SVC is the APF-free transition.

So rung 1 as written is dead — the case the previous version anticipated ("if no, rung 1
must be replaced by something else").

### The replacement rung: the SVC table

Same cost class, same properties — read-only, no storage, no abend risk.

| # | Check | Cost | Primitive |
|---|-------|------|-----------|
| 0 | Override | ~0 | open (decision 3) |
| 1 | **NSF370 at its SVC slot** | ~0, read-only | CVT → SVCTABLE → entry point → eyecatcher |
| 2 | API module present | BLDL, no storage | `__bldl()`, `clibos.h:53` |
| 3 | dyn75 available | one X'75' call | `___try(__75init)`, `clibtry.h:14` |
| 4 | none of the above | — | `socket()` → `-1`/`ENETDOWN` |

Two forms of rung 1, depending on what NSF370 commits to (its questions 1 and 2):

- **Fast path** — read the SVCTABLE entry for the agreed number (**239**), take the entry
  point, check an eyecatcher at a fixed offset. One comparison.
- **Robust path** — walk slots **200–255**, checking the eyecatcher at each entry point.
  56 comparisons, still ~0, independent of the `NSFV_SVCNUM` a given NSF was built with —
  and it yields the actual SVC number, which the caller needs anyway.

A *free* installation slot points at the common "invalid SVC" routine. NSF's own STC already
uses that distinction to steal a slot only when it is demonstrably free; we use the same test
inverted.

**Dependency:** the fixed-offset eyecatcher **does not exist as a contract today** — NSF's
eyecatchers (`"NSFVANCR"`, `"NSFV"`) are internal validation of its own structures. That is
the one commitment we need from NSF370 before rung 1 is implementable.

**Rung 3 still needs no new probe.** `__75init()` *is* the probe: wrap the existing call in
`___try()` and read `___tryrc()`.

**Rung 4 still fixes a present-day defect.** `@@75init.c` issues `__75(&pl)` unconditionally.
On a Hercules without the dyn75 feature that is, as expected, an operation exception — i.e.
**S0C1 in every program that calls `socket()`**. As expected: *not measured*. A single
measurement on a Hercules without the feature settles it, and it is the gate to this whole
branch of the ladder. If the expectation holds, the detection routine is worth building
**before** the native stack is, on its own merits.

### Where it belongs

In the existing lazy hook: `@@75sock.c:27` calls `__75init()` exactly once before the first
socket, under `lock(&grt->grtsock, 0)` — the guarantee of "decided before any socket work"
without burdening every socket-free program. Detection in the CRT prologue would cost all
700+ TU-consuming programs, including the 95 % that never open a socket.

### Where the result belongs

`CLIBGRT.grtflag1` (`clibgrt.h:32`) is a `char` with two bits used; `unused1` at offset 0x0B
sits free beside it (verified). A provider byte fits there **without growing the structure** —
so without a relink round.

---

## Semantic divergence — the real work

Repointing function pointers is the easy half. The two backends have **different contracts**,
and the differences belong without exception **in the respective provider**, not in the
dispatcher.

### The `-2` contract stays with dyn75

X'75' returns `-2` = "nothing moved, wait and reissue". `@@75send.c` (100 × 100 ms, #120),
`@@75recv.c` and `@@75acce.c` each carry a STIMER loop for it. A native stack has no `-2`; it
has `EWOULDBLOCK` and real blocking. Pulling the retry policy into the dispatcher would give
the native path a 10-second stall budget it never needed. `SEND_STALL_MAX` stays where it is.

### The 4096-byte decomposition stays with dyn75

`@@75recv.c` splits every `recv()` into ≤ 4096-byte pieces — a limitation of the emulator
buffer, not a property of TCP. The native provider must not pay for it.

### errno normalisation — **verified: no table needed for NSF370**

The previous version called this the most expensive silent failure class: *"if the new stack
brings its own values, a translation table is needed — and an error in it is silent."*

The numbering is **identical on both sides** (verified against `libc370/include/errno.h` and
`nsf370/include/nsfreq.h`):

| | libc370 | NSF370 |
|---|---|---|
| `EBADF` | 9 | 9 |
| `EINVAL` | 22 | 22 |
| `EAGAIN`/`EWOULDBLOCK` | 35 | 35 |
| `EINPROGRESS` | 36 | 36 |
| `EOPNOTSUPP` | 45 | 45 |
| `ECONNRESET` | 54 | 54 |

Both are the classic BSD set. **For the NSF provider the values pass through unchanged.**
This is not a licence to stop looking: the comparison holds against today's code and belongs
**pinned as a test**, not carried as an assumption — a later divergence would be silent again.
dyn75 is unaffected: it still fetches errno with a **second** X'75' call (function code 2 or
3) after a `-1`.

`ESOCKTNOSUPPORT` (44) and `ENETDOWN` (50) both exist in `errno.h`, so the two conventions
below need no header change.

### `selectex()` and the ECB model — feasible on NSF370

Our `select()` is `selectex()` with a NULL ECB list (`@@75sele.c`), and the X'75' variant is a
nine-subcode state machine that **ignores `timeval`**. A native stack would block properly or
post an ECB. The ECB list in `selectex()` is meaningful for a native stack — and it is how MVS
servers actually wait — and meaningless for dyn75.

For NSF370 the machinery **exists and is proven**: cross-AS branch-entry POST (`__xmpost` via
CVT0PT01) plus the ADR-0040 liveness guard before every POST, and such a POST is documented to
reach an ECB in the target address space's **private key-8 storage**. So a cross-AS
`selectex()` with an ECB list needs a *design*, not a new primitive. Still the largest single
divergence, and still worth its own issue.

### One provider per address space, latched

`CLIBSOCK` (`include/clibsock.h`) and the `grtsock` array form **one** flat namespace over the
socket number. Two simultaneously active providers would hand out overlapping numbers. Rule:
decided at first socket use, never re-evaluated. No "pick the better one per call".

This has a consequence worth stating explicitly: if the provider **disappears at run time**
(`P NSFV` while a consumer is running), there is by definition no re-discovery. The behaviour
must be specified — presumably an error, as with a crashed stack, and **no silent fallback to
dyn75**: a fallback in the middle of socket-number assignment is exactly the overlapping
namespace the latch rule exists to prevent.

### The vector frays

`givesocket()`/`takesocket()` (socket handoff between address spaces) make sense natively —
attractive for httpd/ftpd — and have no dyn75 equivalent. Conversely there are dyn75
peculiarities. So a convention is needed for "this provider cannot do that": **return
`ESOCKTNOSUPPORT`, never call a NULL pointer.** That is exactly what `socket.c` does in its
3.8j arm, and it is the second thing to take from there.

**This is needed immediately, not theoretically: NSF370 v1 has no GIVESOCKET/TAKESOCKET.**
A visible consequence on the NSF side is that its SELECT has **no exception readiness**,
because TAKESOCKET would be the only exception source.

---

## Dispatch form — the decision with costs

Three forms, none free:

**a) Branch in place.** An `if (provider == NATIVE)` in every `@@75xxxx`. Keeps all
`asm("@@75ACCE")` names, changes no public header, no relink round for consumers. It is
exactly the form of `socket.c` itself. Inelegant but cheap.

**b) Provider vector.** `__75vect` (`socket.h:162/214`, filled in `@@75vect.c`) already has the
right *shape* — but is bypassed today: the public entry points are the dyn75 implementations
directly, and the macro dispatch at the end of `socket.h` is under `#if 0`. So there is a
half-finished abstraction to complete. Cleaner, but costs a rename (`__75vect` is X'75'-branded)
and therefore a relink round.

**c) Build-time selection.** Cheapest footprint, gives up run-time detection — i.e. the
requirement.

**The unsolved problem with (a) and (b):** ld370 autocalls from the archive. A vector
referencing both providers — or a branch containing both arms — pulls **both** implementations
into every load module that calls `socket()`. httpd, ftpd and mvsMF pay for the stack they do
not use. On a 24-bit target, with memory as priority 1, that is not a footnote.

Three ways out, none free: accept the duplication; load the non-selected provider at run time
via `__load()` (`clibos.h:202`, and `@@75vect.c` is exactly where a vector would be filled) and
release it with `__delete()`; or take (c) and drop the requirement. **Decide before the first
line of code.**

---

## Constraints inherited from the native stack

- **`FD_SETSIZE` is 1024** in the public header (`socket.h:61`, verified), so the size of
  `fd_set` is compiled into every consumer's stack frames today. The native provider must live
  with it or force a coordinated relink round (TODO tier 5). `socket.c` is the cautionary
  example: its `__jcc_FD_SETSIZE = 256` does not match our 1024.
- **NSF370 clamps MAXSOC to 64** (`NSFEZA_MAXSOC`), whatever INITAPI requests (EZASOKET permits
  50–2000). Against `FD_SETSIZE` 1024 this is functionally harmless — the upper bits are never
  set — but it makes a **third** number in play.
- **EZASOKET SELECT masks are laid out right-to-left in fullwords.** Passing an `fd_set`
  straight through as an EZASOKET mask requires conversion. An error there is silent. By the
  logic of this document, the conversion belongs **in the provider**.

Deciding these now is free; in six months it is expensive.

---

## What comes across from `socket.c` — and what does not

**Take:** the two-arm structure; the `ESOCKTNOSUPPORT` convention for unsupported entry points;
the `NOTDEF` block as the EZASMI specification.

**Do not take:** the translation tables (`socket.c:88,99` are **CP1047**, our runtime is CP037 —
verified via `[` → 0xAD rather than 0x4A and LF → 0x15 rather than 0x25); `inet_addr()`
(`atol`-based, no validation, collapses `0.0.0.0` into the error value). The remaining defects
are in the 2026-08-22 review.

---

## Open questions

Carried forward:

1. **Override mechanism** (decision 3) — DD card / `grtenv` / `crtopts`.
2. **Precedence** (decision 4) — proposal: native wins, override trumps.
3. **Dispatch form and the ld370 autocall problem** (decision 5) — the expensive one.
   **Decide before the first line of code.**
4. **Measure the `@@75init.c` S0C1** on a Hercules without the dyn75 feature. Still the best
   single item here: one measurement, and it pays off even without the native stack.
5. **`FD_SETSIZE` 1024** — live with it, or schedule the coordinated relink round.
6. **`selectex()` with an ECB list** — its own issue; feasible on NSF370, needs a design.

New:

7. **Fast path or robust path for rung 1?** Depends on whether NSF370 fixes 239 as a
   convention. *Recommendation: implement the robust scan, keep the fast path as an
   optimisation — it yields the number and survives a differently built NSF.*
8. **The discovery eyecatcher must be committed to by NSF370** (value, offset, stable from
   which version). Without it rung 1 cannot be implemented.
9. **Capability/version query** — is "NSF is present" enough, or does selection need "which
   verbs does this instance support"? Once GIVESOCKET/TAKESOCKET arrives, libc370 must not have
   to guess the version.
10. **Provider disappears at run time** — specify the behaviour; no silent fallback (see
    "latched").
11. **Pin the errno correspondence as a test**, not as an assumption.
12. **SELECT mask conversion** (`fd_set` ↔ EZASOKET right-to-left) — in the provider.

---

## References

`socket.c` (review of 2026-08-22); `libc370/include/errno.h`, `include/socket.h:61`
(`FD_SETSIZE` 1024), `include/clibgrt.h:32/36` (`grtflag1`, `unused1`), `@@75sock.c:27`,
`@@75init.c`, `@@75vect.c`, `@@75sele.c`, `@@75recv.c`, `@@75send.c`, `@@75acce.c`.
NSF370: ADR-0036 (SSI, transport-superseded), ADR-0038 (private SVC, `NSFV_SVCNUM` 239),
ADR-0040 (client-death guard), `include/nsfeza.h:86` (`NSFEZA_MAXSOC` 64),
`include/nsfreq.h:81-93` (errno values), `include/nsfsel.h:26` (no exception readiness).
Companion: `nsf370-provider-contract.md`.
