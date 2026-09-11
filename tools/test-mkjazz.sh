#!/bin/bash
# Smoke test for tools/mkjazz.sh: build an ISO from a tiny fixture and
# check the key file names landed in it.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/fx"
for f in LEVEL0.000 BLOCKS.000 MENU.000 FONTS.000 PANEL.000 MENUSNG.PSM; do
    printf 'fixture %s\n' "$f" > "$TMP/fx/$f"
done
"$HERE/mkjazz.sh" "$TMP/fx" "$TMP/out.iso"
[ -s "$TMP/out.iso" ] || { echo "FAIL: no iso"; exit 1; }
for f in LEVEL0.000 BLOCKS.000 MENU.000 FONTS.000 PANEL.000 MENUSNG.PSM; do
    grep -aq "$f" "$TMP/out.iso" || { echo "FAIL: $f not in iso directory"; exit 1; }
done
# A folder without the key files must be refused.
mkdir -p "$TMP/bad"; touch "$TMP/bad/README.TXT"
if "$HERE/mkjazz.sh" "$TMP/bad" "$TMP/bad.iso" 2>/dev/null; then echo "FAIL: accepted folder without LEVEL0.000"; exit 1; fi
echo "PASS"
