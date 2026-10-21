#!/usr/bin/env python3
# Prisma FX - SPDX-License-Identifier: BSD-3-Clause
# Writes a deterministic test frame (binary PPM) with highlights, colour
# and fine detail, so every effect has something to work on.
import sys
import numpy as np

w, h = int(sys.argv[2]) if len(sys.argv) > 2 else 960, int(sys.argv[3]) if len(sys.argv) > 3 else 540
y, x = np.mgrid[0:h, 0:w].astype(np.float32)
u, v = x / w, y / h
img = np.stack([0.05 + 0.10 * u, 0.04 + 0.06 * v, 0.12 + 0.10 * (1 - u)], -1)

def disc(cx, cy, r, col, soft=2.0):
    d = np.sqrt((x - cx * w) ** 2 + (y - cy * h) ** 2)
    a = np.clip((r * h - d) / soft, 0, 1)[..., None]
    return a, np.array(col, np.float32)

for cx, cy, r, col in [(0.5, 0.42, 0.10, (1.0, 0.95, 0.8)), (0.22, 0.3, 0.05, (1.0, 0.35, 0.1)),
                       (0.8, 0.65, 0.06, (0.2, 0.7, 1.0)), (0.7, 0.25, 0.025, (1, 1, 1))]:
    a, c = disc(cx, cy, r, col)
    img = img * (1 - a) + c * a

# Bright bars like titles, and a checker strip for fine detail.
for i, (x0, x1) in enumerate([(0.12, 0.40), (0.45, 0.88)]):
    m = ((u > x0) & (u < x1) & (v > 0.78 + 0.05 * i) & (v < 0.81 + 0.05 * i))[..., None]
    img = np.where(m, np.array([0.95, 0.9, 0.85], np.float32), img)
chk = (((x // 8) + (y // 8)) % 2 == 0) & (v > 0.92)
img[chk] = [0.6, 0.6, 0.6]

img = (np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8)
with open(sys.argv[1], "wb") as f:
    f.write(b"P6\n%d %d\n255\n" % (w, h))
    f.write(img.tobytes())
