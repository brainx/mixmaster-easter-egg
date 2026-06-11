#!/usr/bin/env bash
# Date: Thu Jun 11 03:20:00 CEST 2026
# Description: Run Mixmaster remailer server (maintenance tick or daemon)
# MODERNIZED-2026  see MODERNIZATION.md
# Usage:
#   ./scripts/start-remailer.sh              # one maintenance tick (-M)
#   ./scripts/start-remailer.sh --daemon     # background daemon (-D)
#   ./scripts/start-remailer.sh --flush      # force pool send (-S)
#
# Notes:
#   Requires bin/mixremailer from ./scripts/build-macos.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT}/bin/mixremailer"
MODE=(--maintain)

if [[ "${1:-}" == "--daemon" ]]; then
  MODE=(--daemon --no-detach)
elif [[ "${1:-}" == "--flush" ]]; then
  MODE=(--send)
elif [[ -n "${1:-}" ]]; then
  MODE=("$@")
fi

if [[ ! -x "${BIN}" ]]; then
  echo "mixremailer not found. Run: ./scripts/build-macos.sh" >&2
  exit 1
fi

export MIXPATH="${MIXPATH:-${ROOT}/Mix}"
cd "${ROOT}"

echo "Mixmaster remailer server"
echo "Binary:  ${BIN}"
echo "MIXPATH: ${MIXPATH}"
echo "Mode:    ${MODE[*]}"
echo ""

exec "${BIN}" "${MODE[@]}"
