# Mixmaster 3.0

<p align="center">
  <img src="assets/favicon.svg" width="64" height="64" alt="Mixmaster">
</p>

> *"The packets go in. The packets come out. You can't explain that."*  
> — every Cypherpunk remailer operator, circa 1999

Anonymous remailer software from the golden age of the Cypherpunk mailing list — preserved here as a **GitHub easter egg**. Same era as [Bitcoin](https://bitcoin.org/bitcoin.pdf): privacy infrastructure and peer-to-peer money share the same Cypherpunk DNA.

**2026 modernization** — [`MODERNIZATION.md`](MODERNIZATION.md) · [`CHANGES-2026.txt`](CHANGES-2026.txt) (every touched file) · search: `grep -rE 'MODERNIZED-2026|NEW-2026' .`

---

## What is this?

**Mixmaster 3.0** is classic **Type II (Mixmaster) anonymous remailer** software:

| Component | What it does |
|-----------|--------------|
| **Client** | Sends anonymous mail through Cypherpunk and Mixmaster remailers. OpenPGP-compatible (PGP 2, PGP 5+, GnuPG). Menu UI or CLI. |
| **Remailer** | Runs a mix node: accepts encrypted packets, batches, remixes, forwards. Abuse handling included. |
| **`mixremailer`** | Dedicated server binary (re-architected pipeline in `Src/remailer/`). |

Born from the **Cypherpunk movement** — the same culture that gave us PGP, remailer chains, and the idea that privacy is something you *build*, not something you beg for.

See [`HISTORY`](HISTORY) for the original upstream changelog.

### What we changed (2026 modernization)

**Start here:** [`MODERNIZATION.md`](MODERNIZATION.md) — full list of edited vs vintage files.

Quick find in the tree:

```bash
# Every file we touched (markers in source):
grep -rE 'MODERNIZED-2026|NEW-2026' .

# Or open the flat list:
cat CHANGES-2026.txt
```

| Marker | Meaning | Examples |
|--------|---------|----------|
| `MODERNIZED-2026` | Patched vintage file | `Src/crypto.c`, `Src/rem.c`, `scripts/*.sh` |
| `NEW-2026` | New 2026 code | `Src/remailer/*` |

Everything else is **unmodified Mixmaster 3.0** from 2008.

---

## Quick start (macOS)

**Prerequisites:** Xcode Command Line Tools (`xcode-select --install`), plus Homebrew libraries:

```bash
brew install openssl zlib pcre ncurses
```

**Build, set up spool, and run** (recommended):

```bash
./scripts/build-macos.sh
./scripts/setup-mixdir.sh
./scripts/start-mixmaster.sh
```

Or use the Makefile shortcuts:

```bash
make build && make setup && make start
```

`setup-mixdir.sh` copies `conf/` templates into a local `Mix/` spool (or `$MIXPATH`). `start-mixmaster.sh` runs the ncurses menu client in the foreground — press `Ctrl+C` to exit cleanly.

**Alternative:** the vintage interactive installer still works:

```bash
./Install
```

Invoke the client directly once `bin/mixmaster` or `Src/mixmaster` exists:

```bash
export MIXPATH="$PWD/Mix"   # after setup-mixdir.sh
cd Src && ./mixmaster       # menu UI
./mixmaster --help          # CLI options — see mixmaster(1)
```

Pingers and live remailer stats were a thing of another era. For historical context on keys and lists, see the original [`README`](README) § Installation and § Using the remailer client.

**Remailer server** (after `make setup`):

```bash
./scripts/start-remailer.sh           # maintenance tick
./scripts/start-remailer.sh --flush   # force pool send
./scripts/start-remailer.sh --daemon  # background daemon
```

Architecture notes → [`Src/remailer/ARCHITECTURE.md`](Src/remailer/ARCHITECTURE.md)

---

## Easter egg?

Yes — you just found **Mixmaster**.

Mixmaster 3.0 and the [Bitcoin whitepaper](https://bitcoin.org/bitcoin.pdf) both landed in **2008**. Different problems, same Cypherpunk bet: **cryptography can change who gets to speak, spend, and stay anonymous**.

Dig deeper → [`EASTER_EGG.md`](EASTER_EGG.md)

---

## Historical software disclaimer

**This is museum-grade privacy tooling.**

- Type I (Cypherpunk) remailers are **deprecated**; the original docs discourage them.
- The public remailer network this client was built for is **largely gone**.
- Crypto and threat models have moved on; do **not** rely on this for real-world anonymity today.
- Build on modern macOS may require patience, vendored deps, or `./Install` answering `y` to compile libraries from `Src/`.

Treat this repo as **source preservation and curiosity** — not production infrastructure.

---

## Modern alternatives

If you want anonymous messaging infrastructure that people actually run in 2026:

- **[Katzenpost](https://katzenpost.network/)** — modern mix-network design, active development, successor spirit to the Mixmaster era.
- **[Echolot](https://www.palfrader.org/echolot/)** — pinger tooling (historical companion to Mixmaster ops).
- **Tor / I2P** — different threat model, still relevant for metadata resistance.

---

## Original documentation

This README is the friendly front door. The authoritative vintage docs remain:

| File | Contents |
|------|----------|
| [`README`](README) | Full installation, client & remailer usage, security notes |
| [`mixmaster.1`](mixmaster.1) | Manual page |
| [`Install`](Install) | Build & configure script |
| [`COPYRIGHT`](COPYRIGHT) | Mixmaster License Agreement |
| [`idea.txt`](idea.txt) | IDEA™ commercial-use notice |

---

## License

See [`COPYRIGHT`](COPYRIGHT) for the full Mixmaster License Agreement. A pointer file lives at [`LICENSE`](LICENSE).

---

<p align="center">
  <sub>Mixmaster 3.0 · Cypherpunk heritage · preserved for the curious</sub>
</p>
