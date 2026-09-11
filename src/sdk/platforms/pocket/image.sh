#!/bin/bash
#------------------------------------------------------------------------------
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileType: SOURCE
# SPDX-FileCopyrightText: (c) 2026, ThinkElastic <Think@Elastic.com>
#------------------------------------------------------------------------------
#
# openfpgaOS SDK — Pocket platform: assemble ONE app's deliverable.
#
# For a custom core (dist_dir given) this builds the standalone APF SD
# tree under build/pocket/<app>/.  For an SDK demo app (no dist_dir) it
# is a no-op — those bundle into the shared demo core via
# `make build CORE=sdk`.  Called by the core-Makefile template and the
# SDK Makefile APP hook; adding a target = add platforms/<t>/image.sh.
#
# Usage: image.sh <app> <elf> <sdk_root> <dist_dir|""> [assets...]
#
set -e
APP="$1"; ELF="$2"; ROOT="$3"; DIST="$4"; shift 4 2>/dev/null || true
ASSETS=("$@")

# No per-core dist → this is an SDK demo app; the Pocket demo core bundles
# it elsewhere, nothing standalone to assemble here.
{ [ -n "$DIST" ] && [ -d "$DIST" ]; } || exit 0

OUT="$ROOT/build/pocket/$APP"
RT="$ROOT/runtime/pocket"   # pocket-specific artifacts (bank.ofsf stays at runtime/)
rm -rf "$OUT"
mkdir -p "$(dirname "$OUT")"
cp -r "$DIST" "$OUT"

# Resolve the single Cores/<id>/ and Assets/<platform>/common dirs the APF
# dist tree provides.  Build the paths explicitly (no "ls | head" string
# concat — an empty glob there would have produced a relative "common"
# written into the CWD).
CORE_DIR=""; for d in "$OUT"/Cores/*/;  do [ -d "$d" ] && { CORE_DIR="$d"; break; }; done
ASSET_PLAT=""; for d in "$OUT"/Assets/*/; do [ -d "$d" ] && { ASSET_PLAT="$d"; break; }; done
[ -n "$CORE_DIR" ]   || { echo "Error: $DIST has no Cores/<id>/ — not an APF core tree"; exit 1; }
[ -n "$ASSET_PLAT" ] || { echo "Error: $DIST has no Assets/<platform>/"; exit 1; }
ASSET_DIR="${ASSET_PLAT}common"
mkdir -p "$ASSET_DIR"

# Runtime FPGA artifacts (each copied independently so a missing one is
# reported, not silently swallowed alongside the others).  The bitstream
# is variant-named (os25.rbf_r / os30.rbf_r); copy exactly
# the one this core's core.json points at — the single source of truth set
# at scaffold time (customize.sh --variant).  VARIANT=os20 (env) overrides
# per-assembly: the STAGED core.json is rewritten so the core boots that
# bitstream, while dist/ keeps the scaffold default.
RBF=$(grep -o '"filename"[[:space:]]*:[[:space:]]*"[^"]*"' "$CORE_DIR/core.json" 2>/dev/null \
      | head -1 | sed 's/.*"\([^"]*\)"$/\1/')
[ -n "$RBF" ] || RBF="bitstream.rbf_r"
if [ -n "$VARIANT" ] && [ "${VARIANT}.rbf_r" != "$RBF" ]; then
    NEW_RBF="${VARIANT}.rbf_r"
    [ -f "$RT/$NEW_RBF" ] || { echo "Error: runtime/$NEW_RBF missing (VARIANT=$VARIANT)"; exit 1; }
    sed -i "s/\"$RBF\"/\"$NEW_RBF\"/" "$CORE_DIR/core.json"
    RBF="$NEW_RBF"
    echo "  variant override: core boots $RBF"
fi

# Publish-manifest guard: openfpgaOS `make sdk` records md5s of the runtime
# set it publishes atomically (runtime/MANIFEST).  A staged artifact that no
# longer matches means a hand-mixed runtime set (e.g. a lone os.bin copied
# in) -- exactly the firmware/bitstream pairing that boots into a blank
# screen, so refuse to assemble the image.  Absent MANIFEST = no check
# (pre-manifest publishes, or deliberately deleted to override).
MANIFEST="$ROOT/runtime/MANIFEST"
if [ -f "$MANIFEST" ]; then
    for f in "pocket/$RBF" "pocket/os.bin" "pocket/loader.bin"; do
        [ -f "$ROOT/runtime/$f" ] || continue
        want=$(awk -v p="./$f" '$2==p{print $1}' "$MANIFEST")
        [ -n "$want" ] || continue
        have=$(md5sum "$ROOT/runtime/$f" | awk '{print $1}')
        if [ "$want" != "$have" ]; then
            echo "Error: runtime/$f does not match runtime/MANIFEST — hand-mixed runtime set."
            echo "       Re-publish atomically ('make sdk DEST=…' in openfpgaOS), or delete"
            echo "       runtime/MANIFEST to override deliberately."
            exit 1
        fi
    done
fi
for f in "$RBF" loader.bin; do
    [ -f "$RT/$f" ] && cp "$RT/$f" "$CORE_DIR" || echo "  warn: runtime/$f missing (run 'make sdk VARIANT=… DEST=…' in openfpgaOS)"
done
[ -f "$RT/os.bin" ] && cp "$RT/os.bin" "$ASSET_DIR/" || echo "  warn: runtime/os.bin missing"

# Soundfonts (bank.ofsf + any game .ofsf) — target-agnostic, at runtime/ root.
for s in "$ROOT"/runtime/*.ofsf; do [ -f "$s" ] && cp "$s" "$ASSET_DIR/"; done

# Kernel ELF + app data files.
cp "$ELF" "$ASSET_DIR/$APP.elf"
for a in "${ASSETS[@]}"; do [ -f "$a" ] && cp "$a" "$ASSET_DIR/"; done

echo "Ready: build/pocket/$APP/"
