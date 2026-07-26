# Fix plan — Mixmaster 3.0 easter-egg fork

Audit date: 2026-07-26 · Reviewed at commit `7756250` · Platform: macOS 27 arm64, Apple clang 21, OpenSSL 3 (Homebrew)

Verification performed: full `scripts/build-macos.sh` build (succeeds, exit 0), a second
AddressSanitizer build, and end-to-end message processing through `bin/mixremailer -R`
against a real `Mix/` spool.

---

## Status

**All findings below are fixed** on branch `fix/audit-2026-07` (five commits on top
of `7756250`). Verification after the fixes: full clean build with **0 warnings**
(down from 397), `make -C Src check` green at **39 checks / 0 failures**, and the
whole suite **AddressSanitizer-clean**.

| # | Severity | Issue | File | Fixed in |
|---|----------|-------|------|----------|
| F1 | **High** | Remotely-triggerable out-of-bounds stack read, contents mailed to requester | `Src/stats.c:210` | `94bad8e` |
| F13 | **High** | NULL deref on a malformed key — `pk_encrypt`/`pk_decrypt` used a rejected key | `Src/crypto.c:392,414` | `ab22199` |
| F2 | Medium | `BUFFER` leaked on every malformed key | `Src/crypto.c:93,157` | `ab22199` |
| F3 | Medium | Off-by-two bound check; `MAX_RSA_MODULUS_LEN` never enforced | `Src/crypto.c:93` | `ab22199` |
| F4 | Medium | Unbounded `sprintf` into `PATHMAX` buffers | `Src/rem.c:86,90`, `Src/menusend.c:296` | `ab22199` |
| F5 | Medium | `putenv()` given a stack-local buffer (dangling env pointer) | `Src/util.c:637` | `ab22199` |
| F14 | Medium | `id` buffer leaked in `pgp_rkeylist()` | `Src/pgpdb.c:553` | `c3dfe29` |
| F6 | Low | Block-request parser mutates the message it is parsing | `Src/remailer/remailer_admin.c:88` | `2813427` |
| F7 | Low | Any body *mentioning* `destination-block` is treated as a block request | `Src/remailer/remailer_admin.c:91` | `2813427` |
| F8 | Low | `--no-detach` alone silently does nothing | `Src/remailer/mixremailer.c:74` | `2813427` |
| F9 | Low | 140 self-inflicted macro-redefinition warnings | `Src/config.h:84` | `c3dfe29` |
| F10 | Low | Dead `remailer` target shadowed by `Src/remailer/`; `clean` named the source dir | `Src/Makefile.in` | `2813427` |
| F11 | Process | Documented traceability invariant is broken | `MODERNIZATION.md`, `CHANGES-2026.txt` | `c892053` |
| F12 | Process | No automated tests actually run | `Src/tests/`, CI | `c892053` |

F13 and F14 were not in the original audit — F13 surfaced while writing the crypto
regression test, F14 while clearing the warning backlog. Both are described below.

---

## F1 — Out-of-bounds stack read in `stats()`, disclosed by email  ·  **High**

### The defect

`Src/stats.c:210`:

```c
for ((i = (today - havestats) / SECONDSPERDAY) > 79 ? 79 : i;
     i >= 1; i--) {
```

The intent is to clamp `i` to 79. But the clamp is written as a ternary *expression whose
result is discarded* — clang flags it as `-Wunused-value`. What actually executes is the
assignment `i = (today - havestats) / SECONDSPERDAY`, with **no upper bound**. The arrays
indexed in the loop body are `int msg[7][80]` and `int pool[2][80]` (lines 62, 72).

The index is unbounded because `havestats` is updated *outside* the 80-day guard that
protects the writes. At lines 107–110:

```c
else if (today - then < 80 * SECONDSPERDAY)
  msg[type][(today - then) / SECONDSPERDAY] += num;   /* guarded */
if (havestats == 0 || then < havestats)
  havestats = then;                                   /* NOT guarded */
```

The same pattern repeats at lines 122–127 and 142–145. So any record in `stats.log` or
`id.log` older than 80 days sets `havestats` beyond the array range while contributing
nothing to the arrays.

### Reachability

Unauthenticated. An email with `Subject: remailer-stats` classifies as `REM_KIND_STATS`
(`remailer_dispatch.c:69`) and dispatches straight to `stats()` at `remailer_dispatch.c:136`.

### Confirmed impact

AddressSanitizer, triggered by a plain `Subject: remailer-stats` message:

```
ERROR: AddressSanitizer: stack-buffer-overflow on address 0x00016dc996a0
READ of size 4 at 0x00016dc996a0 thread T0
    #0 stats stats.c:220
    #1 remailer_dispatch remailer_dispatch.c:136
    #2 remailer_process_message remailer_dispatch.c:226
    #3 main mixremailer.c:138
  [1088, 3328) 'msg' (line 62) <== Memory access at offset 3328 overflows this variable
```

Without ASan the process does not crash — it formats the out-of-bounds words into the
reply and mails them to whoever asked. Observed reply (431 lines instead of ~100):

```
21 Jun:    0  Intermediate:   0  Mail:   0  Postings:583045632
22 Jun:    0  Intermediate:   0  Mail:   0  Postings: 121
23 Jun:    0  Intermediate:   0  Mail:   0  Postings:583045584
01 Jul:    0  Intermediate:   0  Mail:   0  Postings:-188923520
```

Those `Postings:` values are adjacent stack words (note `583045632` and `583045584`
differ by 48 — consecutive pointer-like values). For a tool whose entire purpose is
metadata resistance, mailing stack contents to an anonymous requester is the worst
possible failure mode. Secondary effects: reply size amplification (23 KB vs ~2 KB), and
undefined behaviour that may fault instead of leaking.

Triggering conditions in practice — a remailer idle longer than 80 days whose `stats.log`
still holds old records; a backwards clock-step; or a fresh `stats.log` alongside an
`id.log` retained past 80 days (`IDEXP` has a lower clamp at `mix.c:745` but **no upper
bound**, so `IDEXP 200d` is honoured). It is one-shot per occurrence — `stats()` rewrites
`stats.log` bounded to 80 days at line 168 — but it recurs every time an out-of-range
record reappears.

### Fix

```c
/* Src/stats.c:210 */
i = (today - havestats) / SECONDSPERDAY;
if (i > 79)
  i = 79;
for (; i >= 1; i--) {
```

Then harden the root cause so the index cannot go out of range even if a new path is
added — clamp `havestats` at the three assignment sites, or assert the bound before the
loop. Add a regression test that writes a `stats.log` record 400 days old and asserts the
reply contains at most 79 daily rows.

---

## F13 — NULL dereference on a malformed key  ·  **High**  ·  found while testing

`Src/crypto.c`. Both public-key entry points discarded the parse result and used
the RSA object regardless:

```c
int pk_encrypt(BUFFER *in, BUFFER *keybuf)
{
  key = RSA_new();
  read_pubkey(keybuf, key, NULL);          /* return value dropped */
  buf_prepare(out, RSA_size(key));
  out->length = RSA_public_encrypt(in->length, in->data, out->data, key, ...);
```

When `read_pubkey` rejects the blob, nothing is set on `key`, so `RSA_size()` and
`RSA_public_encrypt()` walk a NULL modulus. Confirmed under lldb:

```
stop reason = EXC_BAD_ACCESS (code=1, address=0x8)
frame #0: BN_num_bits + 24
```

`pk_decrypt` had the identical shape with `read_seckey` and
`RSA_private_decrypt`. A single corrupt entry in `pubring.mix` — which in normal
operation is populated from remailer `remailer-key` replies — was enough to crash
the client or the remailer.

**Fixed:** check the return value and fail cleanly, leaving the output buffer
empty, which is the state callers already handle from a failed decrypt. Covered by
three cases in `tests/test-crypto-keys.c` (malformed public key, malformed secret
key, well-formed-length blob with a zero modulus).

This one is worth noting as a process point: it was invisible to review because
the bug is an *absent* check, and it only became obvious once a test fed the
function something malformed.

---

## F14 — `id` buffer leaked in `pgp_rkeylist()`  ·  Medium  ·  found while testing

`Src/pgpdb.c:553`. `pgp_rkeylist()` allocates `userid` and `id` but frees only
`userid`, leaking one `BUFFER` per call. It surfaced only because clang's
`-Wunused-but-set-variable` on the neighbouring `err` drew attention to the
function — which is the argument for keeping the warning count at zero.

**Fixed:** free `id`, and stop storing the `pgpdb_getkey()` return code that the
function never consulted (the `id->length == 8` check is the real guard, now
stated as such).

---

## F2 — `BUFFER` leaked on every malformed key  ·  Medium

`Src/crypto.c`, both key readers allocate before validating and return without freeing:

```c
static int read_seckey(BUFFER *buf, SECKEY *key, const byte id[])
{
  md = buf_new();
  ...
  if (3 * len + 5 * plen + 8 < buf->length || 3 * len + 5 * plen > buf->length)
    return (-1);            /* line 94 — leaks md */
```

```c
static int read_pubkey(BUFFER *buf, PUBKEY *key, const byte id[])
{
  md = buf_new();
  ...
  if (2 * len + 2 != buf->length)
    return (-1);            /* line 158 — leaks md */
```

Reachable per-message through `pk_encrypt`, `pk_decrypt`, `check_pubkey`, `check_seckey`,
so a long-running remailer fed malformed keys leaks steadily.

**Fix:** `buf_free(md); return (-1);` at both sites — or move both `buf_new()` calls below
the length validation, which is cleaner since neither buffer is used before it.

---

## F3 — Off-by-two bound check, and an unenforced limit  ·  Medium

`Src/crypto.c:93`. The reads start at `buf->data + 2` and consume `3*len + 5*plen` bytes,
so the last byte touched is at offset `3*len + 5*plen + 1`. The guard only establishes
`3*len + 5*plen <= buf->length`, which permits reading up to **2 bytes past
`buf->length`**. Today this is absorbed by the 128-byte over-allocation in `buffers.c`
(`#define space 128`), so it is not exploitable — but the check is simply wrong.

Separately, `MAX_RSA_MODULUS_LEN` is defined at `crypto.c:77` and **never used**. Nothing
constrains `len`, which can reach 8192 (`bits` is a 16-bit little-endian field, so up to
65535 bits).

**Fix:**

```c
if (len > MAX_RSA_MODULUS_LEN)
  { buf_free(md); return (-1); }
if (3 * len + 5 * plen + 8 < buf->length || 3 * len + 5 * plen + 2 > buf->length)
  { buf_free(md); return (-1); }
```

Apply the analogous `+ 2` correction and length cap in `read_pubkey`.

---

## F4 — Unbounded `sprintf` into `PATHMAX` buffers  ·  Medium

The 2026 pass converted `Src/pool.c` to `snprintf` (10 call sites) but missed the same
pattern elsewhere. `Src/rem.c:86,90`:

```c
sprintf(fname, "%s%cp%02x%02x%02x%02x%02x%02x%01x", POOLDIR, DIRSEP, ...);
```

`POOLDIR` is `char[PATHMAX]` (`mix.c:85`) and `fname` is `char[PATHMAX]`
(`rem2.c:410,467`). The format appends roughly 16 characters, so a `POOLDIR` within 16
bytes of `PATHMAX` overflows the caller's stack frame. `POOLDIR` is derived from the
configured spool path via `mixfile()`, which fills up to `PATHMAX-1`. Same defect at
`Src/menusend.c:296`.

**Fix:** convert all three to `snprintf(fname, PATHMAX, ...)`. Consider having
`pool_packetfile()` take an explicit size argument so the bound cannot drift from the
caller's declaration.

---

## F5 — `putenv()` handed a stack-local buffer  ·  Medium

`Src/util.c:635-641`, in `parse_yearmonthday()`:

```c
#else  /* end of HAVE_SETENV */
    if (tz) {
      char envstr[LINELEN];
      snprintf(envstr, LINELEN, "TZ=%s", tz);
      putenv(envstr);
```

POSIX `putenv()` does **not** copy its argument — it stores the pointer. Once the function
returns, `environ` holds a pointer into a dead stack frame, so later `getenv("TZ")` or
`tzset()` reads reclaimed memory.

`HAVE_SETENV` is defined **only** by `scripts/build-macos.sh:59` and
`scripts/build-linux.sh:54` — it is absent from `config.h`. So the vintage `./Install`
path, which `README.md` presents as a supported alternative, compiles the broken branch.

**Fix:** make the buffer `static`, or restore `TZ` via `setenv`/`unsetenv` unconditionally.
Also define `HAVE_SETENV` in `config.h` for POSIX targets so the two build scripts are not
the only thing standing between this code and a dangling pointer. The same defect is
duplicated in `Src/tests/test-parse_yearmonthday.c:36-38`.

---

## F6 — Block-request parser mutates the message it is parsing  ·  Low

`Src/remailer/remailer_admin.c:84-88`:

```c
while (buf_getheader(message, field, content) == 0)
  if (bufieq(field, "from"))
    buf_set(from, content);
  else if (bufieq(field, "subject"))
    buf_cat(message, content);      /* appends to the buffer being read */
```

The apparent intent is to make a Subject-borne `destination-block` request visible to the
body scanner that follows, by appending the subject to the end of `message`. It is
memory-safe (`buf_getline` re-reads `buffer->data` each call, so the `buf_append` realloc
cannot dangle) and it terminates. But:

- the mutated buffer is what `logmail()` later archives, so the mbox/log copy of every
  message routed through this path gains a spurious trailing copy of its own Subject
  (`remailer_dispatch.c:170,188`);
- appending to a buffer mid-iteration is fragile enough that any future change to
  `buf_getheader`'s folding rules could turn it into unbounded growth.

**Fix:** scan a scratch buffer. Concatenate the subject and the body into a local
`BUFFER`, run the `destination-block` scan over that, and leave `message` untouched.

---

## F7 — Any body mentioning `destination-block` counts as a block request  ·  Low

`Src/remailer/remailer_admin.c:91` uses `bufifind(line, "destination-block")` — an
unanchored, case-insensitive substring search over every body line. Quoting the
remailer's own help text (which documents the `destination-block` command) is enough to
trip it, and with `AUTOBLOCK` enabled the sender's address is appended to `dest.blk`.
That is a self-inflicted denial of service for ordinary correspondents.

**Fix:** anchor to the start of the line (`bufileft`) after stripping leading whitespace,
and require the command to be the first token.

---

## F8 — `--no-detach` alone does nothing  ·  Low

`Src/remailer/mixremailer.c:74` sets only `nodetach`, and the guard at line 115 requires
one of `readmail/storemail/sendpool/maintain/daemon/keygen`. Verified:

```
$ ./bin/mixremailer --no-detach
Mixmaster 3.0 — remailer server
Usage: ./bin/mixremailer [options]
$ echo $?
0
```

The flag is documented as "Daemon mode, keep terminal attached", so it should imply
daemon mode.

**Fix:** `else if (streq(p, "no-detach")) { nodetach = 1; daemon = 1; }`.

---

## F9 — 140 self-inflicted macro-redefinition warnings  ·  Low

`Src/config.h:82-93` defines `USE_PCRE`, `USE_ZLIB`, `USE_NCURSES`, `HAVE_NCURSES_H`
unconditionally for UNIX/WIN32, while both build scripts also pass them as `-D` flags.
Result: 140 of the build's 397 warnings — 35% — are pure noise that masks real diagnostics.

**Fix:** wrap each in `#ifndef` / `#endif`, or drop the redundant `-D` flags from the two
build scripts. The `#ifndef` guard is preferable since it also fixes third-party builds.

For reference, the remaining warning mix is 130 `-Wpointer-sign` and 117
`-Wdeprecated-declarations` (vintage `byte *`/`char *` churn and OpenSSL 3 low-level DES
deprecations) — both are inherent to the vintage code and reasonable to silence
deliberately with `-Wno-pointer-sign -Wno-deprecated-declarations` rather than leave as
noise. That drops the build to ~10 warnings, all of which are worth reading.

---

## F10 — `clean` target names the new source directory  ·  Low

`Src/Makefile.in`:

```make
clean:
	-rm -f *.o remailer/*.o *.a *.res *~ mixmaster mixremailer mix *.exe remailer test mpgp core gmon.out
```

The bare `remailer` token targets `Src/remailer/` — the entire NEW-2026 module. `rm -f`
refuses to remove a directory and the leading `-` suppresses the error, so nothing breaks
today. But the intended target was the old `remailer` *binary*, and adding `-r` at any
point would delete the new source tree.

**Fix:** drop the bare `remailer` and `test` tokens, or name the binaries explicitly.

---

## F11 — The documented traceability invariant is broken  ·  Process

`README.md` and `MODERNIZATION.md` both promise that
`grep -rE 'MODERNIZED-2026|NEW-2026' .` finds every touched file, and that
`CHANGES-2026.txt` is the flat list of "every change". Six files touched by 2026 commits
satisfy neither:

| File | Marker | In MODERNIZATION.md | Added by |
|------|--------|---------------------|----------|
| `Src/mail.c` | no | no | `e7373c7` |
| `Src/menusend.c` | no | no | `e7373c7` |
| `Src/menustats.c` | no | no | `e7373c7`, `a643a22` |
| `Src/tests/test-parse_yearmonthday.c` | no | no | `1d8b0f2` |
| `scripts/build-linux.sh` | no | no | `eb2c8a8` |
| `AGENTS.md` | no | no | `eb2c8a8` |

This matters more than typical doc drift, because the marker grep is the *stated* method
for telling 2026 work apart from the 2008 baseline. Right now it silently under-reports.

**Fix:** add the markers and update both documents; add a CI step that fails when a file
changed in a commit lacks a marker, so the invariant is enforced rather than asserted.

---

## F12 — No automated tests actually run  ·  Process

`Src/tests/test-parse_yearmonthday.c` is referenced by **no** Makefile, script, or
workflow — verified by grepping `Src/Makefile.in`, the root `Makefile`,
`.github/workflows/macos-build.yml`, and `scripts/*.sh`. It also *copies*
`parse_yearmonthday()` into the test file rather than linking the real symbol, so even if
it were wired up it would test a duplicate, not `Src/util.c`. It does compile and pass
standalone (`cc -DHAVE_SETENV`, prints `OK.`).

The only executed check in the whole tree is the `assert()` at `Src/main.c:67`.

CI (`macos-build.yml`) builds on `macos-latest` only. `scripts/build-linux.sh` is
untested, even though `AGENTS.md` documents it as the primary path for the cloud dev
environment and notes a non-obvious `-fcommon` requirement — exactly the kind of thing
that silently rots without CI.

**Fix:**
1. Add a `test` target to `Src/Makefile.in` that links the test against the real
   `util.o` instead of a copy, and call it from CI.
2. Add an `ubuntu-latest` job running `scripts/build-linux.sh`.
3. Add the F1 regression test (crafted `stats.log`, assert bounded reply) as the first
   real behavioural test.

---

## How it was done

Commits, in the order they landed:

| Commit | Covers |
|--------|--------|
| `94bad8e` | F1 — the security fix, on its own so it can be reviewed or cherry-picked alone |
| `c3dfe29` | F9 + F14 — warning backlog to zero, which is what exposed F14 |
| `ab22199` | F2, F3, F4, F5, F13 — the memory-safety cluster |
| `2813427` | F6, F7, F8, F10 — behavioural cleanups in the remailer module |
| `c892053` | F11, F12 — tests, CI, and the docs refresh |

Clearing the warnings second was deliberate: at 397 warnings there was no way to
tell whether a later change introduced a new one. Everything after `c3dfe29` was
verified against a genuinely quiet build.

One caveat worth recording: an incremental `make` hides warnings in files it does
not recompile. An early "0 warnings" reading here was wrong for that reason — the
real number was 2, in `parsedate.y` and `remailer_server.c`. Warning counts in this
document are all from `./scripts/build-macos.sh clean` followed by a full build.

## Not addressed

Deliberately left alone, all pre-existing and low-consequence for a preservation
fork:

- **`t2_decrypt()` returns only the last packet's status** (`Src/rem.c:43`). A
  message carrying several Type II packets reports success if the final one
  succeeds, even if an earlier one failed. Vintage behaviour; changing it would
  alter what gets logged and replied to.
- **`RAND_bytes()` return values are unchecked** throughout (`Src/random.c`). On
  OpenSSL 3 a failure would leave the buffer untouched rather than filled. The
  DRBG self-seeds and `rnd_error()` aborts when unseeded, so this is theoretical,
  but a remailer is exactly the kind of program that should check.
- **`scripts/build-macos.sh` does not clean stale objects** when flags change, so
  switching to a sanitizer build and back produces confusing link errors until
  `build-macos.sh clean` is run.
- **Unanchored `bufifind` used for `x-loop` detection** (`Src/rem.c:166`) has the
  same shape as F7, but loop detection is intentionally permissive.
