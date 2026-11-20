// The five Phosphor themes and their screens. Each screen draws one 640x172 frame at time t (ms) into a Fb.
(function (G) {
  const { Fb, hex, up, mix, bayer, couple, from565 } = G.PH;
  const F = G.FONTS;
  const W = 640, H = 172;

  // ---------------------------------------------------------------- demo state, strings
  const DEMO = { hh: '21', mm: '47', sec: 30, alarm: '07:30', batt: 95, station: 'NIGHTRIDE FM', title: 'Night Drive',
    ch: 7, genre: 1, stationIndex: 6, stationCount: 20 };
  const GENRES = ['80S', 'SYNTH', '90S', 'CHILL', 'LOFI', 'JAZZ', 'TR90'];
  const GENRES_MIX = ['80s', 'Synth', '90s', 'Chill', 'Lo-fi', 'Jazz', 'TR90'];
  const STR = {
    en: { wd: 'Fri', date: '20 Nov', wdLong: 'Friday', dateLong: '20 November', year: '2026', alarm: 'Alarm', wake: 'Wake up!',
      found: 'Found', loading: 'Loading', hint: 'Tap < > to change', battery: 'Battery', clock: 'Clock', radio: 'Radio',
      settings: 'Settings', prefs: 'Preferences', scene: 'Seaside', tapStop: 'Tap to stop.', meditation: 'Wake-up Meditation',
      status: [['F1', ' Help  '], ['F2', ' Alarm  '], ['F10', ' Setup']], statusSet: [['Tap', ' Pick  '], ['< >', ' Change  '], ['F1', ' Help']],
      items: ['Theme', 'Scene colors', '12/24h', '°C/°F', 'Language', 'Brightness', 'City', 'Pages', 'Wi-Fi', 'Help', 'About'] },
    tr: { wd: 'Cum', date: '20 Kas', wdLong: 'Cuma', dateLong: '20 Kasım', year: '2026', alarm: 'Alarm', wake: 'Uyan!',
      found: 'Bulundu', loading: 'Yükleniyor', hint: '< > ile değiştir', battery: 'Pil', clock: 'Saat', radio: 'Radyo',
      settings: 'Ayarlar', prefs: 'Tercihler', scene: 'Sahil', tapStop: 'Durdurmak için dokun.', meditation: 'Uyanış Meditasyonu',
      status: [['F1', ' Yardım  '], ['F2', ' Alarm  '], ['F10', ' Ayarlar']], statusSet: [['Dokun', ' Seç  '], ['< >', ' Değiştir  '], ['F1', ' Yardım']],
      items: ['Tema', 'Sahne renkleri', '12/24 saat', '°C/°F', 'Dil', 'Parlaklık', 'Şehir', 'Sayfalar', 'Wi-Fi', 'Yardım', 'Hakkında'] },
  };
  let LANG = 'en';
  const sec = (t) => (DEMO.sec + Math.floor(t / 1000)) % 60;
  const blink = (t, ms = 500) => Math.floor(t / ms) % 2 === 0;
  const hhmm = () => DEMO.hh + ':' + DEMO.mm;

  function levels(t) {
    const out = [];
    for (let i = 0; i < 14; i++) {
      const base = 9.2 - i * 0.38;
      const v = base * (0.5 + 0.5 * Math.abs(Math.sin(t / 170 * (1 + i * 0.13) + i * 1.7))) * (0.78 + 0.22 * Math.sin(t / 830 + i));
      out.push(Math.max(1, Math.min(10, Math.round(v))));
    }
    return out;
  }
  const peak = (l, i) => Math.min(10, l + 1 + (i % 3 === 0 ? 1 : 0));

  // font-cell iterator: positions of the lit cells of str in font at scale s
  function cells(font, str, x, y, s, adv = 8, sy = s) {
    const out = [];
    let cx = x;
    for (const ch of Array.from(str)) {
      const g = font.glyphs[ch] || font.glyphs['?'];
      for (let r = 0; r < 8; r++) for (let c = 0; c < 8; c++) if (g[r] & (0x80 >> c)) out.push([cx + c * s, y + r * sy, c, r]);
      cx += adv * s;
    }
    out.end = cx;
    return out;
  }
  // big "hh:mm" with a narrow colon. Returns cells with .colon marking the colon's cells.
  function bigTime(font, x, y, s, gap) {
    const dw = (font.bold ? 7 : 6) * s, cw = (font.bold ? 2 : 1) * s;
    const out = [];
    let cx = x;
    const put = (str, colon) => { for (const c of cells(font, str, cx, y, s)) { c.colon = colon; out.push(c); } };
    put(DEMO.hh[0]); cx += dw + gap; put(DEMO.hh[1]); cx += dw + gap;
    put(':', true); cx = cx + cw + gap; cx -= 0;
    // ':' glyph sits at columns 2..: shift it left so its lit columns start at the slot
    for (const c of out) if (c.colon) c[0] -= 2 * s;
    put(DEMO.mm[0]); cx += dw + gap; put(DEMO.mm[1]); cx += dw;
    out.end = cx;
    return out;
  }
  const fit = (str, maxW, scales) => { for (const s of scales) if (Array.from(str).length * 8 * s - s <= maxW) return s; return scales[scales.length - 1]; };

  // ---------------------------------------------------------------- the lighthouse scene, as materials
  const M = { SKY: 1, STAR: 2, MOON: 3, LAND: 4, SEA: 5, WAVE: 6, GLINT: 7, ROCK: 8, TW: 9, TR: 10, IRON: 11, LAMP: 12,
    SAND: 13, FOAM: 14, PALM: 15, SAIL: 16, HULL: 17 };
  const MN = Object.fromEntries(Object.entries(M).map(([k, v]) => [v, k.toLowerCase()]));
  const HZ = 70;
  function hash(a) {
    a = (a ^ 61) ^ (a >>> 16); a = a + (a << 3); a = a ^ (a >>> 4); a = Math.imul(a, 0x27d4eb2d); a = a ^ (a >>> 15);
    return a >>> 0;
  }
  const sceneCache = new Map();
  function sceneModel(t) {
    const key = Math.floor(t / 50);
    if (sceneCache.has(key)) return sceneCache.get(key);
    const mat = new Uint8Array(W * H), val = new Float32Array(W * H), beam = new Float32Array(W * H);
    const put = (x, y, m, v = 0) => {
      x = Math.round(x); y = Math.round(y);
      if (x < 0 || y < 0 || x >= W || y >= H) return;
      mat[y * W + x] = m; val[y * W + x] = v;
    };
    const poly = (pts, m, vf) => {
      let x0 = W, x1 = 0, y0 = H, y1 = 0;
      for (const [x, y] of pts) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
      for (let j = Math.floor(y0); j <= y1; j++) for (let i = Math.floor(x0); i <= x1; i++) {
        let inside = false;
        for (let a = 0, b = pts.length - 1; a < pts.length; b = a++) {
          const [xa, ya] = pts[a], [xb, yb] = pts[b];
          if ((ya > j + .5) !== (yb > j + .5) && i + .5 < (xb - xa) * (j + .5 - ya) / (yb - ya) + xa) inside = !inside;
        }
        if (inside) put(i, j, m, vf ? vf(i, j) : 0);
      }
    };
    const thick = (x0, y0, x1, y1, t0, t1, m, v) => {
      const n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 2);
      for (let s = 0; s <= n; s++) {
        const f = s / n, x = x0 + (x1 - x0) * f, y = y0 + (y1 - y0) * f, th = t0 + (t1 - t0) * f;
        for (let j = -th / 2; j < th / 2; j++) for (let i = -th / 2; i < th / 2; i++) put(x + i, y + j, m, v);
      }
    };
    // sky, stars, moon
    for (let y = 0; y < HZ; y++) for (let x = 0; x < W; x++) put(x, y, M.SKY, Math.pow(y / HZ, 1.6));
    for (let k = 0; k < 120; k++) {
      const x = hash(k * 7 + 1) % W, y = hash(k * 13 + 5) % (HZ - 14);
      const tw = 0.45 + 0.55 * Math.abs(Math.sin(t / 900 + k * 1.3));
      put(x, y, M.STAR, tw);
      if (k % 11 === 0) for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) put(x + dx, y + dy, M.STAR, tw * 0.5);
    }
    for (let y = 8; y < 44; y++) for (let x = 230; x < 272; x++) {
      const d1 = Math.hypot(x + .5 - 250, y + .5 - 25), d2 = Math.hypot(x + .5 - 256, y + .5 - 21);
      if (d1 <= 11.5 && d2 > 10) put(x, y, M.MOON, 1 - (d1 / 11.5) * 0.45);
    }
    // headland on the left horizon, a far islet on the right
    for (let x = 0; x < 170; x++) {
      const k = 1 - x / 170, hgt = 11 * k * k + 2.5 * Math.sin(x / 7) * k;
      for (let y = Math.floor(HZ - hgt); y < HZ; y++) put(x, y, M.LAND, 0);
    }
    for (let x = 590; x < 640; x++) { const hgt = 4 * Math.sin((x - 590) / 50 * Math.PI); for (let y = Math.floor(HZ - hgt); y < HZ; y++) put(x, y, M.LAND, 0); }
    // sea
    for (let y = HZ; y < H; y++) {
      const d = (y - HZ) / 70;
      for (let x = 0; x < W; x++) {
        let m = M.SEA, v = Math.min(1, d);
        const ph = x * (0.05 - d * 0.02) + Math.sin(x * 0.011 + y * 0.3) * 1.6 + (y - HZ) * 0.6 - t / 700;
        if (Math.sin(ph) > 0.94 - d * 0.1) m = M.WAVE;
        const gw = 3 + (y - HZ) * 0.42;
        if (Math.abs(x - 250) < gw && hash(x * 31 + y * 977 + Math.floor(t / 300) * 7) % 4 === 0) { m = M.GLINT; v = hash(x + y * 640) % 100 / 100; }
        put(x, y, m, v);
      }
    }
    // sailboat on the horizon
    const bob = Math.round(Math.sin(t / 900));
    poly([[134, HZ - 2 + bob], [168, HZ - 2 + bob], [163, HZ + 3 + bob], [139, HZ + 3 + bob]], M.HULL);
    poly([[152, HZ - 28 + bob], [152, HZ - 3 + bob], [167, HZ - 3 + bob]], M.SAIL, (i) => (i - 152) / 15);
    poly([[150, HZ - 22 + bob], [150, HZ - 3 + bob], [139, HZ - 3 + bob]], M.SAIL, () => 0.2);
    thick(151, HZ - 29 + bob, 151, HZ - 2 + bob, 1, 1, M.HULL);
    // rocks and lighthouse
    poly([[432, 150], [444, 124], [462, 110], [490, 104], [512, 106], [532, 116], [548, 130], [562, 150]], M.ROCK, (i, j) => (j - 104) / 46);
    for (let y = 46; y < 110; y++) {
      const hw = 8 + (y - 46) / 64 * 6, band = Math.floor((y - 46) / 12);
      for (let x = Math.floor(495 - hw); x < 495 + hw; x++) put(x, y, band % 2 ? M.TR : M.TW, (x - (495 - hw)) / (2 * hw));
    }
    for (let x = 481; x < 510; x++) { put(x, 44, M.IRON); put(x, 45, M.IRON); }
    for (let x = 481; x < 510; x += 4) { put(x, 41, M.IRON); put(x, 42, M.IRON); put(x, 43, M.IRON); }
    for (let x = 481; x < 510; x++) put(x, 40, M.IRON);
    for (let y = 28; y < 40; y++) for (let x = 487; x < 504; x++) put(x, y, (x === 491 || x === 499) ? M.IRON : M.LAMP, 1);
    poly([[484, 28.5], [495, 18], [506, 28.5]], M.IRON);
    thick(495, 13, 495, 18, 1, 1, M.IRON);
    // beam, sweeping round: long to the left, a short stub when it turns away
    const ph = (t / 5200) % 1, c = Math.cos(ph * Math.PI * 2);
    const len = c > 0 ? 360 * c + 30 : 0, back = c < 0 ? 120 * -c : 0;
    for (let y = 0; y < 110; y++) for (let x = 0; x < W; x++) {
      const m = mat[y * W + x];
      if (m >= M.ROCK && m <= M.LAMP) continue;
      let b = 0;
      for (const [dir, L] of [[-1, len], [1, back]]) {
        if (!L) continue;
        const dx = (x - 495) * dir, dy = y - 34;
        if (dx < 6 || dx > L) continue;
        const hw = 1.5 + dx * 0.06;
        const e = Math.abs(dy - dx * 0.035);
        if (e < hw) b = Math.max(b, Math.pow(1 - dx / L, 0.6) * (1 - (e / hw) * 0.5));
      }
      beam[y * W + x] = b;
    }
    // beach and foam
    for (let x = 0; x < W; x++) {
      const f = 138 + 2.5 * Math.sin(x * 0.022 + t / 1100) + 1.5 * Math.sin(x * 0.061 - t / 700);
      for (let y = Math.floor(f); y < H; y++) {
        const m = mat[y * W + x];
        if (m === M.ROCK) continue;
        put(x, y, y < f + 2 ? M.FOAM : M.SAND, (y - f) / (H - f));
      }
    }
    // palm, leaning into the frame from the left
    const trunk = (s) => [58 + 30 * s + 14 * s * s, 171 - 128 * s];
    for (let s = 0; s <= 1; s += 0.005) { const [x, y] = trunk(s); const th = 6 - 3 * s; for (let i = -th / 2; i < th / 2; i++) put(x + i, y, M.PALM, 0.35); }
    const [cx, cy] = trunk(1);
    for (const [dx, dy] of [[-48, 14], [-40, -6], [-18, -16], [16, -16], [40, -4], [50, 16], [-28, 28], [26, 30], [4, 34]]) {
      const n = 80;
      for (let s = 0; s <= n; s++) {
        const f = s / n, sag = (-10 * f + 20 * f * f) * Math.min(1, Math.abs(dx) / 40);
        const x = cx + dx * f, y = cy + dy * f + sag, th = 3.2 - 2.4 * f;
        for (let j = -th / 2; j < th / 2; j++) put(x, y + j, M.PALM, 0.35);
        if (s % 6 === 3 && f > 0.15) for (let k = 1; k < 5 - 3 * f; k++) put(x + (dx > 0 ? -k * 0.4 : k * 0.4), y + k, M.PALM, 0.35);
      }
    }
    put(cx - 2, cy + 4, M.PALM, 0.35); put(cx + 2, cy + 5, M.PALM, 0.35);
    const out = { mat, val, beam };
    if (sceneCache.size > 40) sceneCache.clear();
    sceneCache.set(key, out);
    return out;
  }
  // ramps: material name -> colors (dark/far to light/near as the material's value grows). band: no dither, nearest step.
  function renderScene(fb, ramps, t, opt = {}) {
    const S = sceneModel(t);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
      const xs = opt.wide ? (x & ~1) : x, bx = opt.wide ? (x >> 1) : x;
      const i = y * W + xs, m = S.mat[i];
      const ramp = ramps[MN[m]] || ramps.sky;
      const n = ramp.length;
      let c;
      if (n === 1) c = ramp[0];
      else {
        const f = Math.max(0, Math.min(1, S.val[i])) * (n - 1);
        if (opt.band) c = ramp[Math.round(f)];
        else { const k = Math.floor(f); c = (k + 1 < n && bayer(bx, y) < f - k) ? ramp[k + 1] : ramp[k]; }
      }
      const b = S.beam[i];
      if (b > 0 && ramps.beam) {
        const br = ramps.beam, k = Math.floor(b * br.length + (opt.band ? 0.5 : bayer(bx, y)) - 0.25);
        if (k > 0) c = br[Math.min(br.length, k) - 1];
      }
      fb.d[y * W + x] = c;
    }
  }

  // ---------------------------------------------------------------- settings data shared by every theme
  const THEME_NAMES = ['VFD', 'CGA', 'EGA', 'C64-style', 'Amiga-style'];
  const x12 = (s) => { const h = s.replace('#', ''); return parseInt(h[0] + h[0] + h[1] + h[1] + h[2] + h[2], 16); };

  // ================================================================ VFD
  const VFD = (() => {
    const c = from565;
    const variants = { a: { label: 'Camgöbeği + magenta (bugünkü)', BG: c(0x0041), GHOST: c(0x0904), FAINT: c(0x0082), HALO: c(0x0248),
      LIT: c(0x77BC), DIM: c(0x44B1), HOT: c(0xFA16), HOTH: c(0x6888), KELP: c(0x4D72) } };
    const roles = (P) => ({ BG: P.BG, GHOST: P.GHOST, FAINT: P.FAINT, HALO: P.HALO, LIT: P.LIT, DIM: P.DIM, HOT: P.HOT, HOTH: P.HOTH, KELP: P.KELP });
    const LIT = (P) => ({ c: P.LIT, h: P.HALO }), HOT = (P) => ({ c: P.HOT, h: P.HOTH });
    const vt = (fb, s, x, y, p, d, ink) => fb.vfdText(up(s, LANG), x, y, p, d, ink);
    const BELL = ['...#...', '..###..', '.#####.', '.#####.', '.#####.', '#######', '...#...'];
    const bell = (fb, x, y, s, c, h) => {
      BELL.forEach((row, j) => [...row].forEach((k, i) => { if (k === '#') { if (h != null) fb.rect(x + i * s - 1, y + j * s - 1, s + 2, s + 2, h); } }));
      BELL.forEach((row, j) => [...row].forEach((k, i) => { if (k === '#') fb.rect(x + i * s, y + j * s, s, s, c); }));
    };
    function edge(fb, P, t) {
      for (let x = 8; x < W; x += 32) { fb.rect(x, 2, 2, 2, P.GHOST); fb.rect(x, 168, 2, 2, P.GHOST); }
    }
    return {
      key: 'vfd', name: 'VFD', font: null, variants, roles,
      desc: 'Bugünkü görünüm: siyah cam, sönük segment hayaletleri, camgöbeği ışık, magenta vurgu. Font: mevcut 5x7 nokta matris (VfdFont.h).',
      clock(fb, { t, P, S }) {
        fb.clear(P.BG); edge(fb, P, t);
        fb.seg(hhmm(), 20, 24, 78, 124, 12, 12, { c: P.LIT, h: P.HALO, g: P.GHOST });
        const sx = 8 + sec(t) * 624 / 60;
        fb.rect(sx - 1, 165, 6, 6, P.HOTH); fb.rect(sx, 166, 4, 4, P.HOT);
        vt(fb, 'AL', 412, 26, 2, 2, HOT(P));
        vt(fb, '24H', 412, 48, 2, 2, { c: P.GHOST });
        vt(fb, S.wd, 492, 26, 3, 2, LIT(P));
        vt(fb, S.date, 492, 54, 2, 2, { c: P.DIM });
        fb.rect(492, 80, 132, 1, P.GHOST);
        bell(fb, 492, 94, 2, P.HOT, P.HOTH);
        vt(fb, S.alarm, 512, 96, 2, 2, HOT(P));
        fb.seg(DEMO.alarm, 492, 120, 24, 40, 5, 5, { c: P.HOT, h: P.HOTH });
        fb.frame(412, 120, 30, 16, P.DIM); fb.rect(442, 125, 3, 6, P.DIM); fb.rect(415, 123, 24, 10, P.KELP);
        vt(fb, DEMO.batt + '%', 412, 146, 2, 2, { c: P.DIM });
      },
      radio(fb, { t, P, S }) {
        fb.clear(P.BG);
        vt(fb, 'CH 0' + DEMO.ch, 16, 10, 2, 2, { c: P.DIM });
        vt(fb, GENRES[DEMO.genre], 96, 10, 2, 2, LIT(P));
        vt(fb, 'STEREO', 440, 10, 2, 2, HOT(P));
        vt(fb, hhmm(), 568, 10, 2, 2, { c: P.DIM });
        vt(fb, DEMO.station.padEnd(13), 16, 36, 4, 3, { c: P.LIT, h: P.HALO, g: P.GHOST });
        fb.clip(16, 70, 336, 100); vt(fb, DEMO.title, 16, 80, 2, 2, { c: P.DIM }); fb.noClip();
        for (let x = 16; x <= 336; x += 8) { const major = (x - 16) % 64 === 0; fb.rect(x, major ? 106 : 110, major ? 2 : 1, major ? 10 : 6, major ? P.DIM : P.GHOST); }
        const nx = 16 + Math.round(DEMO.stationIndex * 320 / (DEMO.stationCount - 1));
        fb.rect(nx - 3, 100, 6, 22, P.HOTH); fb.rect(nx - 1, 102, 2, 18, P.HOT);
        // companion: the couple on a small dot-matrix panel
        const g = couple(Math.floor(t / 480) % 2);
        for (let j = 0; j < 32; j++) for (let i = 0; i < 26; i++) fb.rect(343 + i * 3, 28 + j * 3, 2, 2, P.FAINT);
        g.v.forEach((k, idx) => { if (k) fb.rect(343 + (idx % 26) * 3 - 1, 28 + Math.floor(idx / 26) * 3 - 1, 4, 4, P.HALO); });
        g.v.forEach((k, idx) => { if (k) fb.rect(343 + (idx % 26) * 3, 28 + Math.floor(idx / 26) * 3, 2, 2, P.LIT); });
        const L = levels(t);
        L.forEach((lv, i) => {
          const x = 426 + i * 14, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) {
            const y = 115 - r * 9, on = r < lv, isPk = r === pk - 1 && !on, hot = r >= 8;
            if (on || isPk) fb.rect(x - 1, y - 1, 13, 8, hot || isPk ? P.HOTH : P.HALO);
            fb.rect(x, y, 11, 6, isPk || (on && hot) ? P.HOT : on ? P.LIT : P.GHOST);
          }
        });
        GENRES.forEach((k, i) => {
          const x = 16 + i * 87, cur = i === DEMO.genre;
          fb.frame(x, 136, 83, 28, cur ? P.DIM : P.GHOST);
          vt(fb, k, x + 41 - Math.floor(fb.vfdW(k, 2) / 2), 143, 2, 2, cur ? LIT(P) : { c: P.GHOST });
        });
      },
      scene(fb, { t, P }) {
        renderScene(fb, {
          sky: [P.BG, P.BG, P.FAINT, P.GHOST], star: [P.GHOST, P.DIM, P.LIT], moon: [P.DIM, P.LIT], land: [P.BG],
          sea: [P.GHOST, P.FAINT, P.BG], wave: [P.HALO, P.DIM], glint: [P.DIM, P.LIT], rock: [P.BG, P.FAINT],
          tw: [P.LIT], tr: [P.HOTH], iron: [P.HALO], lamp: [P.HOT], sand: [P.HOTH, P.BG], foam: [P.LIT],
          palm: [P.HALO], sail: [P.DIM], hull: [P.HOTH], beam: [P.HOTH, P.HOT],
        }, t);
        vt(fb, hhmm(), 568, 150, 2, 2, LIT(P));
      },
      settings(fb, { t, P, S, theme }) {
        fb.clear(P.BG);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 10 + i * 26, sel = i === 0;
          if (sel) { fb.rect(15, y + 1, 4, 12, P.HOTH); fb.rect(16, y + 2, 2, 10, P.HOT); fb.rect(18, y + 4, 2, 6, P.HOT); fb.rect(20, y + 6, 2, 2, P.HOT); }
          vt(fb, it, 30, y, 2, 2, sel ? LIT(P) : { c: P.DIM });
        });
        for (let k = 0; k < 11; k++) fb.rect(240, 12 + k * 14, 4, 4, k < 6 ? P.DIM : P.GHOST);
        fb.rect(254, 8, 1, 156, P.GHOST);
        vt(fb, S.items[0], 272, 12, 2, 2, { c: P.DIM });
        vt(fb, '1/5', 590, 12, 2, 2, { c: P.GHOST });
        const tri = (x, y, dir) => {
          for (let r = 0; r < 9; r++) { const n = 5 - Math.abs(r - 4); for (let k = 0; k < n; k++) { const col = dir < 0 ? 4 - k : k; fb.rect(x + col * 4 - 1, y + r * 4 - 1, 4, 4, P.HALO); } }
          for (let r = 0; r < 9; r++) { const n = 5 - Math.abs(r - 4); for (let k = 0; k < n; k++) { const col = dir < 0 ? 4 - k : k; fb.rect(x + col * 4, y + r * 4, 2, 2, P.LIT); } }
        };
        fb.frame(268, 40, 52, 70, P.DIM); tri(285, 57, -1);
        fb.frame(572, 40, 52, 70, P.DIM); tri(589, 57, 1);
        const name = up(theme).replace('-STYLE', '');
        const field = 9, start = 338;
        vt(fb, ' '.repeat(Math.floor((field - name.length) / 2)) + name + ' '.repeat(Math.ceil((field - name.length) / 2)), start, 58, 4, 3, { c: P.LIT, h: P.HALO, g: P.GHOST });
        for (let k = 0; k < 5; k++) { const x = 412 + k * 20; if (k === 0) { fb.rect(x - 1, 117, 8, 8, P.HOTH); fb.rect(x, 118, 6, 6, P.HOT); } else fb.rect(x + 1, 119, 4, 4, P.GHOST); }
        vt(fb, S.hint, 272, 146, 2, 2, { c: P.GHOST });
      },
    };
  })();

  // ================================================================ CGA
  const CGA = (() => {
    const variants = {
      a: { label: 'Palet 1: camgöbeği, magenta, beyaz', k: 0x000000, c: 0x55FFFF, m: 0xFF55FF, w: 0xFFFFFF },
      b: { label: 'Palet 0: yeşil, kırmızı, sarı', k: 0x000000, c: 0x55FF55, m: 0xFF5555, w: 0xFFFF55 },
    };
    const roles = (V) => ({ BG: V.k, GHOST: { flat: mix(V.k, V.c, 0.25), dither: ['BG', 'DIM', 0.25] }, FAINT: { flat: mix(V.k, V.c, 0.125), dither: ['BG', 'DIM', 0.125] },
      HALO: V.c, LIT: V.w, DIM: V.c, HOT: V.m, HOTH: V.m, KELP: V.c });
    const T = (fb, V, s, x, y, sc, c, lang, sh) => fb.text(F.cga, up(s, lang), x, y, { s: sc, c, sh: sh ? { dx: sc, dy: sc, c: sh } : undefined });
    const ghost = (fb, V, x, y, w, h) => fb.stip(x, y, w, h, V.c, 0.25);
    const block = (fb, V, x, y, w, h, face, sh = V.m, d = 4) => { fb.stip(x + d, y + d, w, h, sh, 0.5); fb.rect(x, y, w, h, face); };
    return {
      key: 'cga', name: 'CGA', font: 'cga', variants, roles,
      desc: '4 renk, gerisi dither. Kalın kare 8x8 blok font, magenta gölge, sönük şeyler %25 dither.',
      clock(fb, { t, P: V, S, lang }) {
        fb.clear(V.k);
        const on = sec(t) % 2 === 0;
        for (const [x, y, , , colon] of bigTime(F.cga, 14, 30, 12, 12).map((c) => [c[0], c[1], 0, 0, c.colon])) if (!colon || on) fb.stip(x + 6, y + 6, 12, 12, V.m, 0.5);
        for (const c of bigTime(F.cga, 14, 30, 12, 12)) if (!c.colon || on) fb.rect(c[0], c[1], 12, 12, V.w);
        const lit = Math.floor(sec(t) / 2.5);
        for (let k = 0; k < 24; k++) { const x = 14 + k * 17; if (k < lit) fb.rect(x, 130, 15, 8, V.c); else ghost(fb, V, x, 130, 15, 8); }
        T(fb, V, S.wd, 446, 28, 4, V.w, lang, V.m);
        T(fb, V, S.date, 446, 66, 2, V.c, lang);
        fb.stip(446, 88, 180, 2, V.c, 0.5);
        const B = ['...#...', '..###..', '.#####.', '.#####.', '#######', '...#...'];
        B.forEach((row, j) => [...row].forEach((k, i) => { if (k === '#') fb.rect(446 + i * 2, 100 + j * 2, 2, 2, V.m); }));
        T(fb, V, S.alarm, 466, 98, 2, V.m, lang);
        T(fb, V, DEMO.alarm, 446, 120, 3, V.w, lang, V.m);
        fb.frame(590, 122, 28, 16, V.c, 2); fb.rect(618, 127, 3, 6, V.c); fb.rect(593, 125, Math.round(22 * DEMO.batt / 100), 10, V.w);
      },
      radio(fb, { t, P: V, S, lang }) {
        fb.clear(V.k);
        // cassette shell
        const sx = 10, sy = 6, sw = 324, sh = 122;
        fb.rect(sx + 4, sy, sw - 8, sh, V.k); fb.rect(sx, sy + 4, sw, sh - 8, V.k);
        fb.stip(sx + 2, sy + 2, sw - 4, sh - 4, V.m, 0.25);
        fb.rect(sx + 4, sy, sw - 8, 2, V.w); fb.rect(sx + 4, sy + sh - 2, sw - 8, 2, V.w);
        fb.rect(sx, sy + 4, 2, sh - 8, V.w); fb.rect(sx + sw - 2, sy + 4, 2, sh - 8, V.w);
        fb.rect(sx + 2, sy + 2, 2, 2, V.w); fb.rect(sx + sw - 4, sy + 2, 2, 2, V.w); fb.rect(sx + 2, sy + sh - 4, 2, 2, V.w); fb.rect(sx + sw - 4, sy + sh - 4, 2, 2, V.w);
        for (const [x, y] of [[16, 12], [324, 12], [16, 118], [324, 118]]) fb.rect(x - 2, y - 2, 4, 4, V.c);
        // label
        fb.rect(24, 14, 296, 84, V.w);
        fb.rect(24, 14, 296, 6, V.m); fb.rect(24, 92, 296, 6, V.c);
        const name = up(DEMO.station, lang);
        T(fb, V, name, 172 - Math.floor(fb.tw(name, 2) / 2), 25, 2, V.k, lang);
        T(fb, V, 'A', 40, 54, 4, V.m, lang);
        // window and reels
        fb.rect(92, 46, 162, 44, V.k); fb.frame(92, 46, 162, 44, V.c, 2);
        fb.clip(94, 48, 252, 88);
        const rot = t / 260;
        for (const [cx, r] of [[130, 19], [216, 15]]) {
          fb.disc(cx, 68, r, V.k); fb.fill(cx - r, 68 - r, 2 * r, 2 * r, (x, y) => (Math.hypot(x + .5 - cx, y + .5 - 68) <= r && Math.hypot(x + .5 - cx, y + .5 - 68) > 9 && bayer(x, y) < 0.5) ? V.c : null);
          fb.ring(cx, 68, 6, 9, V.w);
          for (let k = 0; k < 3; k++) { const a = rot + k * Math.PI * 2 / 3; fb.rect(Math.round(cx + Math.cos(a) * 4.5) - 1, Math.round(68 + Math.sin(a) * 4.5) - 1, 3, 3, V.w); }
        }
        fb.rect(130, 86, 86, 2, V.c);
        fb.noClip();
        const play = blink(t, 600);
        if (play) T(fb, V, '▶', 270, 52, 2, V.m, lang);
        T(fb, V, 'C90', 266, 70, 2, V.k, lang);
        // lower shell with the tape holes
        const pts = [[60, 104], [284, 104], [298, 126], [46, 126]];
        for (let y = 104; y < 126; y++) { const f = (y - 104) / 22, x0 = 60 - 14 * f, x1 = 284 + 14 * f; fb.rect(x0, y, x1 - x0, 1, V.k); fb.rect(x0, y, 2, 1, V.w); fb.rect(x1 - 2, y, 2, 1, V.w); }
        fb.rect(60, 104, 224, 2, V.w);
        for (const x of [88, 124, 208, 244]) { fb.rect(x, 112, 10, 8, V.c); fb.rect(x + 2, 114, 6, 4, V.k); }
        for (const x of [160, 180]) fb.rect(x, 114, 4, 4, V.w);
        // companion
        const g = couple(Math.floor(t / 480) % 2);
        fb.stip(352, 108, 60, 6, V.c, 0.25);
        fb.grid(g, 356 + 2, 42 + 2, 2, null, (f, x, y) => f.stip(x, y, 2, 2, V.m, 0.5));
        fb.grid(g, 356, 42, 2, V.w);
        // spectrum
        T(fb, V, 'STEREO', 426, 6, 2, V.m, lang);
        T(fb, V, hhmm(), 548, 6, 2, V.w, lang);
        levels(t).forEach((lv, i) => {
          const x = 426 + i * 14, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) {
            const y = 120 - r * 9;
            if (r < lv) fb.rect(x, y, 11, 7, r < 6 ? V.c : r < 8 ? V.w : V.m);
            else if (r === pk - 1) fb.rect(x, y, 11, 7, V.m);
            else ghost(fb, V, x, y, 11, 7);
          }
        });
        GENRES.forEach((k, i) => {
          const x = 16 + i * 87, cur = i === DEMO.genre;
          if (cur) block(fb, V, x, 138, 83, 28, V.m, V.c, 3); else fb.frame(x, 138, 83, 28, V.c, 2);
          T(fb, V, k, x + 42 - Math.floor(fb.tw(k, 2) / 2), 145, 2, cur ? V.w : V.c, lang);
        });
      },
      scene(fb, { t, P: V, lang }) {
        renderScene(fb, {
          sky: [V.k, V.k, V.k, V.m], star: [V.c, V.w], moon: [V.w], land: [V.k], sea: [V.c, V.k, V.k, V.k], wave: [V.c, V.w],
          glint: [V.w], rock: [V.k, V.m], tw: [V.w], tr: [V.m], iron: [V.c], lamp: [V.w], sand: [V.m, V.k], foam: [V.w],
          palm: [V.k, V.c], sail: [V.w], hull: [V.m], beam: [V.m, V.w],
        }, t);
        T(fb, V, hhmm(), 548, 148, 2, V.w, lang, V.m);
      },
      settings(fb, { t, P: V, S, lang, theme }) {
        fb.clear(V.k);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 8 + i * 26, sel = i === 0;
          if (sel) block(fb, V, 8, y, 228, 24, V.m, V.c, 3);
          T(fb, V, it, 18, y + 5, 2, sel ? V.w : V.c, lang);
        });
        ghost(fb, V, 246, 8, 6, 154); fb.rect(246, 8, 6, Math.round(154 * 6 / 11), V.w);
        T(fb, V, S.items[0], 268, 10, 2, V.c, lang);
        T(fb, V, '1/5', 632 - fb.tw('1/5', 2), 10, 2, V.c, lang);
        block(fb, V, 268, 38, 54, 68, V.c); T(fb, V, '◀', 281, 58, 4, V.k, lang);
        block(fb, V, 572, 38, 54, 68, V.c); T(fb, V, '▶', 587, 58, 4, V.k, lang);
        const name = up(theme, lang), s = fit(name, 236, [4, 3, 2]);
        T(fb, V, name, 449 - Math.floor(fb.tw(name, s) / 2), 72 - Math.floor(7 * s / 2), s, V.w, lang, V.m);
        for (let k = 0; k < 5; k++) { const x = 404 + k * 18; if (k === 1) fb.rect(x, 114, 10, 10, V.w); else ghost(fb, V, x, 114, 10, 10); }
        T(fb, V, S.hint, 268, 146, 2, V.c, lang);
      },
    };
  })();

  // ================================================================ EGA
  const E = { k: 0x000000, b: 0x0000AA, g: 0x00AA00, c: 0x00AAAA, r: 0xAA0000, m: 0xAA00AA, br: 0xAA5500, lg: 0xAAAAAA,
    dg: 0x555555, lb: 0x5555FF, lgn: 0x55FF55, lc: 0x55FFFF, lr: 0xFF5555, lm: 0xFF55FF, y: 0xFFFF55, w: 0xFFFFFF };
  const EGA = (() => {
    const variants = {
      a: { label: 'Mavi masaüstü (DOS klasiği)', win: E.b, desk0: E.b, desk1: E.lg, ghost: E.lb },
      b: { label: 'Siyah masaüstü', win: E.k, desk0: E.k, desk1: E.dg, ghost: E.dg },
    };
    const roles = (V) => ({ BG: V.win, GHOST: V.ghost, FAINT: E.k, HALO: E.c, LIT: E.w, DIM: E.lg, HOT: E.y, HOTH: E.lr, KELP: E.g });
    const T = (fb, s, x, y, sc, c, o = {}) => fb.text(F.ega, s, x, y, { s: sc, c, ...o });
    const desk = (fb, V) => fb.dith(0, 0, W, H, V.desk0, V.desk1, 0.25);
    function win(fb, V, x, y, w, h, title) {
      fb.rect(x + 8, y + 6, w, h, E.k);
      fb.rect(x, y, w, h, V.win);
      fb.frame(x + 4, y + 6, w - 8, h - 12, E.lg, 2);
      fb.frame(x + 8, y + 10, w - 16, h - 20, E.lg, 2);
      const tw = fb.tw(title, 2), tx = x + Math.floor((w - tw) / 2);
      fb.rect(tx - 8, y + 2, tw + 16, 16, V.win);
      T(fb, title, tx, y + 2, 2, E.w);
      fb.rect(x + 14, y + 2, 48, 16, V.win);
      T(fb, '[', x + 16, y + 2, 2, E.lg); T(fb, '■', x + 30, y + 2, 2, E.lgn); T(fb, ']', x + 44, y + 2, 2, E.lg);
    }
    function status(fb, items) {
      fb.rect(0, 156, W, 16, E.lg);
      let x = 8;
      for (const [k, txt] of items) { x = T(fb, k, x, 157, 2, E.r); x = T(fb, txt, x, 157, 2, E.k); }
    }
    function bevel(fb, x, y, w, h, pressed) {
      fb.rect(x - 1, y - 1, w + 2, h + 2, E.k);
      fb.rect(x, y, w, h, pressed ? E.dg : E.lg);
      const a = pressed ? E.k : E.w, b = pressed ? E.lg : E.dg;
      fb.rect(x, y, w, 2, a); fb.rect(x, y, 2, h, a); fb.rect(x, y + h - 2, w, 2, b); fb.rect(x + w - 2, y, 2, h, b);
    }
    function well(fb, x, y, w, h, fill) {
      fb.rect(x - 2, y - 2, w + 4, h + 4, E.w); fb.rect(x - 2, y - 2, w + 2, 2, E.dg); fb.rect(x - 2, y - 2, 2, h + 2, E.dg);
      fb.rect(x, y, w, h, fill);
    }
    function knob(fb, cx, cy, r, ang) {
      fb.disc(cx + 2, cy + 3, r, E.dg);
      fb.disc(cx, cy, r, E.k);
      fb.disc(cx, cy, r - 2, E.lg);
      fb.fill(cx - r, cy - r, 2 * r, 2 * r, (x, y) => {
        const dx = x + .5 - cx, dy = y + .5 - cy, d = Math.hypot(dx, dy);
        if (d > r - 2) return null;
        const s = (dx + dy) / (r * 1.2);
        if (s < -0.35 && bayer(x, y) < -s - 0.2) return E.w;
        if (s > 0.25 && bayer(x, y) < s - 0.1) return E.dg;
        return null;
      });
      for (let k = 0; k < 24; k++) { const a = k * Math.PI / 12; fb.line(cx + Math.cos(a) * (r - 4), cy + Math.sin(a) * (r - 4), cx + Math.cos(a) * (r - 2), cy + Math.sin(a) * (r - 2), E.dg); }
      fb.disc(cx, cy, r * 0.45, E.lg); fb.ring(cx, cy, r * 0.45 - 1, r * 0.45, E.dg);
      fb.line(cx + Math.cos(ang) * r * 0.2, cy + Math.sin(ang) * r * 0.2, cx + Math.cos(ang) * (r - 6), cy + Math.sin(ang) * (r - 6), E.k, 2);
    }
    return {
      key: 'ega', name: 'EGA', font: 'ega', variants, roles,
      desc: '16 renk, DOS pencereleri: çift çizgi çerçeve, siyah gölge, alt durum çubuğu. İnce 8x8 font. Radyo: krom analog araba radyosu.',
      clock(fb, { t, P: V, S }) {
        desk(fb, V); win(fb, V, 2, 2, 620, 148, S.clock);
        const on = sec(t) % 2 === 0, cl = bigTime(F.ega, 28, 30, 10, 14);
        for (const c of cl) if (!c.colon || on) fb.rect(c[0] + 3, c[1] + 3, 10, 10, E.br);
        for (const c of cl) if (!c.colon || on) fb.rect(c[0], c[1], 10, 10, E.y);
        fb.stip(28, 116, 300, 10, E.lg, 0.25);
        fb.rect(28, 116, Math.round(300 * sec(t) / 60), 10, E.c);
        T(fb, S.wdLong, 374, 28, 2, E.w);
        T(fb, S.dateLong.replace('November', 'Nov').replace('Kasım', 'Kas') + ' ' + S.year, 374, 50, 2, E.lg);
        fb.rect(374, 73, 226, 2, E.lg);
        T(fb, S.alarm, 374, 86, 2, E.lg); T(fb, DEMO.alarm, 470, 86, 2, E.y);
        T(fb, S.battery, 374, 108, 2, E.lg);
        for (let k = 0; k < 6; k++) fb.rect(502 + k * 16, 108, 13, 14, k < Math.round(DEMO.batt / 100 * 6) ? E.lgn : E.dg);
        status(fb, S.status);
      },
      radio(fb, { t, P: V, S }) {
        fb.rect(0, 0, W, H, E.lg);
        for (let y = 2; y < H; y += 3) fb.stip(0, y, W, 1, E.w, 0.25);
        fb.rect(0, 0, W, 2, E.w); fb.rect(0, 0, 2, H, E.w); fb.rect(0, H - 2, W, 2, E.dg); fb.rect(W - 2, 0, 2, H, E.dg);
        knob(fb, 42, 54, 30, Math.PI * 0.85 + 0.7 * Math.PI * 1.4);
        T(fb, 'VOL', 30, 92, 1, E.dg, { s: 1 });
        well(fb, 84, 10, 254, 94, E.k);
        T(fb, '♪', 92, 16, 2, E.y); T(fb, DEMO.station, 112, 16, 2, E.w);
        fb.disc(318, 20, 3, E.lr); T(fb, 'ST', 300, 28, 1, E.lr, { s: 1 });
        const fx = (f) => 96 + Math.round((f - 88) / 20 * 228);
        for (let f = 88; f <= 108; f += 1) { const major = f % 4 === 0; fb.rect(fx(f), major ? 46 : 54, major ? 2 : 1, major ? 14 : 6, major ? E.y : E.br); }
        fb.rect(fx(88), 60, 230, 1, E.br);
        for (const f of [88, 92, 96, 100, 104, 108]) T(fb, String(f), fx(f) - Math.floor(fb.tw(String(f), 1) / 2), 66, 1, E.y, { s: 1 });
        T(fb, 'FM', 92, 36, 1, E.lr, { s: 1 }); T(fb, 'MHz', 306, 76, 1, E.br, { s: 1 });
        const nx = fx(88 + 20 * DEMO.stationIndex / (DEMO.stationCount - 1));
        fb.rect(nx - 1, 34, 3, 46, E.lr); fb.rect(nx, 34, 1, 46, E.w);
        T(fb, DEMO.title, 92, 84, 2, E.lg);
        well(fb, 348, 10, 70, 94, E.k);
        const g = couple(Math.floor(t / 480) % 2);
        fb.stip(352, 88, 62, 4, E.br, 0.5);
        fb.grid(g, 357, 24, 2, E.y);
        well(fb, 426, 10, 160, 94, E.k);
        levels(t).forEach((lv, i) => {
          const x = 430 + i * 11, l = Math.round(lv * 0.9), pk = Math.min(9, peak(l, i));
          for (let r = 0; r < 9; r++) {
            const y = 94 - r * 10;
            if (r < l) fb.rect(x, y, 9, 8, r < 5 ? E.lgn : r < 7 ? E.y : E.lr);
            else if (r === pk - 1) fb.rect(x, y + 6, 9, 2, E.lr);
            else fb.stip(x, y, 9, 8, E.g, 0.25);
          }
        });
        knob(fb, 614, 52, 22, -Math.PI * 0.3);
        T(fb, 'TUNE', 598, 82, 1, E.dg, { s: 1 });
        GENRES.forEach((k, i) => {
          const x = 9 + i * 89, cur = i === DEMO.genre;
          bevel(fb, x, 114, 85, 50, cur);
          if (cur) fb.rect(x + 36, 118, 14, 3, E.lr);
          T(fb, String(i + 1), x + 6, 120, 1, cur ? E.lg : E.dg, { s: 1 });
          T(fb, k, x + 43 - Math.floor(fb.tw(k, 2) / 2), 134, 2, cur ? E.w : E.k);
        });
      },
      scene(fb, { t, P: V }) {
        renderScene(fb, {
          sky: [E.k, E.k, E.b, E.b, E.m], star: [E.lg, E.w], moon: [E.y, E.w], land: [E.k], sea: [E.b, E.b, E.k], wave: [E.c, E.lc],
          glint: [E.y, E.w], rock: [E.k, E.dg], tw: [E.w], tr: [E.r], iron: [E.dg], lamp: [E.y], sand: [E.br, E.k], foam: [E.w],
          palm: [E.k], sail: [E.lg, E.w], hull: [E.r], beam: [E.lg, E.y, E.w],
        }, t);
        fb.rect(548, 142, 96, 30, E.k); fb.rect(540, 136, 96, 30, V.win); fb.frame(543, 139, 90, 24, E.lg, 2);
        T(fb, hhmm(), 549, 144, 2, E.w);
      },
      settings(fb, { t, P: V, S, theme }) {
        desk(fb, V); win(fb, V, 2, 2, 620, 148, S.settings);
        fb.rect(16, 22, 238, 120, E.c);
        S.items.slice(0, 5).forEach((it, i) => {
          const y = 22 + i * 24, sel = i === 0;
          if (sel) fb.rect(16, y, 230, 24, E.g);
          T(fb, it, 24, y + 5, 2, sel ? E.w : E.k);
        });
        fb.rect(246, 22, 8, 120, E.b); fb.rect(246, 22, 8, 8, E.lc); fb.rect(246, 134, 8, 8, E.lc); fb.rect(246, 34, 8, 10, E.lc);
        T(fb, S.items[0], 272, 24, 2, E.w);
        T(fb, '1/5', 604 - fb.tw('1/5', 2), 24, 2, E.lg);
        const btn = (x, ch) => { fb.rect(x + 6, 54, 56, 52, E.k); fb.rect(x, 48, 56, 52, E.g); T(fb, ch, x + 18, 64, 3, E.k); };
        btn(272, '◀'); btn(548, '▶');
        fb.rect(340, 60, 198, 28, E.c);
        const name = theme, s = fit(name, 190, [3, 2]);
        T(fb, name, 439 - Math.floor(fb.tw(name, s) / 2), 74 - Math.floor(7 * s / 2), s, E.k);
        for (let k = 0; k < 5; k++) { const x = 400 + k * 16; fb.rect(x, 100, 10, 10, k === 2 ? E.y : V.ghost); }
        T(fb, S.hint, 340, 120, 2, E.lg);
        status(fb, S.statusSet);
      },
    };
  })();

  // ================================================================ C64-style
  const C = { k: 0x000000, w: 0xFFFFFF, r: 0x68372B, c: 0x70A4B2, p: 0x6F3D86, g: 0x588D43, b: 0x352879, y: 0xB8C76F, o: 0x6F4F25,
    n: 0x433900, lr: 0x9A6759, dg: 0x444444, gr: 0x6C6C6C, lg: 0x9AD284, lb: 0x6C5EB5, lgr: 0x959595 };
  const C64 = (() => {
    const variants = {
      a: { label: 'Açılış mavisi', border: C.lb, bg: C.b, text: C.lb, hi: C.w, acc: C.y, dim: C.lgr, ghost: C.dg },
      b: { label: 'Gece siyahı', border: C.dg, bg: C.k, text: C.lg, hi: C.w, acc: C.y, dim: C.g, ghost: C.dg },
    };
    const roles = (V) => ({ BG: V.bg, GHOST: V.ghost, FAINT: V.ghost, HALO: V.border, LIT: V.text, DIM: V.dim, HOT: V.hi, HOTH: V.acc, KELP: C.g });
    const BT = 8, BS = 16; // border: top/bottom, sides
    const T = (fb, s, x, y, sc, c, lang) => fb.text(F.c64, up(s, lang), x, y, { s: sc, c });
    const row = (r) => 10 + r * 16;
    const screen = (fb, V) => { fb.clear(V.border); fb.rect(BS, BT, W - 2 * BS, H - 2 * BT, V.bg); };
    return {
      key: 'c64', name: 'C64-style', font: 'c64', variants, roles, border: { top: BT, side: BS },
      desc: 'Ekranın çevresinde çerçeve rengi, açılış satırı, READY. ve yanıp sönen imleç. Yuvarlak kalın 8x8 font. Sahnelerde 2x geniş piksel. Radyo çalarken çerçevede yükleme şeritleri müziğe göre atar.',
      clock(fb, { t, P: V, S, lang }) {
        screen(fb, V);
        const ban = '**** PHOSPHOR CLOCK V1.1 ****';
        T(fb, ban, BS + Math.floor((W - 2 * BS - fb.tw(ban, 2)) / 2), row(0), 2, V.text, lang);
        const on = sec(t) % 2 === 0;
        for (const c of bigTime(F.c64, 32, 42, 10, 10)) if (!c.colon || on) fb.rect(c[0], c[1], 10, 10, V.text);
        T(fb, S.wd, 404, 42, 3, V.hi, lang);
        T(fb, S.date, 404, row(4), 2, V.text, lang);
        T(fb, 'AL ' + DEMO.alarm, 404, row(5), 2, V.acc, lang);
        T(fb, 'BAT ' + DEMO.batt + '%', 404, row(6), 2, V.dim, lang);
        T(fb, 'READY.', 24, row(7), 2, V.text, lang);
        if (blink(t)) fb.rect(24, row(8), 14, 14, V.text);
      },
      radio(fb, { t, P: V, S, lang }) {
        const L = levels(t), avg = L.reduce((a, b) => a + b, 0) / L.length / 10;
        const STRIPES = [C.c, C.r, C.y, C.p, C.lb, C.lg, C.o, C.w, C.lr, C.g];
        const frame = Math.floor(t / 60);
        for (let y = 0, k = 0; y < H; k++) {
          const hb = 1 + (hash(k * 3 + frame * 101) % Math.max(2, Math.round(7 - avg * 5)));
          const loud = (hash(k * 7 + frame * 13) % 100) / 100 < 0.35 + avg * 0.6;
          fb.rect(0, y, W, hb, loud ? STRIPES[hash(k + frame * 31) % STRIPES.length] : V.border);
          y += hb;
        }
        fb.rect(BS, BT, W - 2 * BS, H - 2 * BT, V.bg);
        T(fb, 'LOAD"' + DEMO.station + '"', 24, row(0), 2, V.text, lang);
        T(fb, S.found + ' ' + DEMO.station, 24, row(1), 2, V.text, lang);
        T(fb, S.loading + ' ' + DEMO.title, 24, row(2), 2, V.hi, lang);
        // tape deck
        fb.rect(24, 64, 176, 62, C.gr); fb.rect(24, 64, 176, 4, C.lgr); fb.rect(24, 122, 176, 4, C.dg);
        fb.rect(34, 72, 112, 34, C.k); fb.frame(34, 72, 112, 34, C.lgr);
        fb.clip(35, 73, 145, 105);
        const rot = t / 260;
        for (const [cx, r] of [[62, 15], [118, 11]]) {
          fb.disc(cx, 89, r, C.o); fb.disc(cx, 89, 6, C.w); fb.disc(cx, 89, 3, C.k);
          for (let k = 0; k < 3; k++) { const a = rot + k * Math.PI * 2 / 3; fb.rect(Math.round(cx + Math.cos(a) * 4.5) - 1, Math.round(89 + Math.sin(a) * 4.5) - 1, 2, 2, C.w); }
        }
        fb.noClip();
        fb.rect(154, 74, 38, 14, C.k); fb.text(F.c64, '042', 157, 77, { s: 1, c: C.w });
        for (let k = 0; k < 5; k++) { const pressed = k === 1; fb.rect(34 + k * 32, pressed ? 112 : 110, 28, pressed ? 10 : 12, pressed ? C.dg : C.lgr); }
        fb.text(F.c64, '▶', 44, 113, { s: 1, c: C.k });
        T(fb, GENRES[DEMO.genre], 214, 66, 3, V.hi, lang);
        T(fb, 'CH 0' + DEMO.ch, 214, 98, 2, V.text, lang);
        const g = couple(Math.floor(t / 480) % 2);
        fb.grid(g, 358, 62, 2, V.hi);
        L.forEach((lv, i) => {
          const x = 426 + i * 14, l = Math.round(lv * 0.8), pk = Math.min(8, peak(l, i));
          for (let r = 0; r < 8; r++) {
            const y = 120 - r * 7;
            if (r < l) fb.rect(x, y, 12, 6, r < 4 ? C.lg : r < 6 ? C.y : C.lr);
            else if (r === pk - 1) fb.rect(x, y, 12, 6, C.w);
            else fb.rect(x, y, 12, 6, V.ghost);
          }
        });
        GENRES.forEach((k, i) => {
          const x = 24 + i * 86, cur = i === DEMO.genre;
          if (cur) fb.rect(x, 134, 80, 24, V.text); else fb.frame(x, 134, 80, 24, V.text, 2);
          T(fb, k, x + 40 - Math.floor(fb.tw(k, 2) / 2), 139, 2, cur ? V.bg : V.text, lang);
        });
      },
      scene(fb, { t, P: V, lang }) {
        renderScene(fb, {
          sky: [C.k, C.k, C.b, C.p], star: [C.lgr, C.w], moon: [C.lgr, C.w], land: [C.k], sea: [C.b, C.b, C.k], wave: [C.lb, C.c],
          glint: [C.y, C.w], rock: [C.k, C.dg], tw: [C.w], tr: [C.r], iron: [C.dg], lamp: [C.y], sand: [C.o, C.n], foam: [C.w],
          palm: [C.k], sail: [C.lgr, C.w], hull: [C.r], beam: [C.gr, C.y, C.w],
        }, t, { wide: true });
        fb.rect(0, 0, W, BT, V.border); fb.rect(0, H - BT, W, BT, V.border); fb.rect(0, 0, BS, H, V.border); fb.rect(W - BS, 0, BS, H, V.border);
        T(fb, hhmm(), W - BS - 8 - fb.tw(hhmm(), 2), H - BT - 20, 2, V.hi, lang);
      },
      settings(fb, { t, P: V, S, lang, theme }) {
        screen(fb, V);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 12 + i * 24, sel = i === 0;
          if (sel) fb.rect(20, y - 4, 276, 22, V.text);
          T(fb, (i + 1) * 10 + ' ' + it, 24, y, 2, sel ? V.bg : V.text, lang);
        });
        T(fb, S.items[0], 312, 12, 2, V.dim, lang);
        T(fb, '1/5', 616 - fb.tw('1/5', 2), 12, 2, V.dim, lang);
        fb.rect(312, 38, 44, 64, V.text); T(fb, '◀', 322, 56, 4, V.bg, lang);
        fb.rect(572, 38, 44, 64, V.text); T(fb, '▶', 584, 56, 4, V.bg, lang);
        const name = up(theme, lang), s = fit(name, 208, [3, 2]);
        T(fb, name, 464 - Math.floor(fb.tw(name, s) / 2), 70 - Math.floor(7 * s / 2), s, V.hi, lang);
        for (let k = 0; k < 5; k++) fb.rect(424 + k * 18, 110, 10, 10, k === 3 ? V.hi : V.ghost);
        T(fb, 'READY.', 312, row(7), 2, V.text, lang);
        if (blink(t)) fb.rect(312, row(8), 14, 14, V.text);
      },
    };
  })();

  // ================================================================ Amiga-style
  const AMIGA = (() => {
    const variants = {
      a: { label: '1.x: mavi, beyaz, siyah, turuncu', c0: 0x0055AA, c1: 0xFFFFFF, c2: 0x000022, c3: 0xFF8800, wb: 1 },
      b: { label: '2.x: gri, siyah, beyaz, mavi', c0: 0xAAAAAA, c1: 0x000000, c2: 0xFFFFFF, c3: 0x6688BB, wb: 2 },
    };
    for (const V of Object.values(variants)) {
      V.paper = V.wb === 1 ? V.c1 : V.c2; V.ink = V.wb === 1 ? V.c2 : V.c1;
      V.shine = V.paper; V.shadow = V.ink;
    }
    const roles = (V) => ({ BG: V.c0, GHOST: { flat: mix(V.c0, V.ink, 0.5), dither: ['BG', 'DIM', 0.5] }, FAINT: { flat: mix(V.c0, V.ink, 0.25), dither: ['BG', 'DIM', 0.25] },
      HALO: V.ink, LIT: V.c1, DIM: V.c2, HOT: V.c3, HOTH: V.c3, KELP: V.c3 });
    const T = (fb, s, x, y, sc, c, sh) => fb.text(F.amiga, s, x, y, { s: sc, c, sh: sh != null ? { dx: sc, dy: sc, c: sh } : undefined });
    const COPPER = ['#F60', '#F70', '#F80', '#F90', '#FA0', '#FB2', '#FC4', '#FD6', '#FE8', '#FFA', '#FFC', '#FFE', '#FFF', '#EFF', '#DEF', '#BDF', '#ACF', '#9BF', '#8AF', '#79E', '#68D'].map(x12);
    const VU = ['#05A', '#06B', '#08C', '#0AD', '#2CE', '#6DE', '#AEE', '#FFF', '#FD8', '#F80'].map(x12);
    function bar(fb, V, title) {
      fb.rect(0, 0, W, 12, V.paper);
      T(fb, title, 6, 2, 1, V.wb === 1 ? V.c0 : V.c1);
      for (const [x, fill] of [[606, false], [622, true]]) { fb.rect(x, 1, 14, 10, V.paper); fb.frame(x + 2, 2, 8, 6, V.ink); if (fill) fb.rect(x + 5, 4, 8, 6, V.ink); else fb.frame(x + 5, 4, 8, 6, V.ink); }
      fb.rect(0, 12, W, 1, V.ink);
    }
    function bevel(fb, V, x, y, w, h, raised = true) {
      const a = raised ? V.shine : V.shadow, b = raised ? V.shadow : V.shine;
      fb.rect(x, y, w, 2, a); fb.rect(x, y, 2, h, a); fb.rect(x, y + h - 2, w, 2, b); fb.rect(x + w - 2, y, 2, h, b);
    }
    function win(fb, V, x, y, w, h, title) {
      fb.rect(x, y, w, h, V.c0);
      if (V.wb === 1) { fb.frame(x, y, w, h, V.c1, 2); }
      else { bevel(fb, V, x, y, w, h, true); }
      const tb = 18;
      if (V.wb === 1) { for (let r = 2; r < tb; r++) fb.rect(x + 24, y + r, w - 54, 1, r % 2 ? V.c1 : V.c0); }
      else { fb.rect(x + 24, y + 2, w - 54, tb - 2, V.c3); }
      const tw = fb.tw(title, 2);
      fb.rect(x + 30, y + 2, tw + 12, tb - 2, V.wb === 1 ? V.c0 : V.c3);
      T(fb, title, x + 36, y + 3, 2, V.wb === 1 ? V.c1 : V.c1);
      // close gadget
      fb.rect(x + 2, y + 2, 22, tb - 2, V.c0); fb.frame(x + 6, y + 4, 14, 12, V.paper, 2); fb.rect(x + 11, y + 8, 4, 4, V.paper);
      if (V.wb === 2) bevel(fb, V, x + 2, y + 2, 22, tb - 2, true);
      // depth gadget
      const dx = x + w - 30;
      fb.rect(dx, y + 2, 28, tb - 2, V.c0);
      fb.frame(dx + 4, y + 4, 12, 8, V.paper, 2); fb.rect(dx + 10, y + 8, 14, 8, V.paper);
      if (V.wb === 2) bevel(fb, V, dx, y + 2, 28, tb - 2, true);
      fb.rect(x, y + tb, w, 2, V.wb === 1 ? V.c1 : V.shadow);
      return { x: x + 2, y: y + tb + 2, w: w - 4, h: h - tb - 4 };
    }
    function clockBody(fb, V, S, t) {
      fb.clear(V.c0); bar(fb, V, 'Phosphor Screen');
      win(fb, V, 0, 13, W, 159, S.clock);
      const cl = bigTime(F.amiga, 16, 42, 11, 11);
      for (const c of cl) fb.rect(c[0] + 3, c[1] + 3, 11, 11, V.ink);
      for (const c of cl) for (let j = 0; j < 11; j++) fb.rect(c[0], c[1] + j, 11, 1, COPPER[Math.min(COPPER.length - 1, Math.floor((c[1] + j - 42) / 77 * COPPER.length))]);
      T(fb, S.wdLong + ' ' + S.dateLong, 16, 130, 2, V.c1);
      const al = S.alarm + ' ' + DEMO.alarm;
      if (V.wb === 1) T(fb, al, 16, 152, 2, V.c3);
      else { fb.rect(10, 149, fb.tw(al, 2) + 12, 21, V.c3); T(fb, al, 16, 152, 2, V.c1); } // blue on grey is too faint: 2.x marks it like an active title bar
      T(fb, S.battery + ' ' + DEMO.batt + '%', 224, 152, 2, V.wb === 1 ? V.c2 : V.c1);
      // analog clock, like the old desk clock tool
      const cx = 528, cy = 98, r = 60;
      fb.ring(cx, cy, r - 2, r, V.paper);
      for (let k = 0; k < 12; k++) {
        const a = k * Math.PI / 6, big = k % 3 === 0;
        const r0 = big ? r - 12 : r - 8;
        fb.line(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0, cx + Math.cos(a) * (r - 4), cy + Math.sin(a) * (r - 4), big ? V.c3 : V.paper, big ? 3 : 2);
      }
      const hA = ((+DEMO.hh % 12) + +DEMO.mm / 60) / 12 * Math.PI * 2 - Math.PI / 2, mA = +DEMO.mm / 60 * Math.PI * 2 - Math.PI / 2, sA = sec(t) / 60 * Math.PI * 2 - Math.PI / 2;
      fb.line(cx, cy, cx + Math.cos(hA) * r * 0.5, cy + Math.sin(hA) * r * 0.5, V.ink, 4);
      fb.line(cx, cy, cx + Math.cos(mA) * r * 0.78, cy + Math.sin(mA) * r * 0.78, V.ink, 3);
      fb.line(cx, cy, cx + Math.cos(sA) * r * 0.85, cy + Math.sin(sA) * r * 0.85, V.c3, 1);
      fb.disc(cx, cy, 3.5, V.c3);
    }
    return {
      key: 'amiga', name: 'Amiga-style', font: 'amiga', variants, roles,
      desc: '4 renkli masaüstü, çizgili başlık çubukları, kapat/derinlik gadget\'ları, copper renk bantları (her satır kendi 12-bit rengi). Kare kalın 8x8 font, küçük harfli. Alarm: kırmızı/siyah yanıp sönen uyarı kutusu.',
      clock(fb, { t, P: V, S }) { clockBody(fb, V, S, t); },
      alarm(fb, { t, P: V, S }) {
        const tmp = new Fb();
        clockBody(tmp, V, S, t);
        fb.d.set(tmp.d.subarray(0, (H - 74) * W), 74 * W);
        fb.rect(0, 0, W, 74, 0x000000);
        const red = x12('#F00');
        if (blink(t, 700)) fb.frame(6, 6, W - 12, 62, red, 6);
        const l1 = S.alarm + ' ' + DEMO.alarm + '.   ' + S.tapStop, l2 = S.meditation + ' #00000730.00000020';
        T(fb, l1, Math.floor((W - fb.tw(l1, 2)) / 2), 22, 2, red);
        T(fb, l2, Math.floor((W - fb.tw(l2, 2)) / 2), 42, 2, red);
      },
      radio(fb, { t, P: V, S }) {
        fb.clear(V.c0); bar(fb, V, 'Phosphor Tracker');
        win(fb, V, 0, 13, W, 159, S.radio);
        T(fb, DEMO.station, 10, 38, 2, V.c1);
        T(fb, 'CH0' + DEMO.ch, 222, 38, 2, V.c2);
        // pattern
        fb.rect(6, 56, 338, 74, 0x000000); bevel(fb, V, 4, 54, 342, 78, false);
        const row = Math.floor(t / 150), NOTES = ['C-3', 'D#3', 'F-3', 'G-3', 'A#3', 'C-4', 'D#4', 'G-2', 'F-2', 'A#2'];
        const BAR = ['#420', '#730', '#A50', '#D70', '#F90', '#D70', '#A50', '#730', '#420'].map(x12);
        fb.clip(6, 56, 344, 130);
        for (let k = 0; k < 8; k++) {
          const r = row - 3 + k, y = 57 + k * 9, cur = k === 3;
          if (cur) BAR.forEach((c, j) => fb.rect(6, y - 1 + j, 338, 1, c));
          const rn = ((r % 64) + 64) % 64;
          fb.text(F.amiga, rn.toString(16).toUpperCase().padStart(2, '0'), 10, y, { s: 1, c: cur ? 0x000000 : x12('#FA0') });
          for (let ch = 0; ch < 4; ch++) {
            const h = hash(r * 4 + ch + 1000), x = 36 + ch * 76;
            fb.rect(x - 6, 56, 1, 74, x12('#334'));
            if ((h + ch) % 3 === 0 || (ch === 3 && h % 2)) { fb.text(F.amiga, '--- 00', x, y, { s: 1, c: cur ? 0x221100 : x12('#346') }); continue; }
            fb.text(F.amiga, NOTES[h % NOTES.length], x, y, { s: 1, c: cur ? 0x000000 : 0xFFFFFF });
            fb.text(F.amiga, '0' + (ch + 1), x + 32, y, { s: 1, c: cur ? 0x000000 : x12('#8AF') });
          }
        }
        fb.noClip();
        const g = couple(Math.floor(t / 480) % 2);
        fb.grid(g, 360, 50, 2, V.ink);
        fb.grid(g, 358, 48, 2, V.paper);
        fb.rect(352, 116, 62, 2, V.c3);
        levels(t).forEach((lv, i) => {
          const x = 428 + i * 14;
          for (let r = 0; r < 10; r++) {
            const y = 120 - r * 9;
            if (r < lv) fb.rect(x, y, 12, 7, VU[r]);
            else fb.stip(x, y, 12, 7, V.ink, 0.125);
          }
        });
        GENRES_MIX.forEach((k, i) => {
          const x = 6 + i * 90, cur = i === DEMO.genre;
          fb.rect(x, 136, 86, 32, cur ? V.c3 : V.c0);
          bevel(fb, V, x, 136, 86, 32, !cur);
          T(fb, k, x + 43 - Math.floor(fb.tw(k, 2) / 2), 145, 2, cur ? V.c2 : V.c1);
        });
      },
      scene(fb, { t, P: V }) {
        const r = (a) => a.map(x12);
        renderScene(fb, {
          sky: r(['#001', '#002', '#012', '#013', '#023', '#024', '#134', '#135', '#245', '#356', '#467', '#578', '#879', '#A89']),
          star: r(['#89B', '#FFF']), moon: r(['#DDC', '#FFE']), land: r(['#012']),
          sea: r(['#247', '#136', '#125', '#024', '#013', '#012', '#002']), wave: r(['#48B', '#6AD']), glint: r(['#FFD', '#FFF']),
          rock: r(['#222', '#334']), tw: r(['#EEE']), tr: r(['#C22']), iron: r(['#333']), lamp: r(['#FF8']),
          sand: r(['#753', '#642', '#531', '#420']), foam: r(['#DEF']), palm: r(['#011']), sail: r(['#AAB', '#DDC']), hull: r(['#822']),
          beam: r(['#456', '#789', '#BBA', '#FFD']),
        }, t, { band: true });
        T(fb, hhmm(), 552, 148, 2, 0xFFFFFF, 0x000000);
      },
      settings(fb, { t, P: V, S, theme }) {
        fb.clear(V.c0); bar(fb, V, 'Phosphor Screen');
        win(fb, V, 0, 13, W, 159, S.prefs);
        fb.rect(8, 38, 254, 126, V.c0); bevel(fb, V, 6, 36, 258, 130, false);
        S.items.slice(0, 5).forEach((it, i) => {
          const y = 40 + i * 25, sel = i === 0;
          if (sel) fb.rect(8, y, 234, 24, V.c3);
          T(fb, it, 16, y + 5, 2, sel ? V.c2 : V.c1);
        });
        bevel(fb, V, 244, 38, 18, 126, false); fb.rect(246, 40, 14, 66, V.c0); bevel(fb, V, 246, 40, 14, 66, true);
        T(fb, S.items[0], 284, 40, 2, V.c1);
        T(fb, '1/5', 632 - fb.tw('1/5', 2), 40, 2, V.c2);
        const btn = (x, ch) => { fb.rect(x, 64, 56, 56, V.c0); bevel(fb, V, x, 64, 56, 56, true); T(fb, ch, x + 18, 82, 3, V.c1); };
        btn(280, '◀'); btn(576, '▶');
        fb.rect(344, 70, 224, 44, V.c0); bevel(fb, V, 344, 70, 224, 44, false); fb.frame(348, 74, 216, 36, V.ink, 1);
        const s = fit(theme, 208, [3, 2]);
        T(fb, theme, 456 - Math.floor(fb.tw(theme, s) / 2), 92 - Math.floor(7 * s / 2), s, V.c1);
        for (let k = 0; k < 5; k++) { const x = 412 + k * 18; fb.rect(x, 128, 12, 12, k === 4 ? V.c3 : V.c0); bevel(fb, V, x, 128, 12, 12, k !== 4); }
        T(fb, S.hint, 284, 150, 2, V.c2);
      },
    };
  })();

  const THEMES = [VFD, CGA, EGA, C64, AMIGA];
  const SCREENS = ['clock', 'alarm', 'radio', 'scene', 'settings'];
  // Draw one screen of one theme variant. lang: 'en' | 'tr'.
  function render(fb, themeKey, variant, screen, t = 0, lang = 'en') {
    const th = THEMES.find((x) => x.key === themeKey);
    const P = th.variants[variant] || th.variants.a;
    const S = STR[lang];
    const theme = THEME_NAMES[THEMES.indexOf(th)];
    LANG = lang;
    th[screen](fb, { t, P, S, lang, theme });
    return fb;
  }
  const flat = (r) => (typeof r === 'object' ? r.flat : r);
  // A font sheet in the theme's colors: every glyph, then a Turkish pangram.
  function specimen(fb, themeKey, variant = 'a') {
    const th = THEMES.find((x) => x.key === themeKey), P = th.variants[variant] || th.variants.a, R = th.roles(P);
    const bg = flat(R.BG), fg = flat(R.LIT), hot = flat(R.HOT), dim = flat(R.DIM);
    fb.clear(bg);
    const pan1 = 'Pijamalı hasta yağız şoföre', pan2 = 'çabucak güvendi.';
    if (!th.font) {
      fb.vfdText('ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789', 12, 12, 2, 2, { c: fg, h: flat(R.HALO) });
      fb.vfdText('%:/-+.,!?()<>=#*\'"', 12, 34, 2, 2, { c: dim });
      fb.vfdText(up(pan1, 'tr'), 12, 66, 3, 2, { c: fg, h: flat(R.HALO) });
      fb.vfdText(up(pan2, 'tr') + ' ÇĞİÖŞÜ', 12, 96, 3, 2, { c: fg, h: flat(R.HALO) });
      fb.vfdText('21:47', 12, 132, 4, 3, { c: hot, h: flat(R.HOTH) });
      return fb;
    }
    const font = G.FONTS[th.font], keys = Object.keys(G.FONT_BASE);
    keys.forEach((k, i) => fb.text(font, k, 8 + (i % 39) * 16, 6 + Math.floor(i / 39) * 18, { s: 2, c: i % 39 === 0 ? fg : fg }));
    const p1 = font.upper ? up(pan1, 'tr') : pan1, p2 = font.upper ? up(pan2, 'tr') : pan2;
    fb.text(font, p1, 8, 70, { s: 2, c: dim });
    fb.text(font, p2 + (font.upper ? ' ÇĞİÖŞÜ' : ' ÇĞİÖŞÜ çğıöşü'), 8, 90, { s: 2, c: dim });
    fb.text(font, font.upper ? 'ĞİŞ 21:47' : 'Ğğ İı Şş 21:47', 8, 118, { s: 5, c: hot });
    return fb;
  }
  G.PHT = { THEMES, SCREENS, THEME_NAMES, STR, render, specimen, flat, x12 };
})(typeof window !== 'undefined' ? window : globalThis);
