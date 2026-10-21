#!/usr/bin/env bash
# Prisma FX - SPDX-License-Identifier: BSD-3-Clause
#
# Renders every plugin in the bundle through the ofxtest host at 8, 16 and
# 32 bits and at half resolution, checking each time that a tiled render
# matches the full frame render and that the output has no NaN or Inf.
# Needs: OFXTEST (host binary), BUNDLE_DIR, OUT_DIR. Run through ctest.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT_DIR"
python3 "$here/make_test_image.py" "$OUT_DIR/in.ppm" 640 360
python3 "$here/make_test_image.py" "$OUT_DIR/in_half.ppm" 320 180

plugins=$("$OFXTEST" --bundle-dir "$BUNDLE_DIR" --list | cut -d' ' -f1)
[ -n "$plugins" ] || { echo "no plugins found in $BUNDLE_DIR"; exit 1; }

fail=0
for id in $plugins; do
  name=${id##*.}
  for depth in float short byte; do
    if ! "$OFXTEST" --bundle-dir "$BUNDLE_DIR" --plugin "$id" --in "$OUT_DIR/in.ppm" \
         --out "$OUT_DIR/$name-$depth.ppm" --depth "$depth" --time 3 --tiles 3 --check-tiles > "$OUT_DIR/$name-$depth.log" 2>&1; then
      echo "FAIL $id ($depth)"; cat "$OUT_DIR/$name-$depth.log"; fail=1
    else
      echo "ok   $id ($depth) $(cat "$OUT_DIR/$name-$depth.log")"
    fi
  done
  if ! "$OFXTEST" --bundle-dir "$BUNDLE_DIR" --plugin "$id" --in "$OUT_DIR/in_half.ppm" \
       --out "$OUT_DIR/$name-half.ppm" --scale 0.5 --time 3 --tiles 2 --check-tiles > "$OUT_DIR/$name-half.log" 2>&1; then
    echo "FAIL $id (render scale 0.5)"; cat "$OUT_DIR/$name-half.log"; fail=1
  else
    echo "ok   $id (render scale 0.5)"
  fi
done
exit $fail
