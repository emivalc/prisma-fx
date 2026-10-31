#!/usr/bin/env python3
# Prisma FX - SPDX-License-Identifier: BSD-3-Clause
# Renders every effect on the test frame and lays them out with labels.
# usage: contact_sheet.py OFXTEST BUNDLE_DIR OUT.png
import os, subprocess, sys, tempfile
from PIL import Image, ImageDraw

ofxtest, bundle, out = sys.argv[1:4]
here = os.path.dirname(os.path.abspath(__file__))
tmp = tempfile.mkdtemp()
src = os.path.join(tmp, "in.ppm")
subprocess.run([sys.executable, os.path.join(here, "make_test_image.py"), src, "640", "360"], check=True)
ids = subprocess.run([ofxtest, "--bundle-dir", bundle, "--list"], check=True, capture_output=True, text=True).stdout.split("\n")
tiles = [("Original", Image.open(src))]
for line in filter(None, ids):
    pid = line.split()[0]
    dst = os.path.join(tmp, pid + ".ppm")
    extra = {"Flash": ["--param", "Transition=0.3"]}.get(pid.rsplit(".", 1)[1], [])
    subprocess.run([ofxtest, "--bundle-dir", bundle, "--plugin", pid, "--in", src, "--out", dst, "--time", "3"] + extra,
                   check=True)
    tiles.append((pid.rsplit(".", 1)[1], Image.open(dst)))
cols = 3
rows = (len(tiles) + cols - 1) // cols
w, h = tiles[0][1].size
sheet = Image.new("RGB", (cols * w, rows * h), "black")
draw = ImageDraw.Draw(sheet)
for i, (name, im) in enumerate(tiles):
    x, y = (i % cols) * w, (i // cols) * h
    sheet.paste(im, (x, y))
    draw.rectangle([x + 8, y + 8, x + 16 + 7 * len(name), y + 26], fill=(0, 0, 0))
    draw.text((x + 12, y + 11), name, fill=(255, 255, 255))
sheet.save(out)
