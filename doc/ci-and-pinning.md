# CI for libc370, and pinning it in consumer builds

**Status: parked 2026-08-05.** Nothing implemented. This records the analysis so the
discussion does not have to be reconstructed.

Trigger: the `jesprint()` signature change (#21/#22, PR #31) turned into a breaking
change for httpd, mvsmf and ftpd with **no signal in libc370 at all** — it has no CI.

---

## How the ecosystem is wired today

`mbt/.github/workflows/build.yml` is the reusable workflow every mbt project calls
(httpd, mvsmf, ftpd, ufsd). It builds the toolchain from source on the runner:

```yaml
git clone --depth 1 --branch "${{ inputs.cc370_ref }}" ... cc370.git
git clone --depth 1                                    ... libc370.git   # <- no --branch
```

- `cc370_ref` exists as an input but **defaults to `main`**, and no consumer overrides it
- libc370 has **no input at all** — it always takes the default branch

So both halves of the toolchain float at head. That is a deliberate live-at-head model,
not an oversight; libc370 is only the more extreme case because it cannot be pinned even
when you want to.

The toolchain cache key is `cc370-${{ runner.os }}-<cc370 SHA>-<libc370 SHA>`, so a new
libc370 commit is a cache miss and forces a rebuild against it. There is no stale-cache
reprieve.

**Neither libc370 nor cc370 has a `.github/` directory.** Both are invisible until a
consumer trips over them.

`release.yml` clones libc370 the same unpinned way — any fix has to cover both workflows.

---

## The distinction that matters

- **Pinning gives isolation.** It converts "consumer CI is red today" into "consumer CI
  is red whenever somebody raises the pin", with a larger delta and nobody watching. With
  four consumers and one maintainer, early breakage is *cheaper* than deferred breakage.
- **CI in libc370 gives signal.** That is what is actually missing.

Conclusion: CI is the primary measure, pinning is an escape hatch.

---

## Proposed steps

### 1. libc370 CI (foundation)

PR + `push: main`: clone and build cc370, then `make`. Fail on a broken build. Reuse the
toolchain cache from `mbt/build.yml`, otherwise every run rebuilds GCC 3.4.6.

Because this job builds cc370 from source, it is also the first thing that would notice a
cc370 regression — cc370 has no CI either.

### 2. Reverse integration (the step that would have caught #31)

Matrix over httpd, mvsmf, ftpd, ufsd: check each out at `main` and build it **against the
libc370 from the PR**. A source-breaking change then goes red *in libc370, before the
merge*, where the author can act.

Start with `continue-on-error: true`:

- it is red for httpd/mvsmf/ftpd until their migrations land (httpd#129, mvsmf#187, ftpd#68),
  and going green is exactly the completion signal for that work
- a consumer whose `main` is broken for unrelated reasons must not block libc370

Flip to blocking once the false-positive rate is known.

### 3. Warnings: not `-Werror` yet

The tree produces many pre-existing header warnings (`ignoring #pragma pack`, `"/*" within
comment`), so `-Werror` is a separate cleanup project. The workable gate is a **baseline
file** holding `main`'s warning set, failing only on warnings not in it. PR #31 was checked
exactly that way by hand — same warning set as `main`, only shifted line numbers.

First iteration: build must succeed, warning count printed.

**Related and more urgent:** #32 shows the same class of hole one stage later — `as370`
returns rc=8 for a missing macro, the build ignores it, and a silently wrong object ends up
in `libc.a`. A build that fails on a non-zero assembler rc belongs here too, and is being
handled as part of #32.

### 4. `libc370_ref` input in mbt (the escape hatch)

Add the input to `build.yml` **and** `release.yml`, default `main` — behaviour unchanged,
but a consumer can pin itself temporarily while migrating.

**Do not default it to a tag.** That freezes all four consumers at once and turns the
problem into "somebody must maintain the pin" — the pattern that previously let consumers
run against an old libc370 unnoticed.

---

## Prerequisite for pinning to be attractive

libc370 does carry tags and GitHub releases (v1.0.0, v1.0.1 of 2026-07-26) and a `VERSION`
file, currently `1.0.2-dev`. Cutting **v1.0.2** once the three consumer migrations land
would give a coherent marker for "the first libc370 with the new `jesprint()`" — and the
first thing anyone would actually want to pin to.

---

## Open questions

- Is the reverse-integration matrix affordable in CI time once each consumer's `make deps`
  and full build is included?
- Does ufsd belong in the matrix? It does not call `jesprint()`, but it does consume the
  sysroot.
- Should cc370 get the same treatment, or does libc370's CI building it from source cover
  enough?
