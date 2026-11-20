// Drawing kit for the mockups: a 640x172 framebuffer of 0xRRGGBB pixels with the same kind of primitives the firmware
// has (rects, lines, 8x8 text, dot-matrix text, 7 segments, sprites, ordered dither). No anti-aliasing anywhere, and
// every color goes through RGB565 on output, so what you see is what the panel can show.
(function (G) {
  const W = 640, H = 172;

  const hex = (h) => parseInt(h.replace('#', ''), 16);
  const to565 = (c) => (((c >> 16) & 255) >> 3) << 11 | (((c >> 8) & 255) >> 2) << 5 | ((c & 255) >> 3);
  const from565 = (v) => {
    const r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    return (Math.round(r * 255 / 31) << 16) | (Math.round(g * 255 / 63) << 8) | Math.round(b * 255 / 31);
  };
  const toHex = (c) => '#' + c.toString(16).padStart(6, '0').toUpperCase();
  const mix = (a, b, t) => {
    const ch = (s) => Math.round(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t);
    return (ch(16) << 16) | (ch(8) << 8) | ch(0);
  };
  const BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
  const bayer = (x, y) => (BAYER[(y & 3) * 4 + (x & 3)] + 0.5) / 16;

  // Turkish-aware capitals for the themes that show text in upper case.
  const up = (s, lang) => (lang === 'tr' ? s.replace(/i/g, 'İ') : s).toUpperCase();

  class Fb {
    constructor() {
      this.d = new Uint32Array(W * H);
      this.noClip();
    }
    clip(x0, y0, x1, y1) { this.cx0 = Math.max(0, x0); this.cy0 = Math.max(0, y0); this.cx1 = Math.min(W, x1); this.cy1 = Math.min(H, y1); }
    noClip() { this.clip(0, 0, W, H); }
    px(x, y, c) {
      x = Math.floor(x); y = Math.floor(y);
      if (x < this.cx0 || y < this.cy0 || x >= this.cx1 || y >= this.cy1 || c == null) return;
      this.d[y * W + x] = c;
    }
    get(x, y) { return this.d[Math.max(0, Math.min(H - 1, y)) * W + Math.max(0, Math.min(W - 1, x))]; }
    rect(x, y, w, h, c) {
      if (c == null) return;
      const x0 = Math.max(this.cx0, Math.floor(x)), y0 = Math.max(this.cy0, Math.floor(y));
      const x1 = Math.min(this.cx1, Math.floor(x + w)), y1 = Math.min(this.cy1, Math.floor(y + h));
      for (let j = y0; j < y1; j++) this.d.fill(c, j * W + x0, j * W + Math.max(x0, x1));
    }
    clear(c) { this.d.fill(c); }
    frame(x, y, w, h, c, t = 1) {
      this.rect(x, y, w, t, c); this.rect(x, y + h - t, w, t, c);
      this.rect(x, y, t, h, c); this.rect(x + w - t, y, t, h, c);
    }
    // a dithered fill: lvl 0 = all a, 1 = all b
    dith(x, y, w, h, a, b, lvl) {
      for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) this.px(i, j, bayer(i, j) < lvl ? b : a);
    }
    // only the b pixels of a dither (a transparent)
    stip(x, y, w, h, b, lvl) {
      for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) if (bayer(i, j) < lvl) this.px(i, j, b);
    }
    fill(x, y, w, h, fn) {
      for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) { const c = fn(i, j); if (c != null) this.px(i, j, c); }
    }
    line(x0, y0, x1, y1, c, t = 1) {
      x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
      const dx = Math.abs(x1 - x0), dy = -Math.abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
      let e = dx + dy;
      for (;;) {
        this.rect(x0 - (t >> 1), y0 - (t >> 1), t, t, c);
        if (x0 === x1 && y0 === y1) break;
        const e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
      }
    }
    disc(cx, cy, r, c) {
      for (let j = Math.floor(cy - r); j <= cy + r; j++) for (let i = Math.floor(cx - r); i <= cx + r; i++) {
        const dx = i + 0.5 - cx, dy = j + 0.5 - cy;
        if (dx * dx + dy * dy <= r * r) this.px(i, j, c);
      }
    }
    ring(cx, cy, r0, r1, c) {
      for (let j = Math.floor(cy - r1); j <= cy + r1; j++) for (let i = Math.floor(cx - r1); i <= cx + r1; i++) {
        const dx = i + 0.5 - cx, dy = j + 0.5 - cy, d = dx * dx + dy * dy;
        if (d <= r1 * r1 && d > r0 * r0) this.px(i, j, c);
      }
    }

    // ---- 8x8 bitmap text. o: { s: scale, sy: vertical scale (default s), c: color, sh: {dx, dy, c} shadow,
    //      cell: (fb, x, y, w, h) => void to draw each font pixel yourself }
    glyph(font, ch) {
      return font.glyphs[ch] || font.glyphs[ch.toUpperCase()] || font.glyphs['?'];
    }
    text(font, str, x, y, o = {}) {
      const s = o.s || 2, sy = o.sy || s, adv = (o.adv || 8) * s;
      const chars = Array.from(str);
      const draw = (ox, oy, c) => {
        let cx = x;
        for (const ch of chars) {
          const g = this.glyph(font, ch);
          for (let r = 0; r < 8; r++) {
            const bits = g[r];
            if (!bits) continue;
            for (let col = 0; col < 8; col++) {
              if (bits & (0x80 >> col)) {
                if (o.cell && c === o.c) o.cell(this, cx + col * s + ox, y + r * sy + oy, s, sy, col, r);
                else this.rect(cx + col * s + ox, y + r * sy + oy, s, sy, c);
              }
            }
          }
          cx += adv;
        }
        return cx;
      };
      if (o.sh) draw(o.sh.dx, o.sh.dy, o.sh.c);
      return draw(0, 0, o.c);
    }
    tw(str, s = 2, adv = 8) { return Array.from(str).length * adv * s - (adv - 7) * s; }

    // ---- 5x7 dot-matrix text of the VFD theme (firmware VfdFont.h). ink: { c, h: glow, g: ghost cells }
    vfdText(str, x, y, p, d, ink) {
      const chars = Array.from(str.toUpperCase());
      const ACC = { 'Ç': ['C', [[7, 2]]], 'Ş': ['S', [[7, 2]]], 'Ö': ['O', [[-2, 1], [-2, 3]]], 'Ü': ['U', [[-2, 1], [-2, 3]]],
        'İ': ['I', [[-2, 2]]], 'Ğ': ['G', [[-2, 1], [-2, 2], [-2, 3]]] };
      const dots = [];
      chars.forEach((ch, i) => {
        let base = ch, extra = [];
        if (ACC[ch]) { base = ACC[ch][0]; extra = ACC[ch][1]; }
        const rows = G.VFD_GLYPHS[base.codePointAt(0)] || G.VFD_GLYPHS[63];
        const gx = x + i * 6 * p;
        for (let r = 0; r < 7; r++) for (let col = 0; col < 5; col++) if (rows[r] & (0x10 >> col)) dots.push([gx + col * p, y + r * p]);
        for (const [r, col] of extra) dots.push([gx + col * p, y + r * p]);
      });
      if (ink.g != null) chars.forEach((_, i) => {
        for (let r = 0; r < 7; r++) for (let col = 0; col < 5; col++) this.rect(x + i * 6 * p + col * p, y + r * p, d, d, ink.g);
      });
      if (ink.h != null) for (const [dx, dy] of dots) this.rect(dx - 1, dy - 1, d + 2, d + 2, ink.h);
      for (const [dx, dy] of dots) this.rect(dx, dy, d, d, ink.c);
      return x + chars.length * 6 * p;
    }
    vfdW(str, p) { return Array.from(str).length * 6 * p - p; }

    // ---- 7-segment digits, same geometry as the firmware's vfd::seg()
    seg(s, x, y, w, h, t, gap, ink, opt = {}) {
      const MASK = [0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f];
      const shape = (b, grow, c, chamfer) => {
        const horiz = b.w >= b.h, th = horiz ? b.h : b.w;
        for (let k = -grow; k < th + grow; k++) {
          const kk = Math.max(0, Math.min(th - 1, k));
          const inset = chamfer ? Math.floor(Math.min(kk, th - 1 - kk) / 2) - grow : -grow;
          if (horiz) this.rect(b.x + inset, b.y + k, b.w - 2 * inset, 1, c);
          else this.rect(b.x + k, b.y + inset, 1, b.h - 2 * inset, c);
        }
      };
      let cx = x;
      for (const ch of s) {
        if (ch === ':') {
          const y1 = y + Math.floor((h * 3 + 5) / 10), y2 = y + Math.floor((h * 65 + 50) / 100);
          if (ink.h != null) { this.rect(cx - 2, y1 - 2, t + 4, t + 4, ink.h); this.rect(cx - 2, y2 - 2, t + 4, t + 4, ink.h); }
          if (ink.c != null) { this.rect(cx, y1, t, t, ink.c); this.rect(cx, y2, t, t, ink.c); }
          cx += t + gap;
          continue;
        }
        const hh = Math.floor((h + 1) / 2), v = hh - t - Math.floor((t + 1) / 2) - 2;
        const b = [
          { x: cx + t + 1, y, w: w - 2 * t - 2, h: t }, { x: cx + w - t, y: y + t + 1, w: t, h: v },
          { x: cx + w - t, y: y + hh + Math.floor((t + 1) / 2) + 1, w: t, h: v }, { x: cx + t + 1, y: y + h - t, w: w - 2 * t - 2, h: t },
          { x: cx, y: y + hh + Math.floor((t + 1) / 2) + 1, w: t, h: v }, { x: cx, y: y + t + 1, w: t, h: v },
          { x: cx + t + 1, y: y + hh - Math.floor(t / 2), w: w - 2 * t - 2, h: t },
        ];
        const lit = ch >= '0' && ch <= '9' ? MASK[+ch] : ch === '-' ? 0x40 : 0;
        const ch2 = opt.chamfer !== false;
        for (let i = 0; i < 7; i++) if (!(lit & (1 << i)) && ink.g != null) shape(b[i], 0, ink.g, ch2);
        if (ink.h != null) for (let i = 0; i < 7; i++) if (lit & (1 << i)) shape(b[i], 2, ink.h, ch2);
        for (let i = 0; i < 7; i++) if (lit & (1 << i)) shape(b[i], 0, ink.c, ch2);
        cx += w + gap;
      }
      return cx;
    }

    // ---- sprites: a bool grid (array of strings '#'/'.' or a Grid) drawn as s x s blocks, or per-cell callback
    grid(g, x, y, s, c, cell) {
      for (let j = 0; j < g.h; j++) for (let i = 0; i < g.w; i++) if (g.at(i, j)) {
        if (cell) cell(this, x + i * s, y + j * s, i, j, g.at(i, j));
        else this.rect(x + i * s, y + j * s, s, s, typeof c === 'function' ? c(i, j, g.at(i, j)) : c);
      }
    }
    // pixels as RGBA bytes, every color through RGB565
    rgba(out) {
      const cache = new Map();
      for (let i = 0; i < W * H; i++) {
        const c0 = this.d[i];
        let c = cache.get(c0);
        if (c === undefined) { c = from565(to565(c0)); cache.set(c0, c); }
        out[i * 4] = (c >> 16) & 255; out[i * 4 + 1] = (c >> 8) & 255; out[i * 4 + 2] = c & 255; out[i * 4 + 3] = 255;
      }
      return out;
    }
  }

  // A small paint grid for sprites drawn from shapes (the dancing couple, reels).
  class Grid {
    constructor(w, h) { this.w = w; this.h = h; this.v = new Uint8Array(w * h); }
    at(x, y) { return x >= 0 && y >= 0 && x < this.w && y < this.h ? this.v[y * this.w + x] : 0; }
    set(x, y, k = 1) { x = Math.round(x); y = Math.round(y); if (x >= 0 && y >= 0 && x < this.w && y < this.h) this.v[y * this.w + x] = k; }
    disc(cx, cy, r, k = 1) { for (let j = 0; j < this.h; j++) for (let i = 0; i < this.w; i++) { const dx = i + .5 - cx, dy = j + .5 - cy; if (dx * dx + dy * dy <= r * r) this.v[j * this.w + i] = k; } }
    rect(x, y, w, h, k = 1) { for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) this.set(i, j, k); }
    line(x0, y0, x1, y1, t = 1, k = 1) {
      const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0)) * 2 + 1;
      for (let s = 0; s <= n; s++) { const x = x0 + (x1 - x0) * s / n, y = y0 + (y1 - y0) * s / n; this.rect(Math.round(x - (t - 1) / 2), Math.round(y - (t - 1) / 2), t, t, k); }
    }
    poly(pts, k = 1) {
      for (let j = 0; j < this.h; j++) for (let i = 0; i < this.w; i++) {
        const x = i + .5, y = j + .5;
        let inside = false;
        for (let a = 0, b = pts.length - 1; a < pts.length; b = a++) {
          const [xa, ya] = pts[a], [xb, yb] = pts[b];
          if ((ya > y) !== (yb > y) && x < (xb - xa) * (y - ya) / (yb - ya) + xa) inside = !inside;
        }
        if (inside) this.v[j * this.w + i] = k;
      }
    }
    flip() { const g = new Grid(this.w, this.h); for (let j = 0; j < this.h; j++) for (let i = 0; i < this.w; i++) g.v[j * this.w + i] = this.v[j * this.w + (this.w - 1 - i)]; return g; }
  }

  // Dancing couple, 26x32 cells: he leads her into a turn under their raised hands. Two poses, the companion slot
  // alternates them on the beat.
  function couple(pose) {
    const g = new Grid(26, 32);
    const b = pose ? 1 : 0;
    // him (left): head, torso, legs mid-step
    g.disc(7.5, 6.5 + b, 2.6);
    g.poly([[4.5, 10 + b], [10.5, 10 + b], [9.8, 19 + b], [5.2, 19 + b]]);
    g.line(6, 19 + b, pose ? 4 : 6, 31, 2);
    g.line(9, 19 + b, pose ? 10 : 11, 31, 2);
    g.line(10, 11 + b, 13, 1.5, 2); // raised arm to the joined hands
    g.line(5, 11 + b, pose ? 3 : 2, 17, 2); // free arm out
    // her (right): head with bun, torso, flared skirt swinging with the turn
    g.disc(18.5, 8 - b, 2.4); g.disc(20.6, 6.5 - b, 1.2);
    g.poly([[16.4, 11.5 - b], [20.6, 11.5 - b], [20, 18], [17, 18]]);
    g.poly(pose ? [[17, 17], [20, 17], [25, 26], [13.5, 25]] : [[17, 17], [20, 17], [23.5, 25], [12, 26]]);
    g.line(17.5, 26, pose ? 17 : 16, 31, 1);
    g.line(19.5, 26, pose ? 21 : 20, 31, 1);
    g.line(16.5, 12.5 - b, 13, 1.5, 1); // her raised arm
    g.line(20.5, 13 - b, 22.5, 18, 1);
    g.rect(12, 0, 3, 3); // hands
    return g;
  }

  G.PH = { W, H, Fb, Grid, hex, to565, from565, toHex, mix, bayer, up, couple };
})(typeof window !== 'undefined' ? window : globalThis);
