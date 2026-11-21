#!/usr/bin/env python3
"""Fener Bekcisi LCD art, drawn at build time.

The game looks like a Game & Watch style LCD handheld in every theme: a colour-printed backdrop sits behind
the "glass" and black LCD segments switch on top of it. Everything here is original art (lighthouse, keeper,
ships, rocks); only the medium is borrowed.

Writes:
  firmware/src/boards/touch349/gadget/GameFenerArt.h / .cpp   backdrop (RLE RGB565) + 1-bit segment masks + ids
  design/fener-lcd/*.png                                      previews composited the way the firmware does it

Run: python3 tools/fener_lcd.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw

W, H, SS = 640, 172, 4  # screen size, supersampling for smooth edges
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, 'firmware/src/boards/touch349/gadget')
PREV_DIR = os.path.join(ROOT, 'design/fener-lcd')

# ---------------------------------------------------------------------------------------------------------------------
# Layout (keep LANE_SPLIT_Y in GameFener.cpp in line with the lane bands)
# ---------------------------------------------------------------------------------------------------------------------
LANES = 3
LANE_TOP = [31, 69, 117]
LANE_BOT = [69, 117, 172]
LANE_WL = [57, 105, 153]           # waterline: ships stand on it
LANE_CY = [47, 93, 139]            # keeper window centres
LANE_S = [0.62, 0.8, 1.0]          # perspective scale far / mid / near
PX = [198, 296, 394, 492, 588]     # P1..P5 ship centres
ROCK_CX = 132
TOWER_CX, TOWER_Y0, TOWER_Y1, TOWER_HW0, TOWER_HW1 = 52, 30, 160, 26, 42
WIN_W, WIN_H = 34, 26

INK = (30, 34, 36)
GHOST = 0.075   # unlit segment: ink blended over the print
SHADOW = 0.20   # lit segment casts this much darkness 2 px down-right onto the print
SHADOW_D = 2


def tower_hw(y):
    return TOWER_HW0 + (TOWER_HW1 - TOWER_HW0) * (y - TOWER_Y0) / (TOWER_Y1 - TOWER_Y0)


# ---------------------------------------------------------------------------------------------------------------------
# Drawing helpers: coordinates are screen pixels (floats), drawn supersampled
# ---------------------------------------------------------------------------------------------------------------------
class Pen:
    def __init__(self, img, ink):
        self.d = ImageDraw.Draw(img, 'RGBA' if img.mode == 'RGB' else None)  # RGBA fills blend on the backdrop
        self.ink = ink

    def _c(self, c):
        return self.ink if c is None else c

    def poly(self, pts, c=None):
        self.d.polygon([(x * SS, y * SS) for x, y in pts], fill=self._c(c))

    def rect(self, x0, y0, x1, y1, c=None):
        self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], c)

    def ell(self, cx, cy, rx, ry=None, c=None):
        ry = rx if ry is None else ry
        self.d.ellipse([(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS, (cy + ry) * SS], fill=self._c(c))

    def line(self, pts, w, c=None):
        pts = [(x * SS, y * SS) for x, y in pts]
        self.d.line(pts, fill=self._c(c), width=max(1, round(w * SS)), joint='curve')
        r = w * SS / 2  # round caps
        for x, y in (pts[0], pts[-1]):
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=self._c(c))

    def arc(self, cx, cy, rx, ry, a0, a1, w, c=None, steps=24):
        pts = [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
                cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]
        self.line(pts, w, c)


def shape(pen, prims, ox, oy, s=1.0, flip=False, rot=0.0, pivot=(0, 0)):
    """Draw design-unit primitives with a transform. ink=True paints, ink=False cuts a hole (shows the print)."""
    ca, sa = math.cos(math.radians(rot)), math.sin(math.radians(rot))

    def T(x, y):
        x, y = x - pivot[0], y - pivot[1]
        x, y = x * ca - y * sa + pivot[0], x * sa + y * ca + pivot[1]
        return (ox + (-x if flip else x) * s, oy + y * s)

    hole = 0
    for p in prims:
        kind, ink = p[0], p[-1]
        c = None if ink else hole
        if kind == 'poly':
            pen.poly([T(*q) for q in p[1]], c)
        elif kind == 'rect':
            x0, y0, x1, y1 = p[1:5]
            pen.poly([T(x0, y0), T(x1, y0), T(x1, y1), T(x0, y1)], c)
        elif kind == 'ell':
            cx, cy, rx, ry = p[1:5]
            x, y = T(cx, cy)
            pen.ell(x, y, rx * s, ry * s, c)
        elif kind == 'line':
            pen.line([T(*q) for q in p[1]], max(1.0, p[2] * s), c)


# ---------------------------------------------------------------------------------------------------------------------
# Segment art (design units: x right, y down, origin = ship centre on the waterline; ships face LEFT, to the rocks)
# ---------------------------------------------------------------------------------------------------------------------
SAILBOAT = [  # far lane
    ('poly', [(-23, -8), (21, -8), (16, 0), (-15, 0)], True),
    ('rect', -1.2, -34, 1.2, -8, True),
    ('poly', [(2.5, -32), (2.5, -10), (20, -10)], True),          # mainsail
    ('poly', [(-2.5, -30), (-2.5, -10), (-21, -10)], True),        # jib
    ('poly', [(-1, -34), (-1, -39), (-9, -36.5)], True),           # pennant
    ('ell', -8, -4.2, 1.8, 1.8, False), ('ell', 0, -4.2, 1.8, 1.8, False), ('ell', 8, -4.2, 1.8, 1.8, False),
]
TRAWLER = [  # mid lane
    ('poly', [(-33, -14), (-25, 0), (24, 0), (30, -10), (-20, -10)], True),
    ('rect', 6, -24, 24, -10, True),                               # wheelhouse
    ('rect', 9, -21, 13.5, -16, False), ('rect', 16, -21, 20.5, -16, False),
    ('rect', 15, -31, 19.5, -24, True),                            # exhaust stack
    ('rect', -13.2, -33, -10.8, -10, True),                        # mast
    ('line', [(-12, -31), (-28, -14)], 1.6, True),                 # boom / rigging
    ('line', [(-12, -25), (4, -24)], 1.4, True),
    ('ell', -14, -5, 1.9, 1.9, False), ('ell', -5, -5, 1.9, 1.9, False),
    ('ell', 4, -5, 1.9, 1.9, False), ('ell', 13, -5, 1.9, 1.9, False),
]
STEAMER = [  # near lane
    ('poly', [(-38, -15), (-30, 0), (31, 0), (36, -6), (36, -12), (-25, -12)], True),
    ('rect', -15, -21, 27, -12, True),                             # deck house
    ('rect', -10, -28, 5, -21, True),                              # bridge
    ('poly', [(10, -34), (19, -34), (18, -21), (11, -21)], True),  # funnel
    ('rect', 10, -31, 19, -29, False),                             # funnel band
    ('rect', -26.2, -32, -23.8, -12, True),                        # foremast
    ('line', [(-25, -31), (-36, -15)], 1.4, True),                 # forestay
    ('ell', 25, -33.5, 3.2, 2.6, True), ('ell', 31.5, -35, 2.4, 2.0, True),  # smoke
] + [('ell', x, -6, 2.0, 2.0, False) for x in (-20, -11, -2, 7, 16, 25)] \
  + [('rect', x, -18.5, x + 3.6, -15, False) for x in (-11, -4, 3, 10, 17)] \
  + [('rect', -7, -26, 2, -23.5, False)]
SHIPS = [SAILBOAT, TRAWLER, STEAMER]

WAKE = [  # trailing a ship that turned away to the right; origin = its stern on the waterline
    ('line', [(-2, 1), (-12, -2)], 1.8, True), ('line', [(-4, 4), (-17, 3)], 1.8, True),
    ('line', [(-1, 6), (-10, 8)], 1.6, True),
]

WRECK = [  # bow half reared up on the rocks, stern half going down, spray
    ('poly', [(-30, -17), (-21, 0), (-5, 0), (-8, -9)], True),
    ('poly', [(3, -8), (2, 0), (23, 0), (27, -5)], True),
    ('line', [(10, -8), (16, -18)], 2.2, True),                    # snapped mast
    ('ell', -12, -19, 1.9, 1.9, True), ('ell', -4, -15, 1.5, 1.5, True), ('ell', 21, -11, 1.7, 1.7, True),
    ('ell', 28, -10, 1.3, 1.3, True),
    ('ell', -24, -8, 1.2, 1.2, False), ('ell', 15, -3, 1.2, 1.2, False),
]
MISS_ICON = [  # small ship going down bow first, for the miss counter; origin = waterline centre
    ('poly', [(-12, -6), (-9, 0), (10, 0), (12, -6)], True),
    ('rect', -4, -11, 6, -6, True),
    ('rect', 1, -15, 4.5, -11, True),
]

KEEPER = [  # origin = window centre; facing right, to the sea
    ('poly', [(-12, 13), (-11.5, 4), (-8, 1), (0, 1), (4, 4), (5, 13)], True),   # body
    ('ell', -4, -4.5, 5, 5, True),                                                  # head
    ('poly', [(-10, -7.5), (2, -7.5), (5.5, -5.8), (2, -5.5), (-10, -5.5)], True),  # cap brim
    ('ell', -4, -9, 5, 3.2, True),                                                  # cap crown
    ('ell', -1.5, -4, 1.1, 1.1, False),                                             # eye
    ('line', [(2, 4), (10, 1), (15, -1)], 3.4, True),                               # arm out
]
LAMP = [  # lantern in the keeper's hand, origin = window centre
    ('line', [(15, -5), (15, -2)], 1.4, True),
    ('poly', [(12.5, -6), (20.5, -6), (19.5, -8), (13.5, -8)], True),
    ('rect', 13, -6, 20, 5, True),
    ('rect', 14.6, -4, 18.4, 3, False),
    ('ell', 16.5, -0.5, 1.2, 1.6, True),
    ('rect', 12.5, 5, 20.5, 7, True),
]


def ship_x(p):
    return PX[p - 1]


# ---------------------------------------------------------------------------------------------------------------------
# Pixel font (labels) and 7-segment digits
# ---------------------------------------------------------------------------------------------------------------------
FONT = {
    'G': ['.###.', '#...#', '#....', '#.###', '#...#', '#...#', '.###.'],
    'A': ['.###.', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'M': ['#...#', '##.##', '#.#.#', '#.#.#', '#...#', '#...#', '#...#'],
    'E': ['#####', '#....', '#....', '####.', '#....', '#....', '#####'],
    'B': ['####.', '#...#', '#...#', '####.', '#...#', '#...#', '####.'],
    'H': ['#...#', '#...#', '#...#', '#####', '#...#', '#...#', '#...#'],
    'I': ['###', '.#.', '.#.', '.#.', '.#.', '.#.', '###'],
    ' ': ['...'],
}


def text(pen, s, x, y, px, c=None):
    for ch in s:
        rows = FONT[ch]
        for r, row in enumerate(rows):
            for k, v in enumerate(row):
                if v == '#':
                    pen.rect(x + k * px, y + r * px, x + (k + 1) * px - 0.01, y + (r + 1) * px - 0.01, c)
        x += (len(rows[0]) + 1) * px
    return x


def text_w(s, px):
    return sum((len(FONT[ch][0]) + 1) * px for ch in s) - px


def seg7(x, y, w, h, t, skew=0.1):
    """The 7 polygons a..g of one digit (slightly italic, small gaps, short bevels)."""
    g, b = 0.5, t * 0.32

    def hseg(yc, x0, x1):
        return [(x0 + g, yc), (x0 + g + b, yc - t / 2), (x1 - g - b, yc - t / 2), (x1 - g, yc),
                (x1 - g - b, yc + t / 2), (x0 + g + b, yc + t / 2)]

    def vseg(xc, y0, y1):
        return [(xc, y0 + g), (xc + t / 2, y0 + g + b), (xc + t / 2, y1 - g - b), (xc, y1 - g),
                (xc - t / 2, y1 - g - b), (xc - t / 2, y0 + g + b)]

    L, R, T, M, B = x + t / 2, x + w - t / 2, y + t / 2, y + h / 2, y + h - t / 2
    segs = [hseg(T, L, R), vseg(R, T, M), vseg(R, M, B), hseg(B, L, R), vseg(L, M, B), vseg(L, T, M), hseg(M, L, R)]
    return [[(px + skew * (y + h - py), py) for px, py in s] for s in segs]


# HUD placement
GAME_A_X, GAME_B_X, LABEL_Y, LABEL_PX = 116, 196, 5, 2
HI_X, HI_DIG_X, HI_Y, HI_W, HI_H, HI_T = 300, 330, 4, 11, 18, 2.6
MISS_X, MISS_Y = [430, 462, 494], 18
SCORE_X, SCORE_Y, SCORE_W, SCORE_H, SCORE_T, SCORE_GAP = 556, 3, 16, 24, 3.6, 6


# ---------------------------------------------------------------------------------------------------------------------
# Segments
# ---------------------------------------------------------------------------------------------------------------------
def build_segments():
    segs = []  # (name, draw(pen))

    for l in range(LANES):
        for p in range(1, 6):
            segs.append((f'SHIP_L{l}_P{p}', lambda pen, l=l, p=p: shape(pen, SHIPS[l], ship_x(p), LANE_WL[l], LANE_S[l])))
    for l in range(LANES):
        def saved(pen, l=l):
            s = LANE_S[l] * 0.8
            x, wl = PX[0] + 22 * LANE_S[l], LANE_WL[l] - 4 * LANE_S[l]
            shape(pen, SHIPS[l], x, wl, s, flip=True)
            shape(pen, WAKE, x - 30 * s, wl, LANE_S[l])
        segs.append((f'SAVED_L{l}', saved))
    for l in range(LANES):
        segs.append((f'WRECK_L{l}', lambda pen, l=l: shape(pen, WRECK, ROCK_CX - 4, LANE_WL[l] - 3 * LANE_S[l], LANE_S[l])))
    for l in range(LANES):
        segs.append((f'KEEPER_L{l}', lambda pen, l=l: shape(pen, KEEPER, TOWER_CX - 2, LANE_CY[l])))
    for l in range(LANES):
        segs.append((f'LAMP_L{l}', lambda pen, l=l: shape(pen, LAMP, TOWER_CX - 2, LANE_CY[l])))
    for l in range(LANES):
        def beam(pen, l=l):
            ox, oy = TOWER_CX - 2 + 16.5, LANE_CY[l] - 1     # lantern
            x0 = ox + 9                                       # rays fan out right from the lantern
            x1 = ship_x(1) - 36 * LANE_S[l] - 4
            spread = (-15, -5, 5) if l else (-11, -3.5, 5)    # far lane: keep the top ray out of the HUD
            for ang in spread:
                t = math.tan(math.radians(ang))
                pen.line([(x0, oy + (x0 - ox) * t), (x1, oy + (x1 - ox) * t)], 1.8)
            for ang in (spread[0] + spread[1]) / 2, (spread[1] + spread[2]) / 2:  # inner glints at the far end
                t = math.tan(math.radians(ang))
                xa = x1 - (x1 - x0) * 0.3
                pen.line([(xa, oy + (xa - ox) * t), (x1, oy + (x1 - ox) * t)], 1.4)
        segs.append((f'BEAM_L{l}', beam))
    for l in range(LANES):
        for ab in range(2):
            def wave(pen, l=l, ab=ab):
                y = LANE_WL[l] + 6 + 1.5 * l
                xs = [250, 346, 444, 540, 630] if ab == 0 else [222, 300, 400, 500, 610]
                for k, x in enumerate(xs):
                    if x > 632:
                        continue
                    r = 4 + 1.2 * l
                    pen.arc(x - r, y, r, r * 0.7, 200, 340, 1.6)
                    pen.arc(x + r, y, r, r * 0.7, 200, 340, 1.6)
            segs.append((f'WAVE_L{l}_{"AB"[ab]}', wave))

    lx, ly = TOWER_CX, 17  # lantern centre
    def beacon(pen, angles):
        for a in angles:
            for side in (-1, 1):
                ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
                pen.line([(lx + side * 21 * ca, ly + 21 * sa), (lx + side * 33 * ca, ly + 33 * sa)], 2.0)
    segs.append(('BEACON_A', lambda pen: beacon(pen, (-28, 0, 28))))
    segs.append(('BEACON_B', lambda pen: beacon(pen, (-14, 14))))
    segs.append(('GAME_A', lambda pen: text(pen, 'GAME A', GAME_A_X, LABEL_Y, LABEL_PX)))
    segs.append(('GAME_B', lambda pen: text(pen, 'GAME B', GAME_B_X, LABEL_Y, LABEL_PX)))
    for i in range(3):
        def miss(pen, i=i):  # a small steamer going down bow first
            shape(pen, MISS_ICON, MISS_X[i], MISS_Y - 1, 0.95, rot=-22)
            pen.line([(MISS_X[i] - 13, MISS_Y + 3), (MISS_X[i] + 12, MISS_Y + 3)], 1.4)
        segs.append((f'MISS_{i + 1}', miss))
    for d in range(3):
        polys = seg7(SCORE_X + d * (SCORE_W + SCORE_GAP), SCORE_Y, SCORE_W, SCORE_H, SCORE_T)
        for k in range(7):
            segs.append((f'SCORE_D{d}_{"ABCDEFG"[k]}', lambda pen, q=polys[k]: pen.poly(q)))
    for d in range(3):
        polys = seg7(HI_DIG_X + d * (HI_W + 4), HI_Y, HI_W, HI_H, HI_T)
        for k in range(7):
            segs.append((f'HI_D{d}_{"ABCDEFG"[k]}', lambda pen, q=polys[k]: pen.poly(q)))
    return segs


def render_mask(draw):
    img = Image.new('L', (W * SS, H * SS), 0)
    draw(Pen(img, 255))
    small = img.resize((W, H), Image.BOX)
    return np.array(small) >= 128


# ---------------------------------------------------------------------------------------------------------------------
# Backdrop (the colour print behind the LCD glass)
# ---------------------------------------------------------------------------------------------------------------------
LCD = (198, 203, 186)
SEA = [(176, 199, 192), (160, 189, 186), (143, 177, 177)]
FOAM = (212, 224, 216)
RED, CREAM, FRAME, WARM, GLASS = (186, 86, 72), (234, 226, 206), (92, 74, 66), (241, 226, 164), (245, 232, 150)
ROCK, ROCK_HI, ROCK_LO = (132, 121, 106), (160, 149, 132), (102, 94, 84)
GRASS, PRINT = (138, 160, 102), (78, 82, 78)


def build_backdrop():
    img = Image.new('RGB', (W * SS, H * SS), LCD)
    pen = Pen(img, None)

    # sea bands, one per lane, with a pale swell line on each boundary
    for l in range(LANES):
        pen.rect(0, LANE_TOP[l], W, LANE_BOT[l], SEA[l])
    pen.rect(0, LANE_TOP[0] - 0.6, W, LANE_TOP[0] + 0.6, (150, 170, 166))  # horizon
    for l in (1, 2):
        for x in range(0, W, 26):
            pen.arc(x + 13, LANE_TOP[l] + 1, 9, 2, 200, 340, 1.0, FOAM)
    # printed static ripples
    rng = np.random.default_rng(3)
    for l in range(LANES):
        for _ in range(16 + 6 * l):
            x = rng.uniform(180, 636)
            y = rng.uniform(LANE_WL[l] + 3, LANE_BOT[l] - 3)
            r = 2 + 1.5 * l * rng.uniform(0.6, 1.0)
            pen.arc(x, y, r, r * 0.5, 200, 340, 0.9, FOAM)
    # distant headland on the horizon
    pen.poly([(470, LANE_TOP[0]), (500, 26), (528, 24), (556, 27), (580, 25), (610, 28), (640, 27), (640, LANE_TOP[0])],
             (164, 178, 158))

    # tower body with stripes
    y0, y1 = TOWER_Y0, TOWER_Y1
    pen.poly([(TOWER_CX - tower_hw(y0), y0), (TOWER_CX + tower_hw(y0), y0), (TOWER_CX + tower_hw(y1), y1),
              (TOWER_CX - tower_hw(y1), y1)], CREAM)
    for a, b in ((y0, 31.5), (63, 78), (109, 124), (155, y1)):  # red bands between the windows
        pen.poly([(TOWER_CX - tower_hw(a), a), (TOWER_CX + tower_hw(a), a), (TOWER_CX + tower_hw(b), b),
                  (TOWER_CX - tower_hw(b), b)], RED)
    # shade the right flank of the round tower
    pen.poly([(TOWER_CX + tower_hw(y0) - 7, y0), (TOWER_CX + tower_hw(y0), y0), (TOWER_CX + tower_hw(y1), y1),
              (TOWER_CX + tower_hw(y1) - 10, y1)], (40, 20, 10, 46))
    # windows: frame + warm lit room
    for l in range(LANES):
        cx, cy = TOWER_CX - 2, LANE_CY[l]
        x0, x1, t0, t1 = cx - WIN_W / 2, cx + WIN_W / 2, cy - WIN_H / 2, cy + WIN_H / 2
        pen.rect(x0 - 2.5, t0 - 2.5, x1 + 2.5, t1 + 3.5, FRAME)
        pen.rect(x0, t0, x1, t1, WARM)
        pen.rect(x0 - 4, t1 + 1, x1 + 4, t1 + 4, FRAME)  # sill
    # gallery, lantern room, roof
    gy = TOWER_Y0
    pen.rect(TOWER_CX - 32, gy - 2, TOWER_CX + 32, gy + 1.5, FRAME)
    for x in range(-30, 31, 6):
        pen.rect(TOWER_CX + x - 0.6, gy - 8, TOWER_CX + x + 0.6, gy - 2, FRAME)
    pen.rect(TOWER_CX - 32, gy - 9, TOWER_CX + 32, gy - 7.8, FRAME)
    pen.rect(TOWER_CX - 15, 8, TOWER_CX + 15, gy - 2, GLASS)
    for x in (-15, -5, 5, 15):
        pen.rect(TOWER_CX + x - 1, 8, TOWER_CX + x + 1, gy - 2, FRAME)
    pen.ell(TOWER_CX, 17, 5.5, 5.5, (250, 244, 200))
    pen.poly([(TOWER_CX - 18, 9), (TOWER_CX - 10, 3), (TOWER_CX + 10, 3), (TOWER_CX + 18, 9)], RED)
    pen.rect(TOWER_CX - 18, 8, TOWER_CX + 18, 10, FRAME)
    pen.ell(TOWER_CX, 2, 2.2, 2.2, FRAME)

    # island at the tower foot
    pen.poly([(0, 156), (12, 151), (28, 154), (52, 151), (80, 153), (100, 156), (114, 162), (122, 172), (0, 172)], ROCK)
    pen.poly([(0, 156), (12, 151), (28, 154), (20, 158), (6, 160)], ROCK_HI)
    pen.poly([(80, 153), (100, 156), (114, 162), (122, 172), (104, 172), (98, 162)], ROCK_LO)
    for x in (8, 30, 62, 88):
        pen.poly([(x, 153), (x + 3, 146), (x + 5, 152), (x + 8, 147), (x + 10, 154)], GRASS)

    # rocks in each lane
    for l in range(LANES):
        s, wl, cx = LANE_S[l], LANE_WL[l], ROCK_CX
        body = [(-30, 5), (-26, -4), (-19, -9), (-12, -6), (-6, -15), (2, -10), (8, -17), (15, -9), (22, -11),
                (28, -3), (32, 5)]
        T = [(cx + x * s, wl + y * s) for x, y in body]
        pen.poly(T, ROCK)
        pen.poly([(cx + x * s, wl + y * s) for x, y in [(-19, -9), (-12, -6), (-6, -15), (-8, -6), (-16, -3)]], ROCK_HI)
        pen.poly([(cx + x * s, wl + y * s) for x, y in [(8, -17), (15, -9), (10, -4), (6, -8)]], ROCK_HI)
        pen.poly([(cx + x * s, wl + y * s) for x, y in [(22, -11), (28, -3), (32, 5), (20, 5), (21, -4)]], ROCK_LO)
        for k in range(5):
            pen.ell(cx + (-26 + k * 13) * s, wl + 5 * s, 6 * s, 1.6 * s, FOAM)

    # printed HUD label
    text(pen, 'HI', HI_X, LABEL_Y, LABEL_PX, PRINT)

    # rounded LCD window corners
    r = 7
    bez = (40, 42, 44)
    for (cx, cy) in ((0, 0), (W, 0), (0, H), (W, H)):
        sx, sy = (1 if cx == 0 else -1), (1 if cy == 0 else -1)
        pts = [(cx, cy)]
        for i in range(13):
            a = math.radians(180 + 90 * i / 12)
            pts.append((cx + sx * (r + r * math.cos(a)), cy + sy * (r + r * math.sin(a))))
        pen.poly(pts, bez)

    small = np.asarray(img.resize((W, H), Image.BOX)).astype(np.float32)
    return small


def to565(rgb):
    r, g, b = rgb[..., 0].astype(np.uint16), rgb[..., 1].astype(np.uint16), rgb[..., 2].astype(np.uint16)
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def from565(v):
    r = ((v >> 11) & 0x1F) * 255 // 31
    g = ((v >> 5) & 0x3F) * 255 // 63
    b = (v & 0x1F) * 255 // 31
    return np.stack([r, g, b], -1).astype(np.float32)


# ---------------------------------------------------------------------------------------------------------------------
# Firmware-equivalent composite (for previews)
# ---------------------------------------------------------------------------------------------------------------------
def compose(back, masks, ghosted, lit):
    out = back.copy()
    ink = np.array(INK, np.float32)
    shade = np.zeros((H, W), bool)
    for m, on in zip(masks, lit):
        if on:
            shade[SHADOW_D:, SHADOW_D:] |= m[:-SHADOW_D, :-SHADOW_D]
    out[shade] = back[shade] * (1 - SHADOW)
    for m, on, gh in zip(masks, lit, ghosted):
        if not on and gh:
            out[m] = back[m] * (1 - GHOST) + ink * GHOST
    for m, on in zip(masks, lit):
        if on:
            out[m] = ink
    return out


def save_preview(arr, name, scale=2):
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    img.resize((W * scale, H * scale), Image.NEAREST).save(os.path.join(PREV_DIR, name))


# ---------------------------------------------------------------------------------------------------------------------
# C output
# ---------------------------------------------------------------------------------------------------------------------
def c_array(vals, per_line, fmt):
    lines = []
    for i in range(0, len(vals), per_line):
        lines.append('  ' + ', '.join(fmt.format(v) for v in vals[i:i + per_line]) + ',')
    return '\n'.join(lines)


def main():
    os.makedirs(PREV_DIR, exist_ok=True)
    segs = build_segments()
    names = [n for n, _ in segs]
    masks = [render_mask(d) for _, d in segs]

    back = build_backdrop()
    back565 = to565(np.round(back).astype(np.uint16)).reshape(-1)
    back_q = from565(back565.reshape(H, W))

    # RLE: run, colour pairs
    rle = []
    i = 0
    n = len(back565)
    while i < n:
        j = i
        while j < n and back565[j] == back565[i] and j - i < 0xFFFF:
            j += 1
        rle += [j - i, int(back565[i])]
        i = j

    # 1-bit masks cropped to their boxes
    seg_rows, bits = [], []
    for name, m in zip(names, masks):
        ys, xs = np.nonzero(m)
        if len(xs) == 0:
            raise SystemExit(f'empty segment {name}')
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        sub = m[y0:y1, x0:x1]
        packed = np.packbits(sub, axis=1, bitorder='big')
        seg_rows.append((name, int(x0), int(y0), int(x1 - x0), int(y1 - y0), len(bits)))
        bits += packed.reshape(-1).tolist()

    ink565 = int(to565(np.array([INK], np.uint16))[0])
    enum_lines = ',\n'.join(f'  {n}' for n in names)
    h = f'''// Generated by tools/fener_lcd.py. Do not edit: change the script and run it again.
// Fener Bekcisi LCD art: the colour print behind the glass + one 1-bit mask per LCD segment.
#pragma once
#include <stdint.h>

namespace fener_art {{
constexpr int W = {W}, H = {H};
constexpr uint16_t INK = 0x{ink565:04X};      // lit segment
constexpr uint8_t GHOST_PCT = {round(GHOST * 100)};     // unlit segment: INK blended over the print
constexpr uint8_t SHADOW_PCT = {round(SHADOW * 100)};    // a lit segment darkens the print this much
constexpr int SHADOW_DX = {SHADOW_D}, SHADOW_DY = {SHADOW_D};  // ... at this offset (ink sits above the print)

// Backdrop as (run length, RGB565) pairs covering W*H pixels row by row.
extern const uint16_t BACKDROP_RLE[];
extern const uint32_t BACKDROP_RLE_LEN;  // number of uint16 values (pairs * 2)
inline void decodeBackdrop(uint16_t *out)
{{
  uint32_t o = 0;
  for (uint32_t i = 0; i + 1 < BACKDROP_RLE_LEN; i += 2) {{
    uint16_t run = BACKDROP_RLE[i], c = BACKDROP_RLE[i + 1];
    while (run--) out[o++] = c;
  }}
}}

// Segment mask: w x h bits at BITS + off, row-major, MSB first, each row padded to a whole byte.
struct Seg {{ int16_t x, y; uint16_t w, h; uint32_t off; }};
extern const Seg SEGS[];
extern const uint8_t BITS[];
inline bool segBit(const Seg &s, int x, int y) {{ return BITS[s.off + y * ((s.w + 7) >> 3) + (x >> 3)] & (0x80 >> (x & 7)); }}

// SHIP id = SHIP_L0_P1 + lane * 5 + (p - 1) · WAVE id = WAVE_L0_A + lane * 2 + (0 = A, 1 = B)
// SCORE/HI id = *_D0_A + digit * 7 + segment (a..g = 0..6), digit 0 = hundreds
enum Id : uint8_t {{
{enum_lines},
  SEG_COUNT
}};
// The saved-ship pose overlaps the P1 ship, so it shows only when lit (no ghost).
constexpr bool ghosted(uint8_t id) {{ return id < SAVED_L0 || id >= SAVED_L0 + 3; }}
}}  // namespace fener_art
'''
    seg_lines = ',\n'.join(f'  {{ {x}, {y}, {w}, {hh}, {off} }}  /* {n} */' for n, x, y, w, hh, off in seg_rows)
    cpp = f'''// Generated by tools/fener_lcd.py. Do not edit: change the script and run it again.
#include "GameFenerArt.h"

namespace fener_art {{
const uint32_t BACKDROP_RLE_LEN = {len(rle)};
const uint16_t BACKDROP_RLE[] = {{
{c_array(rle, 16, '{}')}
}};

const Seg SEGS[] = {{
{seg_lines}
}};

const uint8_t BITS[] = {{
{c_array(bits, 24, '0x{:02X}')}
}};
}}  // namespace fener_art
'''
    with open(os.path.join(OUT_DIR, 'GameFenerArt.h'), 'w') as f:
        f.write(h)
    with open(os.path.join(OUT_DIR, 'GameFenerArt.cpp'), 'w') as f:
        f.write(cpp)

    # previews
    idx = {n: i for i, n in enumerate(names)}
    nseg = len(names)

    def lit_of(on):
        v = [False] * nseg
        for n in on:
            v[idx[n]] = True
        return v

    def digits(prefix, value, lead_zero=True):
        on = []
        pat = ['ABCDEF', 'BC', 'ABGED', 'ABGCD', 'FGBC', 'AFGCD', 'AFGEDC', 'ABC', 'ABCDEFG', 'ABCDFG']
        s = f'{value:03d}'
        for d, ch in enumerate(s):
            if not lead_zero and d < 2 and s[:d + 1] == '0' * (d + 1):
                continue
            on += [f'{prefix}_D{d}_{k}' for k in pat[int(ch)]]
        return on

    gh = [not n.startswith('SAVED_') for n in names]
    save_preview(back_q, 'backdrop.png')
    save_preview(compose(back_q, masks, gh, [False] * nseg), 'ghosts.png')
    save_preview(compose(back_q, masks, gh, [True] * nseg), 'all-lit.png')
    play = ['GAME_A', 'BEACON_A', 'KEEPER_L1', 'LAMP_L1', 'BEAM_L1', 'SHIP_L0_P2', 'SHIP_L1_P1', 'SHIP_L2_P4',
            'SHIP_L0_P5', 'WAVE_L0_A', 'WAVE_L1_A', 'WAVE_L2_A', 'MISS_1'] + digits('SCORE', 128) + digits('HI', 501, False)
    save_preview(compose(back_q, masks, gh, lit_of(play)), 'play.png')
    crash = ['GAME_B', 'BEACON_B', 'KEEPER_L0', 'LAMP_L0', 'BEAM_L0', 'WRECK_L2', 'SAVED_L1', 'SHIP_L1_P3',
             'SHIP_L2_P2', 'WAVE_L0_B', 'WAVE_L1_B', 'WAVE_L2_B', 'MISS_1', 'MISS_2'] + digits('SCORE', 47) + digits('HI', 225, False)
    save_preview(compose(back_q, masks, gh, lit_of(crash)), 'crash.png')

    flash = len(rle) * 2 + len(bits) + nseg * 12
    print(f'{nseg} segments, backdrop RLE {len(rle) // 2} runs, flash ~{flash // 1024} KB -> {OUT_DIR}/GameFenerArt.*')


if __name__ == '__main__':
    main()
