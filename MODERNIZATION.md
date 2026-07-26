# Modernization (2026)

> **We modernized this repo in June 2026.** Everything we touched is tagged in source with `MODERNIZED-2026` (patched vintage files) or `NEW-2026` (new remailer module).  
> **Flat file list:** [`CHANGES-2026.txt`](CHANGES-2026.txt)

This document lists **every change** made to the vintage Mixmaster 3.0 tree so you can tell our work apart from the 2008 codebase.

## Find changes instantly

```bash
# Patched vintage files:
grep -r "MODERNIZED-2026" Src/ scripts/

# New 2026 module (remailer/):
grep -r "NEW-2026" Src/remailer/

# Everything we touched (either marker):
grep -rE "MODERNIZED-2026|NEW-2026" .
```

---

## Modified vintage C files

These files existed in Mixmaster 3.0; we edited them for **macOS Apple Silicon** and **OpenSSL 3**.

| File | What we changed |
|------|-----------------|
| [`Src/config.h`](Src/config.h) | Platform detection; `USE_PCRE`, `USE_ZLIB`, `USE_NCURSES` on Unix (`#ifndef`-guarded); `HAVE_SETENV` on POSIX |
| [`Src/crypto.h`](Src/crypto.h) | OpenSSL 3 API compat and legacy DES macros |
| [`Src/crypto.c`](Src/crypto.c) | Opaque RSA API, `RSA_generate_key_ex`, DES schedules; key-reader bounds and leak fixes; `pk_encrypt`/`pk_decrypt` no longer use a rejected key |
| [`Src/pgpdata.c`](Src/pgpdata.c) | OpenSSL 3 RSA/DSA/DH accessors |
| [`Src/pgpget.c`](Src/pgpget.c) | DES calls for OpenSSL 3; MDC ciphertext length fix |
| [`Src/pgpcreat.c`](Src/pgpcreat.c) | DES calls for OpenSSL 3 |
| [`Src/pgpdb.c`](Src/pgpdb.c) | Free the key-id buffer in `pgp_rkeylist()` |
| [`Src/random.c`](Src/random.c) | RNG init; no `RAND_cleanup()` on OpenSSL 3+ |
| [`Src/pool.c`](Src/pool.c) | `snprintf` instead of `sprintf` for paths |
| [`Src/rem.c`](Src/rem.c) | Removed monolithic `mix_decrypt()` — see `Src/remailer/`; `snprintf` in `pool_packetfile()`; format-string fix in `logmail()` |
| [`Src/rem1.c`](Src/rem1.c) | Indentation fix in `isline()` |
| [`Src/main.c`](Src/main.c) | Simpler `--about` output |
| [`Src/mail.c`](Src/mail.c) | SMTP reply check was `!line->data[0] == '2'`, always false |
| [`Src/mix.c`](Src/mix.c) | `const`-qualify `mix_init()`; drop always-true array-vs-`NULL` tests |
| [`Src/mix.h`](Src/mix.h) | `mix_init()` takes `const char *` |
| [`Src/menu.c`](Src/menu.c) | Drop unused fields in `sortrel()` |
| [`Src/menusend.c`](Src/menusend.c) | Local news posting dropped the message; `snprintf` for the edit path |
| [`Src/menustats.c`](Src/menustats.c) | `printw("%s", ...)` for config-derived strings |
| [`Src/stats.c`](Src/stats.c) | Clamp the per-day summary to the 80 days the counters actually cover |
| [`Src/util.c`](Src/util.c) | `putenv()` buffer lifetime in `parse_yearmonthday()`; empty-string test in `mixfile()` |
| [`Src/Makefile.in`](Src/Makefile.in) | Build `mixremailer`; link remailer objects; `check`/`tests` targets; warning flags; rename dead `remailer` target |
| [`Src/parsedate.y`](Src/parsedate.y) | `(void) s` instead of self-assignment |

---

## New code (2026)

| Path | Purpose |
|------|---------|
| [`Src/remailer/`](Src/remailer/) | Remailer server pipeline (classify → dispatch → pool) |
| [`Src/remailer/mixremailer.c`](Src/remailer/mixremailer.c) | Standalone server binary |
| [`Src/parsedate.tab.c`](Src/parsedate.tab.c) | Pre-generated parser (build without bison) |
| [`Src/tests/`](Src/tests/) | Regression tests — `make -C Src check` |

Details: [`Src/remailer/ARCHITECTURE.md`](Src/remailer/ARCHITECTURE.md)

### Tests

`make -C Src check` builds and runs each test against the real objects, not a
copy of the code under test:

| Test | Covers |
|------|--------|
| [`test-parse_yearmonthday.c`](Src/tests/test-parse_yearmonthday.c) | Date parsing, and that `TZ` is restored afterwards |
| [`test-crypto-keys.c`](Src/tests/test-crypto-keys.c) | v2 RSA key round trip; malformed keys rejected, not crashed on |
| [`test-stats-window.c`](Src/tests/test-stats-window.c) | The 80-day clamp in `stats()` |

Run them under a sanitizer to check the out-of-bounds claims directly — CI does
this on every push:

```bash
make -C Src clean
make -C Src OPT="-g -O0 -fsanitize=address -fno-omit-frame-pointer" \
            LDFLAGS="-lz -fsanitize=address" check
```

---

## New scripts & tooling

| File | Purpose |
|------|---------|
| [`scripts/build-macos.sh`](scripts/build-macos.sh) | Build with Homebrew (openssl, pcre, ncurses) |
| [`scripts/build-linux.sh`](scripts/build-linux.sh) | Build on Linux (system libs, requires `-fcommon`) |
| [`scripts/setup-mixdir.sh`](scripts/setup-mixdir.sh) | Initialize `Mix/` spool from `conf/mix.cfg.ex` |
| [`scripts/start-mixmaster.sh`](scripts/start-mixmaster.sh) | Run client |
| [`scripts/start-remailer.sh`](scripts/start-remailer.sh) | Run server |
| [`Makefile`](Makefile) | `make build`, `setup`, `start`, `remailer` |
| [`.github/workflows/build.yml`](.github/workflows/build.yml) | CI: macOS, Linux, and a sanitizer job — all run `make -C Src check` |
| [`AGENTS.md`](AGENTS.md) | Notes for coding agents / cloud dev environments |

---

## New docs & assets

| File | Purpose |
|------|---------|
| [`README.md`](README.md) | GitHub front door |
| [`EASTER_EGG.md`](EASTER_EGG.md) | Easter egg — Mixmaster & Bitcoin (2008) |
| [`LICENSE`](LICENSE) | Points to [`COPYRIGHT`](COPYRIGHT) |
| [`assets/favicon.svg`](assets/favicon.svg) | Repo icon |
| [`mixmaster.1`](mixmaster.1) | Updated `--about` / HISTORY sections |

---

## Unchanged (vintage)

All other files under `Src/` — e.g. `chain.c`, `rem2.c`, `pgp.c`, `buffers.c`, the
remaining menu sources, `pool.c` mail logic — are **original Mixmaster 3.0**
unless they contain `MODERNIZED-2026`.

Vintage docs: [`README`](README), [`HISTORY`](HISTORY), [`Install`](Install), [`COPYRIGHT`](COPYRIGHT).

---

## Build output (not in git)

- `bin/mixmaster`, `bin/mixremailer`
- `Mix/` spool directory
- `Src/Makefile` (generated by `build-macos.sh`)

---

## Flat file list

See [`CHANGES-2026.txt`](CHANGES-2026.txt) — one path per line, grouped by category.
