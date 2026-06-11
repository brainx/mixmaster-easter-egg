#!/usr/bin/env bash
# Date: Thu Jun 11 03:08:18 CEST 2026
# Description: Build Mixmaster 3.0 on macOS (Apple Silicon) using Homebrew libraries.
# MODERNIZED-2026  see MODERNIZATION.md
# Usage:
#   ./scripts/build-macos.sh
#   ./scripts/build-macos.sh clean
#
# Notes:
#   Requires Homebrew packages: openssl@3, pcre, ncurses; zlib from the macOS SDK.
#   Installs missing dependencies via brew when possible.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${ROOT}/Src"
BIN="${ROOT}/bin"
HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"

cleanup() {
  local code=$?
  if [[ $code -ne 0 ]]; then
    echo "build-macos.sh: interrupted or failed (exit $code)" >&2
  fi
  exit "$code"
}
trap cleanup INT TERM

require_brew() {
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required. Install from https://brew.sh/" >&2
    exit 1
  fi
}

ensure_pkg() {
  local pkg=$1
  if brew list "$pkg" >/dev/null 2>&1; then
    return 0
  fi
  echo "Installing ${pkg}..."
  if [[ "$pkg" == "pcre" ]]; then
    brew install --build-from-source pcre
  else
    brew install "$pkg"
  fi
}

require_brew
for pkg in openssl@3 pcre ncurses; do
  ensure_pkg "$pkg"
done

OPENSSL_PREFIX="$(brew --prefix openssl@3)"
PCRE_PREFIX="$(brew --prefix pcre)"
NCURSES_PREFIX="$(brew --prefix ncurses)"

INC="-I${OPENSSL_PREFIX}/include -I${PCRE_PREFIX}/include -I${NCURSES_PREFIX}/include"
DEF="-DUSE_ZLIB -DUSE_PCRE -DUSE_NCURSES -DHAVE_NCURSES_H -DUSE_AES -DHAVE_SETENV -DSPOOL='\"${ROOT}\"'"
# Makefile.in lists $(LIBS) as prerequisites — use archive paths, not -L/-l flags.
LIBS="${OPENSSL_PREFIX}/lib/libcrypto.a ${PCRE_PREFIX}/lib/libpcre.a ${NCURSES_PREFIX}/lib/libncurses.a"
LDFLAGS="-lz"

if [[ "${1:-}" == "clean" ]]; then
  make -C "$SRC" clean 2>/dev/null || true
  rm -f "$SRC/Makefile" "$BIN/mixmaster" "$BIN/mixremailer"
  # Keep tracked Src/parsedate.tab.c for bison-free clones
  echo "Clean complete."
  exit 0
fi

mkdir -p "$BIN"
cd "$SRC"

if [[ ! -f parsedate.tab.c ]]; then
  if ! command -v bison >/dev/null 2>&1; then
    echo "parsedate.tab.c missing and bison not found; install Xcode CLT or: brew install bison" >&2
    exit 1
  fi
  echo "Generating parsedate.tab.c from parsedate.y (expect shift/reduce conflicts)..."
  bison -y parsedate.y -o parsedate.tab.c
fi

HOST="$(hostname -s 2>/dev/null || echo mac)"
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
