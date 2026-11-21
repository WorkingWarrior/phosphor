# Turns a generated scene into what that machine could have shown: its resolution, its palette and its limits,
# then writes a preview PNG (640x172, as on the bar) and an .img blob for the firmware's asset pack.
#
#   amiga  320x86, 2x2 px, 32 colors snapped to 12-bit (4096), ordered dither
#   c64    160x86, 4x2 px (double-wide multicolor), the 16 C64 colors, checker dither, and per 4x8 cell only the
#          shared background plus 3 colors, like multicolor character mode
#   atari  160x86, 4x2 px, Atari 2600 NTSC colors, no dither, at most 4 colors on each line (COLUBK, COLUPF, COLUP0, COLUP1)
#   c64_free     the same without the cell limit (sprite-and-raster busy art: small lit windows survive)
#   atari_vivid  the same on the real NTSC table (Stella); for flat banded art
#   lcd    320x86, 2x2 px, flat printed inks (12 colors), no dither
#
# A portrait source (taller than wide) is turned a quarter clockwise first: Px draws portrait scenes with their top at
# the screen's right edge.
#
# usage: python3 convert.py <machine> <in.png> <name> <out_dir> <out.img> [crop|-] [c64 dither]
#
# .img (little endian): u16 w, u16 h, u8 sx, u8 sy, u8 colors, u8 0, u32 rle bytes, then colors x u16 RGB565 and the RLE
# as (count, color index) byte pairs, row by row. RetroScene.cpp reads it.
import struct
import sys
import numpy as np
from PIL import Image

W_OUT, H_OUT = 640, 172

C64 = [0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
       0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595]


STELLA_NTSC = [  # the 2600's 128 NTSC colors as the Stella emulator shows them: 16 hues x 8 lumas
    0x000000, 0x404040, 0x6c6c6c, 0x909090, 0xb0b0b0, 0xc8c8c8, 0xdcdcdc, 0xececec,
    0x444400, 0x646410, 0x848424, 0xa0a034, 0xb8b840, 0xd0d050, 0xe8e85c, 0xfcfc68,
    0x702800, 0x844414, 0x985c28, 0xac783c, 0xbc8c4c, 0xcca05c, 0xdcb468, 0xecc878,
    0x841800, 0x983418, 0xac5030, 0xc06848, 0xd0805c, 0xe09470, 0xeca880, 0xfcbc94,
    0x880000, 0x9c2020, 0xb03c3c, 0xc05858, 0xd07070, 0xe08888, 0xeca0a0, 0xfcb4b4,
    0x78005c, 0x8c2074, 0xa03c88, 0xb0589c, 0xc070b0, 0xd084c0, 0xdc9cd0, 0xecb0e0,
    0x480078, 0x602090, 0x783ca4, 0x8c58b8, 0xa070cc, 0xb484dc, 0xc49cec, 0xd4b0fc,
    0x140084, 0x302098, 0x4c3cac, 0x6858c0, 0x7c70d0, 0x9488e0, 0xa8a0ec, 0xbcb4fc,
    0x000088, 0x1c209c, 0x3840b0, 0x505cc0, 0x6874d0, 0x7c8ce0, 0x90a4ec, 0xa4b8fc,
    0x00187c, 0x1c3890, 0x3854a8, 0x5070bc, 0x6888cc, 0x7c9cdc, 0x90b4ec, 0xa4c8fc,
    0x002c5c, 0x1c4c78, 0x386890, 0x5084ac, 0x689cc0, 0x7cb4d4, 0x90cce8, 0xa4e0fc,
    0x003c2c, 0x1c5c48, 0x387c64, 0x509c80, 0x68b494, 0x7cd0ac, 0x90e4c0, 0xa4fcd4,
    0x003c00, 0x205c20, 0x407c40, 0x5c9c5c, 0x74b474, 0x8cd08c, 0xa4e4a4, 0xb8fcb8,
    0x143800, 0x345c1c, 0x507c38, 0x6c9850, 0x84b468, 0x9ccc7c, 0xb4e490, 0xc8fca4,
    0x2c3000, 0x4c501c, 0x687034, 0x848c4c, 0x9ca864, 0xb4c078, 0xc8d488, 0xe0ec9c,
    0x442800, 0x644818, 0x846830, 0xa08444, 0xb89c58, 0xd0b46c, 0xe8cc7c, 0xfce08c,
]


def atari_palette(vivid=False):
    # vivid = the real NTSC table (Stella), for flat banded art like the clock worlds. The default is a greyer YIQ
    # guess: the painted scenes (fireplace, aquarium) were tuned on it and turn noisy on the real table.
    if vivid:
        return hexpal(STELLA_NTSC)
    pal = []
    for h in range(16):
        for l in range(8):
            y = 0.06 + l * 0.125
            if h == 0:
                r = g = b = y
            else:
                ang = np.radians(-(h - 1) * 25.7 + 180 + 57)
                s = 0.22
                i, q = s * np.cos(ang), s * np.sin(ang)
                r = y + 0.956 * i + 0.621 * q
                g = y - 0.272 * i - 0.647 * q
                b = y - 1.106 * i + 1.703 * q
            pal.append(tuple(int(max(0, min(1, c)) * 255) for c in (r, g, b)))
    return np.array(pal, dtype=np.float32)


def hexpal(lst):
    return np.array([((c >> 16) & 255, (c >> 8) & 255, c & 255) for c in lst], dtype=np.float32)


def bayer(n):
    m = np.array([[0]])
    while m.shape[0] < n:
        m = np.block([[4 * m, 4 * m + 2], [4 * m + 3, 4 * m + 1]])
    return (m + 0.5) / m.size - 0.5  # -0.5 .. 0.5


def nearest(img, pal):
    d = ((img[:, :, None, :] - pal[None, None, :, :]) ** 2).sum(-1)
    return d.argmin(-1)


C64_DITHER = 40  # checker strength; 0 = flat bands (raster skies), 40 = painted shading

CROP = None  # optional 'y0:y1' fractions: take that band and squash it to the bar (pixel art re-pixelates anyway)


def load(path, w, h):
    im = Image.open(path).convert('RGB')
    if im.height > im.width:  # portrait scenes (hourglass) are stored turned a quarter, the way Px draws them
        im = im.rotate(-90, expand=True)
    iw, ih = im.size
    if CROP:
        y0, y1 = (float(v) for v in CROP.split(':'))
        im = im.crop((0, int(y0 * ih), iw, int(y1 * ih)))
        return np.asarray(im.resize((w, h), Image.BOX), dtype=np.float32)
    want = W_OUT / H_OUT
    if iw / ih > want:
        cw = int(ih * want)
        im = im.crop(((iw - cw) // 2, 0, (iw - cw) // 2 + cw, ih))
    else:
        ch = int(iw / want)
        im = im.crop((0, (ih - ch) // 2, iw, (ih - ch) // 2 + ch))
    return np.asarray(im.resize((w, h), Image.BOX), dtype=np.float32)


def median_cut(img, n):
    q = Image.fromarray(img.clip(0, 255).astype(np.uint8)).quantize(colors=n, method=Image.Quantize.MEDIANCUT)
    p = np.array(q.getpalette()[:n * 3], dtype=np.float32).reshape(-1, 3)
    return p


def with_accents(img, n, k):
    # median cut alone spends every slot on the big dark areas and drops the small warm lights (window, moon,
    # cabin wall). Keep k slots for the saturated pixels so the eye still lands on them.
    flat = img.reshape(-1, 3)
    sat = flat.max(1) - flat.min(1)
    acc = flat[(sat > 70) & (flat.max(1) > 110) & (flat[:, 0] > flat[:, 2])]
    base = median_cut(img, n - k)
    if len(acc) < 20:
        return median_cut(img, n)
    return np.vstack([base, median_cut(acc.reshape(1, -1, 3), k)])


def convert(machine, path):
    if machine == 'amiga':
        w, h, sx, sy = 320, 86, 2, 2
        img = load(path, w, h)
        pal = with_accents(img, 32, 6)
        pal = np.round(pal / 17) * 17  # 12-bit: 16 levels a channel
        pal = np.unique(pal, axis=0)
        spread = 26
        b = bayer(4)
        img2 = img + b[np.arange(h)[:, None] % 4, np.arange(w)[None, :] % 4][:, :, None] * spread
        idx = nearest(img2, pal)
    elif machine in ('c64', 'c64_free'):
        w, h, sx, sy = 160, 86, 4, 2
        img = load(path, w, h)
        pal = hexpal(C64)
        b = bayer(2)
        img2 = img + b[np.arange(h)[:, None] % 2, np.arange(w)[None, :] % 2][:, :, None] * C64_DITHER
        idx = nearest(img2, pal)
        # multicolor cells: one background for the whole screen plus 3 colors in each 4x8 cell
        bg = np.bincount(idx.ravel(), minlength=16).argmax()
        for cy in range(0, h, 8) if machine == 'c64' else ():
            for cx in range(0, w, 4):
                cell = idx[cy:cy + 8, cx:cx + 4]
                cnt = np.bincount(cell.ravel(), minlength=16)
                cnt[bg] = 0
                keep = [bg] + [c for c in np.argsort(-cnt)[:3] if cnt[c] > 0]
                sub = pal[keep]
                src = img2[cy:cy + 8, cx:cx + 4]
                cell[:] = np.array(keep)[nearest(src, sub)]
    elif machine in ('atari', 'atari_vivid'):
        w, h, sx, sy = 160, 86, 4, 2
        img = load(path, w, h)
        full = atari_palette(machine == 'atari_vivid')
        idx = nearest(img, full)
        for y in range(h):  # the 2600 draws a line at a time: background, playfield and a sprite or two
            cnt = np.bincount(idx[y], minlength=len(full))
            keep = np.argsort(-cnt)[:4]  # background, playfield and the two player colors
            keep = keep[cnt[keep] > 0]
            idx[y] = keep[nearest(img[y:y + 1], full[keep])[0]]
        used = np.unique(idx)
        remap = {c: i for i, c in enumerate(used)}
        pal = full[used]
        idx = np.vectorize(remap.get)(idx)
    elif machine == 'lcd':
        w, h, sx, sy = 320, 86, 2, 2
        img = load(path, w, h)
        pal = with_accents(img, 12, 4)
        idx = nearest(img, pal)
    else:
        raise SystemExit('machine?')
    used = np.unique(idx)
    remap = np.zeros(max(used) + 1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    return remap[idx].astype(np.uint8), pal[used].astype(np.uint8), w, h, sx, sy


def rle(data):
    out, i, n = bytearray(), 0, len(data)
    while i < n:
        j = i
        while j < n and j - i < 255 and data[j] == data[i]:
            j += 1
        out += bytes((j - i, data[i]))
        i = j
    return out


def main():
    global CROP
    machine, path, name, out_dir, blob = sys.argv[1:6]
    CROP = sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] != '-' else None
    if len(sys.argv) > 7:
        global C64_DITHER
        C64_DITHER = float(sys.argv[7])
    idx, pal, w, h, sx, sy = convert(machine, path)
    rgb = pal[idx]
    Image.fromarray(rgb).resize((w * sx, h * sy), Image.NEAREST).save(f'{out_dir}/{name}.png')
    r565 = [((int(r) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(b) >> 3) for r, g, b in pal]
    data = rle(idx.ravel().tolist())
    with open(blob, 'wb') as f:
        f.write(struct.pack('<HHBBBBI', w, h, sx, sy, len(pal), 0, len(data)))
        f.write(struct.pack(f'<{len(r565)}H', *r565))
        f.write(data)
    print(name, machine, f'{w}x{h}', len(pal), 'colors', len(data), 'bytes')


main()
