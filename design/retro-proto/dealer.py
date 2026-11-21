#!/usr/bin/env python3
"""The 21 dealer ("Kasa Kazim") as sprites for each machine: assets/art/dealer_<pose>_<machine>.img.

Sources (Codex, prompts in gen/dealer_*.txt): gen/dealer_master.png is the character (tracked); gen/dealer_ref.png is
its head-and-chest crop (the part the 172 px bar shows); gen/dealer_<m>.png redraws that in the machine's style on flat
magenta (VFD: on black); gen/dealer_poses.sh makes gen/dealer_<pose>_<m>.png, edits of the base that change the face.
Those 24 Codex pictures are local only (gitignored, ~40 MB); a new run draws them anew, so the .img files are the
reference. Codex redraws the whole picture on an edit, so each pose is aligned to the base by phase correlation and
only the changed upper part is pasted onto the base: the belly never jitters between frames. All poses of a machine
share one crop, one size and one palette. Amiga and VFD cells are box filtered; C64 and 2600 cells take the most common
palette color of their source pixels, so no blended in-between colors (C64 matched in Lab, Pepto has no peach skin).

usage: python3 dealer.py [machine ...]     (default: all four)
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import convert as cv  # noqa: E402

POSES = ['idle', 'blink', 'talk', 'smug', 'sad', 'shock']
GRID = {'amiga': (2, 2), 'c64': (4, 2), 'atari': (4, 2), 'vfd': (2, 2)}
WIDTH = 150  # on screen, px
MAX_H = 170  # the bar is 172 px tall; the sprite stands on its bottom edge
ART = os.path.join(HERE, '..', '..', 'assets', 'art')
OUT = os.path.join(HERE, 'out')
VFD = {'halo': (0x00, 0x49, 0x42), 'dim': (0x42, 0x96, 0x8C), 'lit': (0x73, 0xF7, 0xE6), 'hot': (0xFF, 0x41, 0xB5)}


def load(path, size=None):
    im = Image.open(path).convert('RGB')
    if size and im.size != size:
        im = im.resize(size, Image.LANCZOS)
    return np.asarray(im).astype(np.float32)


def background(a, m):
    if m == 'vfd':
        return a.max(-1) < 34  # VFD art is drawn on black
    return (a[:, :, 0] > 170) & (a[:, :, 2] > 170) & (a[:, :, 1] < 110)


def shift_of(base, pose, y0):
    """Integer (dy, dx) that moves pose onto base, measured on the body (rows from y0 down)."""
    g0, g1 = base[y0:].mean(-1), pose[y0:].mean(-1)
    g0, g1 = g0 - g0.mean(), g1 - g1.mean()
    f = np.fft.fft2(g0) * np.conj(np.fft.fft2(g1))
    r = np.fft.ifft2(f / (np.abs(f) + 1e-6)).real
    dy, dx = np.unravel_index(r.argmax(), r.shape)
    h, w = r.shape
    return (dy - h if dy > h // 2 else dy), (dx - w if dx > w // 2 else dx)


def roll(a, dy, dx, fill):
    out = np.empty_like(a)
    out[...] = fill
    h, w = a.shape[:2]
    ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
    xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
    out[yd, xd] = a[ys, xs]
    return out


def face_box(base, pose, bg, top_frac=0.62):
    """Box around what the edit changed in the upper part of the figure (the face), with a margin."""
    d = np.abs(base - pose).sum(-1)
    d = np.asarray(Image.fromarray(np.clip(d / 3, 0, 255).astype(np.uint8)).resize(
        (base.shape[1] // 8, base.shape[0] // 8), Image.BOX)).astype(np.float32)
    ys, xs = np.nonzero(~bg)
    fy0, fy1 = ys.min(), ys.max()
    lim = int((fy0 + (fy1 - fy0) * top_frac) / 8)
    d[lim:] = 0
    hit = d > 28
    if not hit.any():
        return None
    yy, xx = np.nonzero(hit)
    m = 3
    return (max(0, (yy.min() - m) * 8), min(base.shape[0], (yy.max() + 1 + m) * 8),
            max(0, (xx.min() - m) * 8), min(base.shape[1], (xx.max() + 1 + m) * 8))


def lab(rgb):
    c = rgb / 255.0
    c = np.where(c > 0.04045, ((c + 0.055) / 1.055) ** 2.4, c / 12.92)
    xyz = c @ np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]]).T
    f = xyz / np.array([0.9505, 1.0, 1.089])
    f = np.where(f > 0.008856, np.cbrt(f), 7.787 * f + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)


def nearest_lab(img, pal, chroma=2.0):
    """Nearest palette color in Lab with hue counted double: on the dark Pepto palette plain RGB distance turns skin green."""
    a, p = lab(img), lab(pal)
    a[..., 1:] *= chroma
    p[:, 1:] *= chroma
    return ((a[:, :, None, :] - p[None, None]) ** 2).sum(-1).argmin(-1)


def majority(idx, solid, w, h, n):
    """Each screen cell takes the most common palette color of its source pixels (no blended in-between colors)."""
    H, W = idx.shape
    ye, xe = np.linspace(0, H, h + 1).astype(int), np.linspace(0, W, w + 1).astype(int)
    out, sol = np.zeros((h, w), np.int64), np.zeros((h, w), bool)
    for j in range(h):
        for i in range(w):
            s = solid[ye[j]:ye[j + 1], xe[i]:xe[i + 1]]
            if s.mean() > 0.5:
                sol[j, i] = True
                out[j, i] = np.bincount(idx[ye[j]:ye[j + 1], xe[i]:xe[i + 1]][s], minlength=n).argmax()
    return out, sol


def quantize(m, imgs, solids, w, h):
    """Palette index images (0 = transparent) and the palette, shared by all poses of the machine.
    VFD and Amiga get the cells already (box filtered); C64 and 2600 get 4x4 source pixels a cell and vote."""
    if m == 'vfd':
        pal = np.array([[255, 0, 255], VFD['halo'], VFD['dim'], VFD['lit'], VFD['hot']], np.uint8)
        outs = []
        for img, solid in zip(imgs, solids):
            lum = img.max(-1)
            pink = (img[:, :, 0] > img[:, :, 1] + 50) & (img[:, :, 0] > 120)
            idx = np.where(lum < 95, 1, np.where(lum < 175, 2, 3))
            idx = np.where(pink, 4, idx)
            outs.append(np.where(solid, idx, 0).astype(np.uint8))
        return outs, pal
    if m == 'amiga':
        allpx = np.concatenate([img[s] for img, s in zip(imgs, solids)]).reshape(1, -1, 3)
        pal = np.unique(np.round(cv.median_cut(allpx, 31) / 17) * 17, axis=0)
        idxs = [cv.nearest(img, pal) for img in imgs]
    else:
        # 2600: the Stella table without the four-a-line limit (a 37 px wide face is beyond the real chip anyway, and
        # the line limit only streaks it)
        pal = cv.hexpal(cv.C64) if m == 'c64' else cv.atari_palette(True)
        idxs, sols = [], []
        for img, solid in zip(imgs, solids):
            ix, so = majority(nearest_lab(img, pal) if m == 'c64' else cv.nearest(img, pal), solid, w, h, len(pal))
            idxs.append(ix)
            sols.append(so)
        solids[:] = sols
    used = np.unique(np.concatenate([ix[s] for ix, s in zip(idxs, solids)]))
    remap = np.zeros(len(pal), dtype=np.int64)
    remap[used] = np.arange(1, len(used) + 1)
    outs = [np.where(s, remap[ix], 0).astype(np.uint8) for ix, s in zip(idxs, solids)]
    return outs, np.vstack([[255, 0, 255], pal[used]]).astype(np.uint8)


def write_img(path, idx, pal, sx, sy):
    h, w = idx.shape
    r565 = [((int(r) >> 3) << 11) | ((int(g) >> 2) << 5) | (int(b) >> 3) for r, g, b in pal]
    data = cv.rle(idx.ravel().tolist())
    with open(path, 'wb') as f:
        f.write(struct.pack('<HHBBBBI', w, h, sx, sy, len(pal), 1, len(data)))
        f.write(struct.pack(f'<{len(r565)}H', *r565))
        f.write(data)
    return len(data)


def machine(m):
    base_path = os.path.join(HERE, 'gen', f'dealer_{m}.png')
    base = load(base_path)
    size = (base.shape[1], base.shape[0])
    bg0 = background(base, m)
    fill = np.array([0, 0, 0] if m == 'vfd' else [255, 0, 255], np.float32)
    ys, _ = np.nonzero(~bg0)
    body_y = int(ys.min() + (ys.max() - ys.min()) * 0.55)
    frames = []
    for pose in POSES:
        if pose == 'idle':
            frames.append(base.copy())
            continue
        p = os.path.join(HERE, 'gen', f'dealer_{pose}_{m}.png')
        if not os.path.exists(p):
            print(f'  {m}: no {pose}, using idle')
            frames.append(base.copy())
            continue
        a = load(p, size)
        dy, dx = shift_of(base, a, body_y)
        a = roll(a, dy, dx, fill)
        box = face_box(base, a, bg0)
        f = base.copy()
        if box:
            y0, y1, x0, x1 = box
            f[y0:y1, x0:x1] = a[y0:y1, x0:x1]
        print(f'  {m} {pose}: shift {dy},{dx} face box {box}')
        frames.append(f)

    # one crop for all frames
    bgs = [background(f, m) for f in frames]
    union = ~np.logical_and.reduce(bgs)
    ys, xs = np.nonzero(union)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    sx, sy = GRID[m]
    w = WIDTH // sx
    h = max(1, round(w * sx * (y1 - y0) / (x1 - x0) / sy))
    if h * sy > MAX_H:  # too tall for the bar: keep the head, cut the belly
        h = MAX_H // sy
        y1 = y0 + round(h * sy * (x1 - x0) / (w * sx))
    imgs, solids = [], []
    for f, bg in zip(frames, bgs):
        c, k = f[y0:y1, x0:x1].copy(), bg[y0:y1, x0:x1]
        if m != 'vfd':
            c[k] = 0  # edges bleed into black, which reads as the outline
        size = (w * 4, h * 4) if m in ('c64', 'atari') else (w, h)
        imgs.append(np.asarray(Image.fromarray(c.astype(np.uint8)).resize(size, Image.BOX), dtype=np.float32))
        solids.append(np.asarray(Image.fromarray(((~k) * 255).astype(np.uint8)).resize(size, Image.BOX)) > 127)
    idxs, pal = quantize(m, imgs, solids, w, h)
    os.makedirs(ART, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    sheet = Image.new('RGB', (len(POSES) * (w * sx + 8), h * sy), (20, 60, 40) if m != 'vfd' else (0, 8, 8))
    for k, (pose, idx) in enumerate(zip(POSES, idxs)):
        n = write_img(os.path.join(ART, f'dealer_{pose}_{m}.img'), idx, pal, sx, sy)
        rgb = pal[idx]
        alpha = (idx > 0).astype(np.uint8) * 255
        im = Image.fromarray(np.dstack([rgb, alpha]), 'RGBA').resize((w * sx, h * sy), Image.NEAREST)
        sheet.paste(im, (k * (w * sx + 8), 0), im)
        print(f'  dealer_{pose}_{m}: {w}x{h} cells, {len(pal)} colors, {n} bytes')
    sheet.save(os.path.join(OUT, f'dealer_sheet_{m}.png'))


if __name__ == '__main__':
    for m in sys.argv[1:] or list(GRID):
        machine(m)
