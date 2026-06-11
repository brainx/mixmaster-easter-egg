#!/usr/bin/env bash
# Date: 2026-06-11
# Description: Launch Mixmaster 3.0 client from bin/ or Src/ with clean SIGINT exit
# MODERNIZED-2026  see MODERNIZATION.md
# Usage:
#   ./scripts/start-mixmaster.sh [mixmaster-args...]
#
# Notes:
#   - Requires a prior build (./scripts/build-macos.sh or ./Install)
#   - Sets MIXPATH to repo-local Mix/ unless already exported
#   - Runs in foreground so the ncurses menu works; Ctrl+C exits cleanly

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIX_BIN=""

find_binary() {
  local candidate
  for candidate in "${ROOT}/bin/mixmaster" "${ROOT}/Src/mixmaster"; do
    if [[ -x "${candidate}" ]]; then
      MIX_BIN="${candidate}"
      return 0
    fi
  done
  return 1
}

if ! find_binary; then
  echo "Mixmaster binary not found." >&2
  echo "Build first:  cd \"${ROOT}\" && ./scripts/build-macos.sh" >&2
  echo "Expected:     bin/mixmaster  or  Src/mixmaster" >&2
  exit 1
fi

export MIXPATH="${MIXPATH:-${ROOT}/Mix}"
cd "${ROOT}"

echo "Mixmaster 3.0"
echo "Binary:  ${MIX_BIN}"
echo "MIXPATH: ${MIXPATH}"
echo "Press Ctrl+C for clean exit."
echo ""

exec "${MIX_BIN}" "$@"
