#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
# Build jazz.iso for the OpenJazz Pocket core from a folder with the
# Jazz Jackrabbit 1 files (shareware or full).
#
#   tools/mkjazz.sh <jazz-folder> [jazz.iso]
#
# Uses xorriso or mkisofs when on PATH, else hdiutil (macOS), else xorriso
# inside the SDK build container. ISO9660 level 1: the game files are 8.3
# already; the kernel's iso9660 driver reads the primary descriptor.
set -euo pipefail
SRC="${1:?usage: mkjazz.sh <jazz-folder> [jazz.iso]}"
OUT="${2:-jazz.iso}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

for f in LEVEL0.000 MENU.000 FONTS.000 PANEL.000; do
    [ -f "$SRC/$f" ] || { echo "mkjazz: $SRC does not look like a Jazz Jackrabbit folder ($f missing)" >&2; exit 1; }
done

OUT_ABS="$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
SRC_ABS="$(cd "$SRC" && pwd)"

if command -v xorriso >/dev/null 2>&1; then
    xorriso -as mkisofs -quiet -iso-level 1 -V JAZZ -o "$OUT_ABS" "$SRC_ABS"
elif command -v mkisofs >/dev/null 2>&1; then
    mkisofs -quiet -iso-level 1 -V JAZZ -o "$OUT_ABS" "$SRC_ABS"
elif command -v hdiutil >/dev/null 2>&1; then
    hdiutil makehybrid -quiet -iso -joliet -default-volume-name JAZZ -o "$OUT_ABS" "$SRC_ABS"
else
    SDK_IMG=openfpgaos-openjazz SDK_DOCKERFILE="$ROOT/tools/docker/Dockerfile.openjazz" \
      bash "$ROOT/tools/sdk-container.sh" xorriso -as mkisofs -quiet -iso-level 1 -V JAZZ -o "$OUT_ABS" "$SRC_ABS"
fi
echo "mkjazz: wrote $OUT ($(du -h "$OUT" | cut -f1))"
