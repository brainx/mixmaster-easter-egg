#!/usr/bin/env bash
# Date: Thu Jun 11 03:11:56 CEST 2026
# Description: Initialize a local Mix spool directory from conf/ templates
# MODERNIZED-2026  see MODERNIZATION.md
# Usage:
#   ./scripts/setup-mixdir.sh
#   MIXPATH=/path/to/Mix ./scripts/setup-mixdir.sh
#
# Notes:
#   Default spool: repo-local Mix/ (override with MIXPATH env var)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONF="${ROOT}/conf"
MIXDEST="${MIXPATH:-${ROOT}/Mix}"

copy_if_missing() {
  local src=$1
  local dest=$2
  if [[ -f "${src}" && ! -f "${dest}" ]]; then
    cp "${src}" "${dest}"
    echo "  + $(basename "${dest}")"
  fi
}

echo "Mixmaster 3.0 — spool setup"
echo "Destination: ${MIXDEST}"
echo ""

mkdir -p "${MIXDEST}"

echo "Creating subdirectories..."
for sub in pool requests; do
  mkdir -p "${MIXDEST}/${sub}"
  echo "  + ${sub}/"
done

echo ""
echo "Copying conf/ templates (existing files kept)..."
for f in mlist.txt pubring.mix rlist.txt pubring.asc dest.alw; do
  copy_if_missing "${CONF}/${f}" "${MIXDEST}/${f}"
done

for blk in "${CONF}"/*.blk; do
  [[ -f "${blk}" ]] || continue
  copy_if_missing "${blk}" "${MIXDEST}/$(basename "${blk}")"
done

if [[ -f "${CONF}/mix.cfg.ex" ]]; then
  if [[ ! -f "${MIXDEST}/mix.cfg" ]] || [[ "${SETUP_FORCE_CFG:-}" == "1" ]]; then
    cp "${CONF}/mix.cfg.ex" "${MIXDEST}/mix.cfg"
    echo "  + mix.cfg (from conf/mix.cfg.ex)"
  else
    echo "  = mix.cfg (already present; SETUP_FORCE_CFG=1 to refresh from mix.cfg.ex)"
  fi
else
  echo "  ! warning: conf/mix.cfg.ex not found" >&2
fi

echo ""
echo "Spool ready at: ${MIXDEST}"
echo ""
echo "Next steps:"
echo "  1. Build (macOS):     ./scripts/build-macos.sh"
echo "  2. Export spool path: export MIXPATH=\"${MIXDEST}\""
echo "  3. Launch client:     ./scripts/start-mixmaster.sh"
echo ""
echo "Or use:  make build && make setup && make start"
