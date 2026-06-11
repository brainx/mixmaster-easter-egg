# Easter Egg: Mixmaster & Bitcoin

*You weren't supposed to find this. But you did. Good.*

---

## Why is this here?

**Bitcoin** (2008) and **Mixmaster 3.0** (2008) are siblings in spirit: both assume **you shouldn't need permission** to transact or communicate. Both lean on **cryptography** instead of institutions. Both annoyed people in suits.

This repo is a **time capsule** — not to run a remailer in anger, but to keep the code readable, buildable, and a little bit mischievous for anyone curious about Cypherpunk history.

---

## A thirty-second history lesson

| Era | What happened |
|-----|---------------|
| **1990s** | Cypherpunks on mailing lists; Type I remailers; "remail@" became a verb. |
| **1999** | Mixmaster 2.9 → 3.0; Type II packet format; OpenPGP integration matures. |
| **2000s** | Public remailer networks, pingers, nym servers — peak operator culture. |
| **2008** | Mixmaster 3.0 release; [Bitcoin whitepaper](https://bitcoin.org/bitcoin.pdf). Parallel timelines. |
| **Today** | Networks faded; **[Katzenpost](https://katzenpost.network/)** carries the mix-net torch. |

Read [`MODERNIZATION.md`](MODERNIZATION.md) for **what we changed** in 2026 (`grep MODERNIZED-2026` in the tree).

Read [`HISTORY`](HISTORY) for the vintage changelog; read [`README`](README) for how operators actually ran it.

---

## How to trigger the egg

1. **Build** (once):

   ```bash
   ./scripts/build-macos.sh
   # or: make build
   ```

   Requires Homebrew: `openssl@3`, `pcre`, `ncurses`. A tracked `Src/parsedate.tab.c` lets you build without bison; delete it only if you change `parsedate.y`.

2. **Spool / config** (once per machine):

   ```bash
   ./scripts/setup-mixdir.sh
   # or: make setup
   export MIXPATH="$PWD/Mix"
   ```

3. **Launch**:

   ```bash
   ./scripts/start-mixmaster.sh
   # or: make start
   ```

   The script finds `bin/mixmaster` or `Src/mixmaster`, sets `MIXPATH` to repo-local `Mix/` unless already exported, and hands you the classic menu client. `Ctrl+C` exits cleanly.

4. **Optional deep cut** — pipe mail into the client like it's 1999:

   ```bash
   your-mua-command | ./Src/mixmaster
   ```

---

## What you should *not* do

- Don't publish remailer keys to `alt.privacy.anon-server` in 2026. Please.
- Don't assume any chain of historical remailers still exists.
- Don't use this for anything that would make your lawyer nervous.

*Look. Learn. Appreciate the mix. Then go read Katzenpost.*

---

<p align="center"><sub>Found the egg? You're one of us.</sub></p>
