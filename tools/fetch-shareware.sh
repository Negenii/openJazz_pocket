#!/bin/bash
# Download Jazz Jackrabbit shareware 1.3 (freely distributable) and unpack
# the game files into <dest> (default: ./jazz). Used for development and
# for the first jazz.iso.
set -euo pipefail
DEST="${1:-jazz}"
URL="http://www.classicdosgames.com/files/games/epic/1jazz13.zip"
SHA_ZIP="3e04fd2b756bdad2d96d46fddaef5567b18cb7abd9ed238afe357e7d17400e8a"
SHA_EXE="c9278a60c473b78eafee35136db5473afee08f7c2f38edb7b7376dc767038ef4"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

sha256() { shasum -a 256 "$1" 2>/dev/null | awk '{print $1}' || sha256sum "$1" | awk '{print $1}'; }

curl -fsSL -o "$TMP/1jazz13.zip" "$URL"
[ "$(sha256 "$TMP/1jazz13.zip")" = "$SHA_ZIP" ] || { echo "checksum mismatch for 1jazz13.zip" >&2; exit 1; }
unzip -oq "$TMP/1jazz13.zip" -d "$TMP"
[ "$(sha256 "$TMP/JAZZ1X.EXE")" = "$SHA_EXE" ] || { echo "checksum mismatch for JAZZ1X.EXE" >&2; exit 1; }
# JAZZ1X.EXE is a PKSFX self-extractor; unzip reads it (warns about the stub).
mkdir -p "$DEST"
unzip -oq "$TMP/JAZZ1X.EXE" -d "$DEST" 2>/dev/null || true
[ -f "$DEST/LEVEL0.000" ] || { echo "extraction failed: LEVEL0.000 missing" >&2; exit 1; }
echo "shareware unpacked to $DEST ($(ls "$DEST" | wc -l | tr -d ' ') files)"
