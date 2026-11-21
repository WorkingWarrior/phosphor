# Turns a generated scene into what that machine could have shown: its resolution, its palette and its limits,
# then writes a preview PNG (640x172, as on the bar) and a C header for the firmware.
#
#   amiga  320x86, 2x2 px, 32 colors snapped to 12-bit (4096), ordered dither
#   c64    160x86, 4x2 px (double-wide multicolor), the 16 C64 colors, checker dither, and per 4x8 cell only the
#          shared background plus 3 colors, like multicolor character mode
#   atari  160x86, 4x2 px, Atari 2600 NTSC colors, no dither, at most 4 colors on each line (COLUBK, COLUPF, COLUP0, COLUP1)
#   lcd    320x86, 2x2 px, flat printed inks (12 colors), no dither
#
# usage: python3 convert.py <machine> <in.png> <name> <out_dir> <header.h>
import sys
import numpy as np
from PIL import Image

W_OUT, H_OUT = 640, 172

C64 = [0x000000, 0xFFFFFF, 0x68372B, 0x70A4B2, 0x6F3D86, 0x588D43, 0x352879, 0xB8C76F,
       0x6F4F25, 0x433900, 0x9A6759, 0x444444, 0x6C6C6C, 0x9AD284, 0x6C5EB5, 0x959595]


def atari_palette():
    # NTSC 2600: 16 hues x 8 lumas. Hue 0 is grey, 1..15 walk the color wheel from gold.
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


CROP = None  # optional 'y0:y1' fractions: take that band and squash it to the bar (pixel art re-pixelates anyway)


def load(path, w, h):
    im = Image.open(path).convert('RGB')
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
    elif machine == 'c64':
        w, h, sx, sy = 160, 86, 4, 2
        img = load(path, w, h)
        pal = hexpal(C64)
        b = bayer(2)
        img2 = img + b[np.arange(h)[:, None] % 2, np.arange(w)[None, :] % 2][:, :, None] * 40
        idx = nearest(img2, pal)
        # multicolor cells: one background for the whole screen plus 3 colors in each 4x8 cell
        bg = np.bincount(idx.ravel(), minlength=16).argmax()
        for cy in range(0, h, 8):
            for cx in range(0, w, 4):
                cell = idx[cy:cy + 8, cx:cx + 4]
                cnt = np.bincount(cell.ravel(), minlength=16)
                cnt[bg] = 0
                keep = [bg] + [c for c in np.argsort(-cnt)[:3] if cnt[c] > 0]
                sub = pal[keep]
                src = img2[cy:cy + 8, cx:cx + 4]
                cell[:] = np.array(keep)[nearest(src, sub)]
    elif machine == 'atari':
        w, h, sx, sy = 160, 86, 4, 2
        img = load(path, w, h)
        full = atari_palette()
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
    machine, path, name, out_dir, header = sys.argv[1:6]
    CROP = sys.argv[6] if len(sys.argv) > 6 else None
    idx, pal, w, h, sx, sy = convert(machine, path)
    rgb = pal[idx]
    Image.fromarray(rgb).resize((w * sx, h * sy), Image.NEAREST).save(f'{out_dir}/{name}.png')
    r565 = [((int(r) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(b) >> 3) for r, g, b in pal]
    data = rle(idx.ravel().tolist())
    with open(header, 'a') as f:
        f.write(f'// {name}: {machine}, {w}x{h} at {sx}x{sy} px, {len(pal)} colors, {len(data)} bytes RLE\n')
        f.write(f'static const uint16_t {name}_pal[] = {{' + ','.join(f'0x{c:04X}' for c in r565) + '};\n')
        f.write(f'static const uint8_t {name}_rle[] = {{')
        for k in range(0, len(data), 32):
            f.write('\n  ' + ','.join(str(v) for v in data[k:k + 32]) + ',')
        f.write('\n};\n')
        f.write(f'static const art::Image {name} = {{ {w}, {h}, {sx}, {sy}, {len(pal)}, {name}_pal, {name}_rle, '
                f'sizeof({name}_rle) }};\n\n')
    print(name, machine, f'{w}x{h}', len(pal), 'colors', len(data), 'bytes')


main()
