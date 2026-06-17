# AGENTS.md

Mixmaster 3.0 — vintage Type II anonymous remailer (C). Two binaries share a
`Mix/` spool: `mixmaster` (client + legacy remailer) and `mixremailer` (server).
See `README.md`, `MODERNIZATION.md`, and `Src/remailer/ARCHITECTURE.md` for design.

## Cursor Cloud specific instructions

The cloud VM is **Linux (Ubuntu)**, but the repo's documented build path
(`scripts/build-macos.sh`, `make build`, CI) targets **macOS/Homebrew**. Use the
Linux path instead:

- **Build:** `./scripts/build-linux.sh` (added for Linux; mirrors the macOS
  script). Outputs `bin/mixmaster` and `bin/mixremailer`. `./scripts/build-linux.sh clean` cleans.
- **System libs** (installed by the startup update script): `libssl-dev`,
  `libpcre3-dev` (PCRE **1** — the vintage code uses the old `pcre.h` API, not
  pcre2), `libncurses-dev`, `zlib1g-dev`.
- **`-fcommon` is mandatory.** Legacy globals are declared (not just `extern`)
  in a shared header; modern GCC/Clang default to `-fno-common` and would fail
  linking with "multiple definition" errors. `build-linux.sh` adds `-fcommon`.
- **bison is NOT needed:** `Src/parsedate.tab.c` is committed for bison-free builds.

### Run / test (from repo root, after build)

Always export the spool path first: `export MIXPATH="$PWD/Mix"`.

- **Spool setup:** `./scripts/setup-mixdir.sh` seeds `Mix/` from `conf/`
  (config, keyrings, lists). Re-seed config with `SETUP_FORCE_CFG=1`.
- **Client:** `./bin/mixmaster` (no args) opens the ncurses menu; needs a TTY.
  CLI flags skip the menu (`--version`, `--help`, `-T` list remailers,
  `-l <chain> -t <addr>` encrypt, `-S` flush pool, `-G` generate keys).
- **Server smoke test (matches CI):** `MIXPATH="$PWD/Mix" ./bin/mixremailer -M`.
  The first `-M`/`-G` auto-generates the remailer's RSA key (OpenSSL 3 path).
- **Unit test:** `cc -DHAVE_SETENV -include string.h -o /tmp/t Src/tests/test-parse_yearmonthday.c && /tmp/t`
  (the `-include string.h` works around a missing include the source omits; do
  not edit the source). The same function is asserted at every `mixmaster` startup.

### Non-obvious runtime gotchas

- **No MTA on the VM.** `mix.cfg` defaults `SENDMAIL` to `/usr/lib/sendmail -t`,
  which does not exist here. For local delivery tests, point `SENDMAIL` at a
  capture script in `Mix/mix.cfg` (env `SENDMAIL` does **not** override the
  config value). Outbound mail is then written to a file instead of sent.
- **Pool batching holds single messages.** With default `POOLSIZE`/`RATE`, one
  pooled message is held for anonymity and won't flush. Force it for testing by
  setting `POOLSIZE 0` / `RATE 100` in `mix.cfg` (or per the `mixmaster(1)`
  `--POOLSIZE=0 --RATE=100` overrides).
- **Self-addressed messages loop back:** sending to the remailer's own
  `REMAILERADDR` stores the packet in the pool (prefix `i*`) for local
  processing rather than mailing it; feed it to `./bin/mixremailer -R < <file>`
  to decrypt the next hop.
- **Bundled sample keys (2007) are expired** — the `Key ... has expired` and
  `Statistics are older than one day` warnings are benign. For a working hop,
  publish the locally generated key: `cat Mix/key.txt >> Mix/pubring.mix`.
- `Mix/`, `bin/`, `Src/Makefile`, secrets/keyrings are gitignored runtime
  artifacts — safe to delete and regenerate.
