#!/usr/bin/env bash
# Author: BrainX
# Date: 2026-06-17
# Description: Build Mixmaster 3.0 on Linux (Debian/Ubuntu) using system libraries.
# Usage:
#   ./scripts/build-linux.sh
#   ./scripts/build-linux.sh clean
#
# Notes:
#   Requires dev packages: libssl-dev libpcre3-dev libncurses-dev zlib1g-dev
#   (PCRE1 / libpcre3-dev is required — the vintage code uses the old pcre.h API.)
#   bison is NOT needed: Src/parsedate.tab.c is tracked for bison-free builds.
#   -fcommon is required: legacy code declares globals in a shared header, which
#   modern GCC/Clang (-fno-common default) would otherwise reject at link time.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/Src"
BIN="${ROOT}/bin"

cleanup() {
  local code=$?
  if [[ $code -ne 0 ]]; then
    echo "build-linux.sh: interrupted or failed (exit $code)" >&2
  fi
  exit "$code"
}
trap cleanup INT TERM

check_headers() {
  local missing=()
  [[ -f /usr/include/openssl/ssl.h ]] || missing+=("libssl-dev")
  [[ -f /usr/include/pcre.h ]]        || missing+=("libpcre3-dev")
  [[ -f /usr/include/ncurses.h || -f /usr/include/curses.h ]] || missing+=("libncurses-dev")
  [[ -f /usr/include/zlib.h ]]        || missing+=("zlib1g-dev")
  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "Missing dev libraries. Install with:" >&2
    echo "  sudo apt-get install -y ${missing[*]}" >&2
    exit 1
  fi
}

if [[ "${1:-}" == "clean" ]]; then
  make -C "$SRC" clean 2>/dev/null || true
  rm -f "$SRC/Makefile" "$BIN/mixmaster" "$BIN/mixremailer"
  echo "Clean complete."
  exit 0
fi

check_headers

INC="-I/usr/include"
DEF="-DUSE_ZLIB -DUSE_PCRE -DUSE_NCURSES -DHAVE_NCURSES_H -DUSE_AES -DHAVE_SETENV -DSPOOL='\"${ROOT}\"' -fcommon"
LIBS=""
LDFLAGS="-lcrypto -lpcre -lncurses -lz"

mkdir -p "$BIN"
cd "$SRC"

HOST="$(hostname -s 2>/dev/null || echo linux)"
{
  echo "# Makefile generated on ${HOST} $(date)"
  sed -e "s#%MIXDIR##" \
      -e "s#%LIBS#${LIBS}#" \
      -e "s#%LDFLAGS#${LDFLAGS}#" \
      -e "s#%INC#${INC}#" \
      -e "s#%DEF#${DEF}#" < Makefile.in
  echo "CC = cc"
} > Makefile

echo "Building mixmaster + mixremailer in ${SRC}..."
make mixmaster mixremailer

install -m 755 mixmaster "${BIN}/mixmaster"
install -m 755 mixremailer "${BIN}/mixremailer"
echo "Success: ${BIN}/mixmaster ($(file -b "${BIN}/mixmaster"))"
echo "Success: ${BIN}/mixremailer ($(file -b "${BIN}/mixremailer"))"
