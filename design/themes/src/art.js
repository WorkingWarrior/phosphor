// 320x86 pixel-art layer, the same grid as the firmware's px:: (each pixel a 2x2 block on the 640x172 screen).
// Worlds are painted here in the machine's own colors; HUD text can sit at this resolution or at full resolution on top.
(function (G) {
  const { bayer } = G.PH;
  const W = 320, H = 86;

  function hash(a) {
    a = (a ^ 61) ^ (a >>> 16); a = a + (a << 3); a = a ^ (a >>> 4); a = Math.imul(a, 0x27d4eb2d); a = a ^ (a >>> 15);
    return a >>> 0;
  }
  const rnd = (i) => hash(i | 0) / 4294967296;
  // smooth 1-D value noise in [0,1] and a few octaves of it
  function noise(x, seed = 0) {
    const i = Math.floor(x), f = x - i, u = f * f * (3 - 2 * f);
    return rnd(i * 7919 + seed * 104729) * (1 - u) + rnd((i + 1) * 7919 + seed * 104729) * u;
  }
  function fbm(x, seed = 0, oct = 4) {
    let s = 0, a = 0.5, f = 1, n = 0;
    for (let k = 0; k < oct; k++) { s += a * noise(x * f, seed + k * 31); n += a; a *= 0.5; f *= 2.03; }
    return s / n;
  }
  const clamp = (v, a = 0, b = 1) => (v < a ? a : v > b ? b : v);
  // Amiga Extra-Half-Brite: the second 32 colors are the first 32 at half brightness (each 4-bit channel shifted right)
  const half = (c) => [16, 8, 0].reduce((o, s) => o | ((Math.round(((c >> s) & 255) / 17) >> 1) * 17) << s, 0);
  const x12 = (s) => { const h = s.replace('#', ''); return parseInt(h[0] + h[0] + h[1] + h[1] + h[2] + h[2], 16); };

  class Px {
    constructor() {
      this.d = new Int32Array(W * H);
      this.wide = false;          // C64 multicolor: pixels come in horizontal pairs
      this.dm = 'bayer';          // dither: 'bayer' (4x4 ordered), 'checker' (50% only, Sierra-era), 'none'
      this.cl = [0, 0, W, H];
    }
    clip(x, y, w, h) { this.cl = [Math.max(0, x), Math.max(0, y), Math.min(W, x + w), Math.min(H, y + h)]; return this; }
    noClip() { this.cl = [0, 0, W, H]; return this; }
    _p(x, y, c) { const k = this.cl; if (x >= k[0] && y >= k[1] && x < k[2] && y < k[3]) this.d[y * W + x] = c; }
    px(x, y, c) {
      if (c == null || c < 0) return;
      x = Math.floor(x); y = Math.floor(y);
      if (this.wide) { x &= ~1; this._p(x, y, c); this._p(x + 1, y, c); } else this._p(x, y, c);
    }
    get(x, y) { x = Math.floor(x); y = Math.floor(y); return x >= 0 && y >= 0 && x < W && y < H ? this.d[y * W + x] : -1; }
    clear(c) { this.d.fill(c); return this; }
    rect(x, y, w, h, c) { for (let j = Math.floor(y); j < y + h; j++) for (let i = Math.floor(x); i < x + w; i += 1) this.px(i, j, c); return this; }
    frame(x, y, w, h, c) { this.rect(x, y, w, 1, c); this.rect(x, y + h - 1, w, 1, c); this.rect(x, y, 1, h, c); this.rect(x + w - 1, y, 1, h, c); return this; }
    fill(x, y, w, h, fn) { for (let j = Math.floor(y); j < y + h; j++) for (let i = Math.floor(x); i < x + w; i++) this.px(i, j, fn(i, j)); return this; }
    map(x, y, w, h, fn) {
      for (let j = Math.max(0, y); j < Math.min(H, y + h); j++) for (let i = Math.max(0, x); i < Math.min(W, x + w); i++) this.d[j * W + i] = fn(this.d[j * W + i], i, j);
      return this;
    }
    // ordered-dither threshold at a pixel (pairs share one in wide mode)
    th(x, y) {
      const bx = this.wide ? x >> 1 : x;
      if (this.dm === 'checker') return (bx + y) & 1 ? 0.7 : 0.3;
      if (this.dm === 'none') return 0.5;
      return bayer(bx & 3, y & 3);
    }
    // color from a ramp for a value in [0,1], dithered between neighbouring steps
    pick(ramp, f, x, y) {
      const n = ramp.length;
      if (n === 1) return ramp[0];
      f = clamp(f) * (n - 1);
      const k = Math.floor(f);
      if (k >= n - 1) return ramp[n - 1];
      return this.th(x, y) < f - k ? ramp[k + 1] : ramp[k];
    }
    disc(cx, cy, r, c) {
      for (let j = Math.floor(cy - r); j <= cy + r; j++) for (let i = Math.floor(cx - r); i <= cx + r; i++) {
        const d = Math.hypot(i + 0.5 - cx, j + 0.5 - cy);
        if (d <= r) this.px(i, j, typeof c === 'function' ? c(i, j, d / r) : c);
      }
      return this;
    }
    poly(pts, c) {
      let x0 = W, x1 = 0, y0 = H, y1 = 0;
      for (const [x, y] of pts) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
      for (let j = Math.floor(y0); j <= y1; j++) for (let i = Math.floor(x0); i <= x1; i++) {
        let inside = false;
        for (let a = 0, b = pts.length - 1; a < pts.length; b = a++) {
          const [xa, ya] = pts[a], [xb, yb] = pts[b];
          if ((ya > j + 0.5) !== (yb > j + 0.5) && i + 0.5 < (xb - xa) * (j + 0.5 - ya) / (yb - ya) + xa) inside = !inside;
        }
        if (inside) this.px(i, j, typeof c === 'function' ? c(i, j) : c);
      }
      return this;
    }
    line(x0, y0, x1, y1, c, th = 1) {
      const n = Math.max(1, Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 1.5));
      for (let s = 0; s <= n; s++) {
        const f = s / n, x = x0 + (x1 - x0) * f, y = y0 + (y1 - y0) * f, t = typeof th === 'function' ? th(f) : th;
        if (t <= 1) this.px(x, y, c);
        else for (let j = -t / 2; j < t / 2; j++) for (let i = -t / 2; i < t / 2; i++) this.px(x + i + 0.5, y + j + 0.5, c);
      }
      return this;
    }
    // a sprite from strings: key maps characters to colors, anything else is transparent
    sprite(rows, x, y, key, flip = false) {
      rows.forEach((r, j) => { const n = r.length; for (let i = 0; i < n; i++) { const c = key[r[flip ? n - 1 - i : i]]; if (c != null) this.px(x + i, y + j, c); } });
      return this;
    }
    // ---- text with the 8x8 fonts. o: s (scale), sx/sy, c, sh {dx, dy, c} shadow, ol outline color, adv (cell advance)
    tw(str, s = 1, adv = 8) { return Array.from(str).length * adv * s; }
    mask(font, str, x, y, sx = 1, sy = sx, adv = 8, alt = null) {
      const m = new Set();
      let cx = x;
      for (const ch of Array.from(str)) {
        const g = (alt && alt[ch]) || font.glyphs[ch] || font.glyphs[ch.toUpperCase()] || font.glyphs['?'];
        for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) if (g[r] & (0x80 >> c))
          for (let j = 0; j < sy; j++) for (let i = 0; i < sx; i++) m.add((y + r * sy + j) * 1024 + (cx + c * sx + i));
        cx += adv * sx;
      }
      return m;
    }
    paintMask(m, o) {
      const at = (k) => [k % 1024, Math.floor(k / 1024)];
      if (o.sh) for (const k of m) { const [x, y] = at(k); if (!m.has((y + o.sh.dy) * 1024 + x + o.sh.dx)) this.px(x + o.sh.dx, y + o.sh.dy, o.sh.c); }
      if (o.ol != null) for (const k of m) {
        const [x, y] = at(k);
        for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) if (!m.has((y + j) * 1024 + x + i)) this.px(x + i, y + j, o.ol);
      }
      for (const k of m) { const [x, y] = at(k); this.px(x, y, typeof o.c === 'function' ? o.c(x, y) : o.c); }
    }
    text(font, str, x, y, o = {}) {
      const sx = o.sx || o.s || 1, sy = o.sy || o.s || 1;
      const w = this.wide; this.wide = false;
      this.paintMask(this.mask(font, str, x, y, sx, sy, o.adv || 8), o);
      this.wide = w;
      return x + this.tw(str, sx, o.adv || 8);
    }
    // "hh:mm" in big digits: digits on a fixed pitch, a narrow colon. Returns the right edge.
    bigTime(font, str, x, y, o) {
      const sx = o.sx || o.s, sy = o.sy || o.s, gap = o.gap == null ? sx : o.gap, dw = (font.bold ? 7 : 6) * sx;
      // big clock digits use an unslashed zero: rows 1-5 take the plain sides of row 1
      if (!font.plainZero) { const z = Array.from(font.glyphs['0']); for (let r = 2; r <= 5; r++) z[r] = z[1]; font.plainZero = { 0: z }; }
      const m = new Set();
      let cx = x;
      for (const ch of Array.from(str)) {
        if (ch === ':' || ch === '.') { for (const k of this.mask(font, ch, cx - 2 * sx, y, sx, sy)) m.add(k); cx += (font.bold ? 2 : 1) * sx + gap; continue; }
        if (ch === '°') { for (const k of this.mask(font, ch, cx, y, Math.ceil(sx / 2), Math.ceil(sy / 2))) m.add(k); cx += 5 * Math.ceil(sx / 2) + gap; continue; }
        for (const k of this.mask(font, ch, cx, y, sx, sy, 8, font.plainZero)) m.add(k);
        cx += dw + gap;
      }
      const w = this.wide; this.wide = false;
      this.paintMask(m, o);
      this.wide = w;
      return cx - gap;
    }
    bigW(font, str, s, gap = s) {
      let w = 0;
      for (const ch of Array.from(str)) w += ch === ':' || ch === '.' ? (font.bold ? 2 : 1) * s + gap : ch === '°' ? 5 * Math.ceil(s / 2) + gap : (font.bold ? 7 : 6) * s + gap;
      return w - gap;
    }
    // copy to the 640x172 frame, 2x2 per pixel
    toFb(fb) {
      const FW = 640;
      for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
        const c = this.d[y * W + x], i = (y * 2) * FW + x * 2;
        fb.d[i] = c; fb.d[i + 1] = c; fb.d[i + FW] = c; fb.d[i + FW + 1] = c;
      }
      return fb;
    }
  }
  G.ART = { W, H, Px, hash, rnd, noise, fbm, clamp, half, x12 };
})(typeof window !== 'undefined' ? window : globalThis);
