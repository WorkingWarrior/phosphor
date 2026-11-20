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
  // weather demo (firmware Weather.h: temp, humidity, wind, sky, three next days)
  const WX = { temp: 14, hum: 72, wind: 23, sky: 'partcloud', next: [[0, 16, 'rain'], [1, 13, 'cloud'], [2, 17, 'clear']] };
  Object.assign(STR.en, { weather: 'Weather', hum: 'Humidity', windL: 'Wind', days: ['Sat', 'Sun', 'Mon'], kmh: 'km/h',
    sky: { clear: 'Clear', partcloud: 'Partly cloudy', cloud: 'Cloudy', rain: 'Rain' },
    narr: ['It is Friday, 20 November.', 'The alarm is set for 07:30.'], wxNarr: ['Partly cloudy and 14°.', 'Rain is coming on Saturday.'],
    options: 'Options' });
  Object.assign(STR.tr, { weather: 'Hava durumu', hum: 'Nem', windL: 'Rüzgar', days: ['Cmt', 'Paz', 'Pzt'], kmh: 'km/s',
    sky: { clear: 'Açık', partcloud: 'Parçalı bulutlu', cloud: 'Bulutlu', rain: 'Yağmurlu' },
    narr: ['Bugün 20 Kasım, Cuma.', 'Alarm 07:30\'a kurulu.'], wxNarr: ['Parçalı bulutlu, 14°.', 'Cumartesi yağmur geliyor.'],
    options: 'Seçenekler' });
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
      weather(fb, { t, P, S }) {
        fb.clear(P.BG);
        fb.seg(String(WX.temp), 20, 18, 60, 96, 10, 10, { c: P.LIT, h: P.HALO, g: P.GHOST });
        fb.frame(158, 18, 12, 12, P.LIT); fb.frame(159, 19, 10, 10, P.LIT);
        vt(fb, S.sky[WX.sky], 20, 126, 2, 2, LIT(P));
        vt(fb, WX.hum + '%  ' + WX.wind + ' ' + S.kmh, 20, 152, 2, 2, { c: P.DIM });
        // dot-matrix sky: faint grid, a cloud drawn as lit outline + dim fill, the moon, stars
        const X0 = 232, Y0 = 2, PIT = 4, NC = 100, NR = 33;
        const drift = (t / 260) % 140;
        const inCloud = (i, j, cx, cy, k) => [[0, 0, 7], [-9, 3, 5], [9, 3, 6], [-15, 6, 3.5], [15, 6, 3.5]].some(([dx, dy, r]) => Math.hypot(i - (cx + dx * k), j - (cy + dy * k)) < r * k) && j < cy + 9 * k;
        const cloud = (i, j) => inCloud(i, j, 30, 13, 1) || inCloud(i, j, ((92 + drift) % 140) - 20, 17, 0.7);
        for (let j = 0; j < NR; j++) for (let i = 0; i < NC; i++) {
          const x = X0 + i * PIT, y = Y0 + j * PIT;
          const c = cloud(i, j), edge = c && !(cloud(i - 1, j) && cloud(i + 1, j) && cloud(i, j - 1) && cloud(i, j + 1));
          const md = Math.hypot(i - 46, j - 8), moon = md < 6.5 && Math.hypot(i - 49, j - 6) > 5.2;
          const star = !c && hash(i * 131 + j * 7) % 53 === 0 && Math.sin(t / 700 + i) > -0.3;
          if (edge || moon || star) { fb.rect(x - 1, y - 1, 5, 5, P.HALO); fb.rect(x, y, 3, 3, P.LIT); }
          else if (c) fb.rect(x, y, 3, 3, P.DIM);
          else fb.rect(x + 1, y + 1, 1, 1, P.FAINT);
        }
        const ICON = { rain: ['.###.', '#####', '.#.#.', '#.#..'], cloud: ['.##..', '#####', '#####', '.....'], clear: ['#.#.#', '.###.', '#####', '.###.'] };
        WX.next.forEach(([d, tmax, sky], k) => {
          const x = 268 + k * 124;
          ICON[sky].forEach((row, j) => [...row].forEach((ch, i) => { if (ch === '#') fb.rect(x + i * 4, 148 + j * 4, 3, 3, P.DIM); }));
          vt(fb, S.days[d], x + 26, 150, 2, 2, { c: P.DIM });
          vt(fb, tmax + '°', x + 70, 150, 2, 2, LIT(P));
        });
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


  // retro.js adds CGA, EGA, C64-style and Amiga-style after this file runs
  const THEMES = [VFD];
  const SCREENS = ['world', 'clock', 'alarm', 'radio', 'weather', 'scene', 'settings'];
  // Draw one screen of one theme variant. lang: 'en' | 'tr'. A theme may show its own demo time (th.time 'hh:mm').
  function render(fb, themeKey, variant, screen, t = 0, lang = 'en') {
    const th = THEMES.find((x) => x.key === themeKey);
    const P = th.variants[variant] || th.variants.a;
    const S = STR[lang];
    const theme = THEME_NAMES[THEMES.indexOf(th)];
    LANG = lang;
    const keep = [DEMO.hh, DEMO.mm];
    if (th.time) [DEMO.hh, DEMO.mm] = th.time.split(':');
    try { th[screen](fb, { t, P, S, lang, theme }); } finally { [DEMO.hh, DEMO.mm] = keep; }
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
  // shared with retro.js
  G.PHC = { DEMO, WX, STR, GENRES, GENRES_MIX, levels, peak, sec, blink, hhmm, cells, sceneModel, M, MN, hash, x12, lang: () => LANG };
})(typeof window !== 'undefined' ? window : globalThis);
