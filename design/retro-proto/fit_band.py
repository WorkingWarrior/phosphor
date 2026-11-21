# For a scene that came out too tall (night weather): take the band that puts the village above the forecast bar, then put the moon back
# whole into free sky (only its bright pixels, so the banded sky behind it stays).
#
#   usage: python3 fit_band.py <in.png> <out.png> <band top y> <moon x0 y0 x1 y1> <moon scale> <moon x y>
#   wx_night_c64:   gen/wx_night_c64.png gen/wx_night_c64_fit.png 264 1366 130 1493 256 1 1366 16
#   wx_night_atari: gen/wx_night_atari.png gen/wx_night_atari_fit.png 392 1462 48 1662 248 0.6 700 24
import sys
from PIL import Image
import numpy as np
src, out, y0, mx0, my0, mx1, my1, scale, dx, dy = sys.argv[1], sys.argv[2], *map(float, sys.argv[3:])
im = Image.open(src).convert('RGB')
W = im.width; H = round(W * 172 / 640)
band = im.crop((0, int(y0), W, int(y0) + H))
moon = im.crop((int(mx0), int(my0), int(mx1), int(my1)))
if scale != 1:
    moon = moon.resize((round(moon.width * scale), round(moon.height * scale)), Image.NEAREST)
mask = Image.fromarray(((np.asarray(moon).astype(int).sum(2) > 600) * 255).astype(np.uint8))
band.paste(moon, (int(dx), int(dy)), mask)
band.save(out)
print(out, band.size)
