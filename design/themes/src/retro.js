// CGA, EGA, C64-style and Amiga-style: each theme is a world painted in that machine's game-art idiom on the 320x86
// pixel layer (firmware px::, shown 2x), with the clock / radio / weather / settings HUD sitting on top of it.
// All art is drawn here from shapes and noise; no game assets, logos or ROM fonts.
(function (G) {
  const { up, mix, couple } = G.PH;
  const { Px, rnd, noise, fbm, clamp, half, x12 } = G.ART;
  const { DEMO, WX, GENRES, GENRES_MIX, levels, peak, blink, hhmm, sceneModel, MN, hash } = G.PHC;
  const lang = () => G.PHC.lang();
  const F = G.FONTS;
  const PW = 320, PH = 86;
  const q12 = (c) => (Math.round(((c >> 16) & 255) / 17) * 17 << 16) | (Math.round(((c >> 8) & 255) / 17) * 17 << 8) | Math.round((c & 255) / 17) * 17;
  const ridged = (v, seed) => 1 - Math.abs(2 * fbm(v, seed, 3) - 1);

  // ---------------------------------------------------------------- shared bits
  const THIN = new Set([2, 3, 9, 10, 11, 12, 15, 16, 17]);   // star moon tower rail iron lamp palm sail hull
  const SPRAY = new Set([6, 7, 14]);                            // wave glint foam: only where they fill the block
  function sceneLow(px, ramps, t, opt = {}) {
    // the lighthouse scene, sampled down to the pixel layer: of the four full-res pixels under one pixel, the most
    // foreground material wins (keeps 1 px stars, rigging and palm fronds)
    const S = sceneModel(t), FW = 640;
    for (let y = 0; y < PH; y++) for (let x = 0; x < PW; x += px.wide ? 2 : 1) {
      let best = -1, bi = 0, bw = -1;
      const w = {};
      for (const [dx, dy] of [[0, 0], [1, 0], [0, 1], [1, 1]]) { const i = (y * 2 + dy) * FW + x * 2 + dx, m = S.mat[i]; w[m] = (w[m] || 0) + (THIN.has(m) ? 3 : SPRAY.has(m) ? 0.45 : 1); if (w[m] > bw || (w[m] === bw && m > best)) { bw = w[m]; best = m; bi = i; } }
      const ramp = ramps[MN[best]] || ramps.sky;
      let c = opt.band ? ramp[Math.round(clamp(S.val[bi]) * (ramp.length - 1))] : px.pick(ramp, S.val[bi], x, y);
      const b = S.beam[bi];
      if (b > 0 && ramps.beam) { const br = ramps.beam, k = Math.floor(b * br.length + (opt.band ? 0.5 : px.th(x, y)) - 0.25); if (k > 0) c = br[Math.min(br.length, k) - 1]; }
      px.px(x, y, c);
    }
  }
  // dancing couple (26x32). light / dark: optional edge colors on the left / right side of the shape
  function dancers(px, x, y, t, body, light, dark) {
    const g = couple(Math.floor(t / 480) % 2), w = px.wide;
    px.wide = false;
    for (let j = 0; j < g.h; j++) for (let i = 0; i < g.w; i++) {
      if (!g.at(i, j)) continue;
      let c = body;
      if (light != null && !g.at(i - 1, j)) c = light;
      else if (dark != null && !g.at(i + 1, j)) c = dark;
      px.px(x + i, y + j, c);
    }
    px.wide = w;
  }
  const BELL = ['..#..', '.###.', '.###.', '#####', '..#..'];
  const icon = (px, rows, x, y, c, c2) => rows.forEach((r, j) => [...r].forEach((k, i) => { if (k === '#') px.px(x + i, y + j, c); else if (k === '*' && c2 != null) px.px(x + i, y + j, c2); }));
  function battery(px, x, y, c, fillC) {
    px.frame(x, y, 10, 5, c); px.rect(x + 10, y + 1, 1, 3, c);
    px.rect(x + 1, y + 1, Math.round(8 * DEMO.batt / 100), 3, fillC);
  }
  // weather icons, 13x9: w cloud, g cloud shade, y sun, b rain
  const WICON = {
    clear: ['......y......', '..y...y...y..', '....yyyyy....', '...yyyyyyy...', 'yy.yyyyyyy.yy', '...yyyyyyy...', '....yyyyy....', '..y...y...y..', '......y......'],
    cloud: ['.............', '.....wwww....', '...wwwwwwww..', '..wwwwwwwwww.', '.wwwwwwwwwwww', 'wwwwwwwwwwwww', 'ggggggggggggg', '.ggggggggggg.', '.............'],
    rain: ['.....wwww....', '...wwwwwwww..', '.wwwwwwwwwww.', 'wwwwwwwwwwwww', 'ggggggggggggg', '..b...b...b..', '.b...b...b...', 'b...b...b....', '.............'],
    partcloud: ['..y.y........', '...yyy.......', '.yyyyy.www...', '..yyy.wwwwww.', '.y.y.wwwwwwww', '....wwwwwwwww', '...gggggggggg', '....gggggggg.', '.............'],
  };
  // 9x7 versions for one text line
  const WMINI = {
    clear: ['..#.#.#..', '...###...', '.#######.', '..#####..', '.#######.', '...###...', '..#.#.#..'],
    cloud: ['.........', '...###...', '..#####..', '.#######.', '#########', '.*******.', '.........'],
    rain: ['...###...', '..#####..', '.#######.', '#########', '.........', '.*..*..*.', '*..*..*..'],
    partcloud: ['.#.#.....', '..##.##..', '.##.####.', '..#######', '.########', '..******.', '.........'],
  };
  const wicon = (px, kind, x, y, key) => px.sprite(WICON[kind], x, y, key);
  const NOTES = ['C-3', 'D#3', 'F-3', 'G-3', 'A#3', 'C-4', 'D#4', 'G-2', 'F-2', 'A#2'];

  // ================================================================ Amiga-style: dusk over a mountain lake
  const AMIGA = (() => {
    const c = x12, K = 0x000000, WH = 0xffffff;
    const SKY = ['#001', '#002', '#103', '#104', '#105', '#206', '#207', '#307', '#308', '#418', '#428', '#528', '#538', '#638',
      '#748', '#847', '#957', '#A56', '#B66', '#C65', '#D75', '#E85', '#F96', '#FA7', '#FB8'].map(c);
    const MTN = ['#214', '#325', '#436', '#547', '#658', '#769', '#87A'].map(c);
    const SNOW = ['#98B', '#BAD', '#DCE'].map(c);
    const HILL = ['#102', '#213', '#324', '#435'].map(c);
    const GND = ['#101', '#201', '#212'].map(c);
    const RIM = c('#435'), RIM2 = c('#536');
    const MOON = ['#AA9', '#CCB', '#EED', '#FFE'].map(c);
    const CL = { top: c('#425'), body: c('#536'), lit: c('#B67'), hot: c('#E97') };
    const CUM = ['#536', '#636', '#859', '#B9C'].map(c);
    const FIRE = c('#EF8'), FIRE2 = c('#8A5');
    const ORANGE = c('#F90'), GOLD = c('#FC4'), LIGHT = c('#99A'), GREEN = c('#6C4');
    const VU = ['#05A', '#06B', '#08C', '#0AD', '#2CE', '#6DE', '#AEE', '#FFF', '#FD8', '#F80'].map(c);
    const BAR = ['#420', '#840', '#C60', '#F90', '#C60', '#840'].map(c);
    const CHROME = (() => {
      const st = [[0, '#FFF'], [0.18, '#FFE'], [0.34, '#FDA'], [0.47, '#FB6'], [0.5, '#823'], [0.55, '#F86'], [0.72, '#D58'], [0.88, '#A49'], [1, '#738']];
      const out = [];
      for (let k = 0; k < 36; k++) {
        const f = k / 35; let a = 0;
        while (a < st.length - 2 && st[a + 1][0] <= f) a++;
        const [fa, ca] = st[a], [fb, cb] = st[a + 1];
        out.push(q12(mix(c(ca), c(cb), Math.min(1, (f - fa) / (fb - fa)))));
      }
      return out;
    })();
    const dark = (col) => q12(mix(col, 0x111133, 0.4));
    const HZ = 55;
    const skyAt = (y) => SKY[Math.min(SKY.length - 1, Math.floor(Math.pow(clamp(y / (HZ - 1)), 1.15) * (SKY.length - 1) + 0.5))];

    function world(px, t, o = {}) {
      for (let y = 0; y < HZ; y++) px.rect(0, y, PW, 1, skyAt(y));
      for (let k = 0; k < 70; k++) {
        const x = hash(k * 7 + 3) % PW, y = hash(k * 13 + 1) % 30, tw = Math.sin(t / 600 + k * 1.7);
        if (tw > -0.3 && y < 30 - (hash(k) % 12)) px.px(x, y, tw > 0.55 ? WH : c('#99B'));
      }
      // moon, with a lighter band of sky round it
      const mx = o.moonX ?? 298, my = 13, moon = o.moon !== false;
      if (moon) px.disc(mx, my, 10, (x, y, f) => (f > 0.62 && px.th(x, y) < 0.5 ? SKY[Math.min(SKY.length - 1, SKY.indexOf(skyAt(y)) + 3)] : undefined));
      if (moon) px.disc(mx, my, 6.5, (x, y, f) => { const cr = (x - mx === 2 && y - my === -1) || (x - mx === -2 && y - my === 2) || (x - mx === 1 && y - my === 3); return cr ? MOON[1] : px.pick(MOON, 1.05 - f * 0.75, x, y); });
      // long streak clouds, lit from below by the afterglow
      [[30, 24, 120, 4, 0], [200, 31, 96, 3, 1], [130, 17, 70, 3, 2], [270, 38, 70, 2, 3]].forEach(([x0, y0, len, th, i]) => {
        const cx = ((x0 + t / (1100 + i * 400)) % (PW + 160)) - 80;
        for (let j = 0; j < th; j++) {
          const e = (j + 0.5 - th / 2) / (th / 2 + 0.4), hl = (len / 2) * Math.sqrt(Math.max(0, 1 - e * e));
          const col = j === th - 1 ? (y0 > 26 ? CL.hot : CL.lit) : j === th - 2 ? CL.lit : j === 0 ? CL.top : CL.body;
          const a = cx - hl + noise(j * 3.1 + i, 5) * 8, b = cx + hl - noise(j * 2.3 + i, 6) * 10;
          for (let x = Math.floor(a); x < b; x++) px.px(x, y0 + j, col);
        }
      });
      if (o.wx) {   // a heap of cumulus, half over the moon
        const cx = mx - 30 + ((t / 2600) % 20);
        const blobs = [[0, 26, 8], [12, 21, 10], [27, 23, 9], [40, 27, 7], [-12, 29, 6], [50, 30, 5], [20, 13, 7]];
        const inC = (x, y) => y <= 33 && blobs.some(([dx, by, r]) => Math.hypot(x - (cx + dx), y - by) < r);
        for (let y = 2; y < 34; y++) for (let x = Math.floor(cx - 20); x < cx + 58; x++) if (inC(x, y)) {
          const v = inC(x, y - 2) ? (y > 31 ? 0 : 0.35 - (y - 20) / 40) : 0.95;
          px.px(x, y, y === 33 || (y === 32 && !inC(x, y + 2)) ? CL.hot : px.pick(CUM, v, x, y));
        }
      }
      // far mountains, slow parallax
      const o1 = t / 1600;
      const r1 = (x) => 52 - (0.65 * ridged((x + o1) / 52, 3) + 0.35 * fbm((x + o1) / 20, 4, 3)) * 28;
      for (let x = 0; x < PW; x++) {
        const r = r1(x), s = r1(x + 1) - r1(x - 1);
        for (let y = Math.floor(r); y < HZ; y++) {
          const d = y - r;
          let v = 0.5 + (s > 0 ? 0.28 : -0.2) * Math.min(1, Math.abs(s)) - d / 30 + Math.max(0, (y - 44) / 11) * 0.35;
          let col = px.pick(MTN, v, x, y);
          if (r < 37 && d < (37 - r) * 0.5 + noise(x / 3, 7) * 2 && s > -0.4) col = px.pick(SNOW, 0.45 + s * 0.4 - d / 10, x, y);
          px.px(x, y, col);
        }
      }
      // pine hills, faster parallax
      const o2 = t / 800;
      const base = (u) => 50 - 5 * fbm(u / 34, 9, 3);
      const r2 = (x) => {
        const u = x + o2; let top = base(u);
        const ci = Math.floor(u / 7);
        for (const k of [ci - 1, ci, ci + 1]) {
          if (hash(k * 31 + 7) % 3 === 0) continue;
          const cx = k * 7 + 3.5 + (hash(k) % 3) - 1, hgt = 4 + (hash(k * 5 + 1) % 7), hw = 1.5 + hgt * 0.3, dx = Math.abs(u - cx);
          if (dx < hw) top = Math.min(top, base(cx) - hgt * (1 - dx / hw));
        }
        return top;
      };
      for (let x = 0; x < PW; x++) {
        const r = r2(x), s = r2(x + 1) - r2(x - 1);
        for (let y = Math.floor(r); y < HZ; y++) { const d = y - r; px.px(x, y, d < 1 && s > 0 ? RIM : px.pick(HILL, 0.62 - d / 10 + (s > 0 ? 0.15 : 0), x, y)); }
      }
      // the lake mirrors what is above it, rippling
      for (let y = HZ; y < 74; y++) {
        const k = y - HZ;
        for (let x = 0; x < PW; x++) {
          const wob = Math.round(Math.sin(y * 1.9 + t / 260 + x * 0.05) * (0.6 + k * 0.09));
          const sy = Math.max(0, HZ - 1 - Math.floor(k * 1.7));
          let col = dark(px.get(clamp(x + wob, 0, PW - 1), sy));
          if (hash((x >> 3) * 977 + y * 131 + Math.floor(t / 500 + x / 40)) % 13 === 0) col = q12(mix(col, 0xffeedd, 0.18));
          if (!o.wx && k % 3 === 1) {   // moon path: short broken dashes, wider and dimmer toward the shore
            const off = Math.round(3 * Math.sin(k * 2.1 + t / 700)), hw = 1 + k * 0.25;
            if (Math.abs(x - mx - off) < hw && hash(Math.floor((x - off) / 2) * 31 + k * 7 + Math.floor(t / 300)) % 3) col = k < 6 ? MOON[1] : MOON[0];
          }
          px.px(x, y, col);
        }
      }
      // near shore with reeds and rocks
      const ys = (x) => 69 + 3 * fbm(x / 22, 13, 3) - (x < 70 ? (70 - x) * 0.1 : 0) - (x > 290 ? (x - 290) * 0.1 : 0);
      for (let x = 0; x < PW; x++) {
        const s0 = ys(x);
        for (let y = Math.floor(s0); y < PH; y++) { const d = y - s0; px.px(x, y, d < 1 ? RIM : d < 2 ? px.pick([GND[2], RIM], 0.4, x, y) : px.pick(GND, 0.85 - d / 12, x, y)); }
        if (rnd(x * 3 + 5) > 0.6) {
          const hh = 1 + Math.floor(rnd(x * 7 + 1) * 5) + (x > 110 && x < 175 ? 3 : 0), sway = Math.round(Math.sin(t / 900 + x * 0.7) * 0.6);
          for (let j = 0; j < hh; j++) px.px(x + (j > hh - 2 ? sway : 0), s0 - 1 - j, GND[0]);
          if (rnd(x * 11) > 0.6) px.px(x + sway, s0 - 1 - hh, c('#312'));
        }
      }
      for (const [rx, ry, rw, rh] of [[132, 74, 9, 4], [252, 76, 13, 5], [300, 72, 7, 3]]) {
        for (let y = ry - rh; y <= ry; y++) for (let x = rx - rw; x <= rx + rw; x++) {
          const e = ((x - rx) / rw) ** 2 + ((y - ry) / rh) ** 2;
          if (e <= 1) px.px(x, y, e > 0.55 && y < ry - rh * 0.3 && x > rx - rw * 0.2 ? c('#546') : c('#212'));
        }
      }
      // the old tree framing the left side: one canopy mass with a bumpy edge, lit from the upper right
      const trunk = new Set(), put = (x, y) => trunk.add(Math.round(y) * 1024 + Math.round(x));
      const limb = (x0, y0, x1, y1, t0, t1) => {
        const n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) * 2);
        for (let s = 0; s <= n; s++) { const f = s / n, x = x0 + (x1 - x0) * f, y = y0 + (y1 - y0) * f, th = t0 + (t1 - t0) * f; for (let j = -th / 2; j <= th / 2; j += 0.5) for (let i = -th / 2; i <= th / 2; i += 0.5) put(x + i, y + j); }
      };
      limb(15, 88, 18, 40, 10, 6); limb(18, 42, 46, 22, 4, 2); limb(17, 40, 8, 16, 4, 2); limb(42, 24, 76, 20, 2, 1);
      limb(44, 22, 56, 8, 2, 1); limb(11, 20, 28, 6, 2, 1); limb(14, 82, 3, 88, 3, 2); limb(19, 82, 30, 88, 3, 1);
      const sway = Math.sin(t / 1500) * 0.7;
      const CL_ = [[6, 9, 9], [20, 4, 9], [35, 1, 9], [50, 5, 8], [63, 12, 7], [76, 18, 6], [87, 22, 4], [-1, 21, 7], [13, 17, 7],
        [29, 13, 7], [45, 15, 6], [58, 21, 5], [70, 24, 4], [25, 22, 4], [38, 21, 4]];
      const inCan = (x, y) => CL_.some(([cx, cy, r], i) => {
        const dx = x + 0.5 - cx - sway * (1 - cy / 30), dy = y + 0.5 - cy;
        return Math.hypot(dx, dy) <= r * (0.82 + 0.3 * noise((Math.atan2(dy, dx) + Math.PI) * 2.2, i + 40));
      });
      const can = new Uint8Array(PW * 40);
      for (let y = 0; y < 34; y++) for (let x = 0; x < 100; x++) if (inCan(x, y)) can[y * PW + x] = 1;
      const cAt = (x, y) => x >= 0 && y >= 0 && y < 40 && x < PW && can[y * PW + x];
      for (let y = 0; y < 34; y++) for (let x = 0; x < 100; x++) {
        if (!cAt(x, y)) continue;
        const edge = !cAt(x + 1, y - 1), near = !cAt(x + 2, y - 2) || !cAt(x + 1, y - 2);
        const hole = !edge && !near && cAt(x - 2, y) && cAt(x + 2, y) && cAt(x, y + 2) && hash(Math.floor(x / 3) * 131 + Math.floor(y / 2) * 977) % 23 === 0 && hash(x * 7 + y) % 2;
        if (hole) continue;   // a little sky through the leaves
        let col = GND[0];
        if (edge) col = RIM2; else if (near) col = HILL[2];
        else if (hash(Math.floor(x / 2) * 31 + Math.floor(y / 2) * 17) % 5 === 0) col = GND[2];
        else if (!cAt(x - 1, y + 1)) col = c('#101');
        px.px(x, y, col);
      }
      for (const k of trunk) {
        const x = k % 1024, y = Math.floor(k / 1024);
        if (y < 30 && cAt(x, y) && !(x > 12 && x < 22)) continue;
        const col = !trunk.has((y) * 1024 + x + 1) ? RIM2 : !trunk.has(y * 1024 + x + 2) ? HILL[1] : (x * 3 + (y >> 2)) % 5 === 0 ? GND[2] : GND[0];
        px.px(x, y, col);
      }
      if (o.life !== false) {
        for (let i = 0; i < 7; i++) {
          const x = 34 + i * 41 + 8 * Math.sin(t / 1700 + i * 2), y = 60 + 5 * Math.sin(t / 1100 + i * 1.3) + (i % 2) * 6;
          if (Math.sin(t / 380 + i * 2.3) > -0.1) { for (const [dx, dy] of [[-1, 0], [1, 0], [0, -1], [0, 1]]) px.px(x + dx, y + dy, FIRE2); px.px(x, y, FIRE); }
        }
        for (let i = 0; i < 3; i++) {
          const bx = ((t / 70 + i * 11) % 420) - 50, by = 19 + i * 3 + Math.sin(t / 500 + i), up_ = Math.floor(t / 180 + i) % 2;
          px.px(bx, by, K); px.px(bx - 1, by - up_, K); px.px(bx + 1, by - up_, K); px.px(bx - 2, by - 1 + up_ * 0, K); px.px(bx + 2, by - 1, K);
        }
      }
    }

    const T = (px, s, x, y, col = WH, o = {}) => px.text(F.amiga, s, x, y, Object.assign({ c: col, sh: { dx: 1, dy: 1, c: K } }, o));
    function ehb(px, x, y, w, h) {
      px.map(x, y, w, h, half);
      px.rect(x, y, w, 1, LIGHT); px.rect(x, y, 1, h, LIGHT); px.rect(x, y + h - 1, w, 1, K); px.rect(x + w - 1, y, 1, h, K);
    }
    function metal(px, x, y, w, h, face = c('#778'), pressed = false) {
      px.rect(x, y, w, h, face);
      const a = pressed ? c('#223') : c('#CCD'), b = pressed ? c('#CCD') : c('#223');
      px.rect(x, y, w, 1, a); px.rect(x, y, 1, h, a); px.rect(x, y + h - 1, w, 1, b); px.rect(x + w - 1, y, 1, h, b);
      px.frame(x - 1, y - 1, w + 2, h + 2, K);
    }
    const chrome = (y0) => (x, y) => CHROME[clamp(y - y0, 0, CHROME.length - 1)];
    const SHADOW = c('#102');
    function clockPx(px, t, S) {
      world(px, t);
      const y0 = 7;
      px.bigTime(F.amiga, hhmm(), 96, y0, { s: 5, gap: 4, c: chrome(y0), ol: K, sh: { dx: 3, dy: 3, c: SHADOW } });
      ehb(px, 170, 55, 146, 28);
      T(px, S.wdLong + ' ' + S.date, 176, 59);
      icon(px, BELL, 176, 72, ORANGE); T(px, DEMO.alarm, 185, 71, ORANGE);
      battery(px, 238, 72, WH, GREEN); T(px, DEMO.batt + '%', 254, 71);
    }
    const roles = () => ({ BG: c('#103'), GHOST: c('#334'), FAINT: c('#223'), HALO: K, LIT: WH, DIM: LIGHT, HOT: ORANGE, HOTH: GOLD, KELP: GREEN });
    return {
      key: 'amiga', name: 'Amiga-style', font: 'amiga', time: '17:24', variants: { a: { label: 'Alacakaranlıkta göl' } }, roles,
      desc: 'Oyun ekranı gibi: copper gökyüzü (her satır kendi 12-bit rengi), paralaks dağ ve çam katmanları, gölde dalgalanan yansıma, ağaç silüeti, ateş böcekleri. Paneller Extra-Half-Brite: arkadaki resmi yarı parlaklığa indirir. Rakamlar krom-copper, siyah kontür.',
      copper: { sky: SKY, chrome: CHROME, vu: VU, note: 'one 12-bit color per scanline (copper list); EHB panels halve the colors behind them' },
      paint: (px, t) => world(px, t),
      world(fb, { t }) { const px = new Px(); world(px, t); px.toFb(fb); },
      clock(fb, { t, S }) { const px = new Px(); clockPx(px, t, S); px.toFb(fb); },
      alarm(fb, { t, S }) {
        const tmp = new Px(); clockPx(tmp, t, S);
        const px = new Px(); px.clear(K);
        px.d.set(tmp.d.subarray(0, (PH - 37) * PW), 37 * PW);
        const red = c('#F00');
        if (blink(t, 700)) { px.frame(3, 3, PW - 6, 31, red); px.frame(4, 4, PW - 8, 29, red); px.frame(5, 5, PW - 10, 27, red); }
        const l1 = S.alarm + ' ' + DEMO.alarm + '.   ' + S.tapStop, l2 = S.meditation + ' #00000730.00000020';
        px.text(F.amiga, l1, Math.floor((PW - px.tw(l1)) / 2), 10, { c: red });
        px.text(F.amiga, l2, Math.floor((PW - px.tw(l2)) / 2), 21, { c: red });
        px.toFb(fb);
      },
      radio(fb, { t, S }) {
        const px = new Px(); world(px, t);
        ehb(px, 4, 4, 164, 62);
        T(px, DEMO.station, 9, 8);
        T(px, 'CH0' + DEMO.ch, 164 - 36, 8, ORANGE);
        px.map(8, 18, 156, 44, half);
        BAR.forEach((col, j) => px.rect(8, 38 + j, 156, 1, col));
        for (let ch = 0; ch < 4; ch++) px.rect(21 + ch * 35, 18, 1, 44, c('#334'));
        dancers(px, 180, 50, t, K, CL.lit, CL.top);
        ehb(px, 210, 4, 106, 62);
        const L = levels(t);
        L.forEach((lv, i) => {
          const x = 213 + i * 7, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) { const y = 59 - r * 5, on = r < lv; px.rect(x, y, 6, 4, on ? VU[r] : r === pk - 1 ? WH : c('#112')); }
        });
        GENRES_MIX.forEach((g, i) => {
          const x = 5 + i * 45, cur = i === DEMO.genre;
          if (cur) metal(px, x, 70, 42, 14, ORANGE, true); else ehb(px, x, 70, 42, 14);
          T(px, g, x + Math.floor((42 - px.tw(g)) / 2), 73, cur ? K : WH, cur ? { sh: undefined } : {});
        });
        px.toFb(fb);
        // tracker rows in hi-res on top, like the trackers did
        const row = Math.floor(t / 150);
        for (let k = 0; k < 7; k++) {
          const r = row - 3 + k, y = 39 + k * 12 + 2, cur = k === 3, rn = ((r % 64) + 64) % 64;
          fb.text(F.amiga, rn.toString(16).toUpperCase().padStart(2, '0'), 20, y, { s: 1, c: cur ? K : ORANGE });
          for (let ch = 0; ch < 4; ch++) {
            const h = hash(r * 4 + ch + 1000), x = 48 + ch * 70;
            if ((h + ch) % 3 === 0 || (ch === 3 && h % 2)) { fb.text(F.amiga, '--- 00', x, y, { s: 1, c: cur ? 0x331100 : c('#557') }); continue; }
            fb.text(F.amiga, NOTES[h % NOTES.length], x, y, { s: 1, c: cur ? K : WH });
            fb.text(F.amiga, '0' + (ch + 1), x + 32, y, { s: 1, c: cur ? K : c('#8AF') });
          }
        }
      },
      weather(fb, { t, S }) {
        const px = new Px(); world(px, t, { wx: true, moonX: 262 });
        const y0 = 7;
        px.bigTime(F.amiga, WX.temp + '°', 100, y0, { s: 5, gap: 4, c: chrome(y0), ol: K, sh: { dx: 3, dy: 3, c: SHADOW } });
        T(px, S.sky[WX.sky], 100, 45);
        T(px, WX.hum + '%', 100, 55, c('#8CF')); T(px, WX.wind + ' ' + S.kmh, 148, 55, c('#8CF'));
        ehb(px, 100, 66, 216, 18);
        WX.next.forEach(([d, tmax, sky], k) => {
          const x = 104 + k * 72;
          wicon(px, sky, x, 70, { w: WH, g: LIGHT, y: GOLD, b: c('#6AF') });
          T(px, S.days[d], x + 16, 71); T(px, tmax + '°', x + 48, 71, ORANGE);
        });
        px.toFb(fb);
      },
      scene(fb, { t }) {
        const px = new Px(), r = (a) => a.map(c);
        sceneLow(px, {
          sky: r(['#001', '#002', '#012', '#013', '#023', '#024', '#134', '#135', '#245', '#356', '#467', '#578', '#879', '#A89']),
          star: r(['#89B', '#FFF']), moon: r(['#DDC', '#FFE']), land: r(['#012']),
          sea: r(['#247', '#136', '#125', '#024', '#013', '#012', '#002']), wave: r(['#48B', '#6AD']), glint: r(['#FFD', '#FFF']),
          rock: r(['#222', '#334']), tw: r(['#EEE']), tr: r(['#C22']), iron: r(['#333']), lamp: r(['#FF8']),
          sand: r(['#753', '#642', '#531', '#420']), foam: r(['#DEF']), palm: r(['#011']), sail: r(['#AAB', '#DDC']), hull: r(['#822']),
          beam: r(['#456', '#789', '#BBA', '#FFD']),
        }, t, { band: true });
        T(px, hhmm(), 274, 74);
        px.toFb(fb);
      },
      settings(fb, { t, S, theme }) {
        const px = new Px(); world(px, t, { life: false, moon: false }); px.map(0, 0, PW, PH, half);
        ehb(px, 4, 4, 150, 78);
        S.items.slice(0, 7).forEach((it, i) => {
          const y = 8 + i * 10;
          if (i === 0) { BAR.forEach((col, j) => px.rect(6, y - 1 + j * 1.5, 140, 2, col)); T(px, it, 10, y, K, { sh: undefined }); }
          else T(px, it, 10, y);
        });
        px.rect(147, 6, 5, 74, c('#112')); metal(px, 148, 7, 3, 44, LIGHT);
        T(px, S.items[0], 162, 8);
        T(px, '1/5', 316 - 28, 8, LIGHT);
        metal(px, 162, 24, 30, 38); metal(px, 288, 24, 28, 38);
        px.poly([[183, 31], [183, 55], [170, 43]], K); px.poly([[297, 31], [297, 55], [310, 43]], K);
        px.rect(196, 31, 88, 24, c('#112')); px.rect(196, 31, 88, 1, K); px.rect(196, 31, 1, 24, K); px.rect(196, 54, 88, 1, LIGHT); px.rect(283, 31, 1, 24, LIGHT);
        const nm = theme, s = px.tw(nm) <= 84 ? 1 : 1;
        T(px, nm, 196 + Math.floor((88 - px.tw(nm, s)) / 2), 39, ORANGE);
        for (let k = 0; k < 5; k++) { const x = 212 + k * 12; if (k === 4) metal(px, x, 66, 6, 6, ORANGE); else px.frame(x, 66, 6, 6, LIGHT); }
        T(px, S.hint, 162, 76, LIGHT);
        px.toFb(fb);
      },
    };
  })();

  // ================================================================ C64-style: night city, a car on the highway
  const C = { k: 0x000000, w: 0xFFFFFF, r: 0x68372B, c: 0x70A4B2, p: 0x6F3D86, g: 0x588D43, b: 0x352879, y: 0xB8C76F, o: 0x6F4F25,
    n: 0x433900, lr: 0x9A6759, dg: 0x444444, gr: 0x6C6C6C, lg: 0x9AD284, lb: 0x6C5EB5, lgr: 0x959595 };
  const C64 = (() => {
    const BT = 4, BS = 8, BORDER = C.b;
    const PAL16 = [C.k, C.w, C.r, C.c, C.p, C.g, C.b, C.y, C.o, C.n, C.lr, C.dg, C.gr, C.lg, C.lb, C.lgr];
    const skyRow = (y) => (y < 5 ? C.k : y < 9 ? (y % 2 ? C.b : C.k) : y < 15 ? C.b : y < 19 ? (y % 2 ? C.p : C.b) : y < 25 ? C.p
      : y < 29 ? (y % 2 ? C.lr : C.p) : y < 35 ? C.lr : y < 39 ? (y % 2 ? C.y : C.lr) : y < 44 ? C.y : y % 2 ? C.w : C.y);
    const CAR = [
      '..........kkkkkkkkkk............',
      '........kkLLLLccLLLLkk..........',
      '.......kLLLLLccLLLLLLLk.........',
      '..kkkkkkkkkkkkkkkkkkkkkkkkkk....',
      '.kRRRRRRRRRRRRRRRRRRRRRRRRRRRkk.',
      'kTrrrrrrrrrrrrrrrrrrrrrrrrrrrrYk',
      'kTrrrrrrrrrrrrrrrrrrrrrrrrrrrrYk',
      'kkrrrkkkkkrrrrrrrrrrrkkkkkrrrrkk',
      '...kkABAkk..........kkABAkk.....',
      '...kBABABk..........kBABABk.....',
      '....kkkkk............kkkkk......',
    ];
    function world(px, t, o = {}) {
      px.clear(C.k); px.dm = 'checker';
      px.wide = true;
      for (let y = 0; y < 52; y++) px.rect(0, y, PW, 1, skyRow(y));
      px.wide = false;
      for (let k = 0; k < 50; k++) { const x = hash(k * 7 + 3) % PW, y = hash(k * 13 + 1) % 12; if (Math.sin(t / 500 + k * 2.1) > -0.2) px.px(x, y, k % 3 ? C.lgr : C.w); }
      px.wide = true;
      px.disc(262, 15, 9, (x, y, f) => {
        const cr = hash((x >> 1) * 31 + y * 7) % 9 === 0 && f < 0.8;
        return cr ? C.gr : f > 0.86 ? C.lgr : (x - 262) + (y - 15) < -4 ? C.w : C.lgr;
      });
      if (o.clouds) [[150, 16, 1.4], [228, 24, 1.1], [44, 8, 0.8]].forEach(([x0, y0, sc], i) => {
        const cx = ((x0 + t / 140) % 400) - 40;
        const blobs = [[0, 0, 6], [8, -3, 7], [17, 0, 6], [24, 2, 4], [-8, 2, 4]];
        for (let y = Math.floor(y0 - 10 * sc); y <= y0 + 3; y++) for (let x = Math.floor(cx - 14 * sc); x < cx + 30 * sc; x += 2) {
          if (!blobs.some(([dx, dy, r]) => Math.hypot((x - cx - dx * sc) / 1.3, y - y0 - dy * sc) < r * sc)) continue;
          const v = (y0 + 3 - y) / (10 * sc);
          px.px(x, y, v < 0.18 ? C.dg : v < 0.3 ? (((x >> 1) + y) % 2 ? C.gr : C.dg) : v < 0.55 ? C.gr : v < 0.65 ? (((x >> 1) + y) % 2 ? C.lgr : C.gr) : C.lgr);
        }
      });
      // back skyline
      const o1 = Math.floor(t / 90) * 2;
      for (let x = 0; x < PW; x += 2) {
        const u = x + o1, i = Math.floor(u / 14), low = hash(i * 3) % 5 === 0, top = low ? 46 : 29 + (hash(i * 7 + 1) % 16);
        for (let y = top; y < 57; y++) px.px(x, y, u % 4 === 2 && y % 4 === 1 && y > top + 1 && hash(i * 131 + y * 17 + (u >> 2)) % 3 === 0 ? C.gr : C.dg);
      }
      // front skyline with lit windows
      const o2 = Math.floor(t / 45) * 2;
      for (let x = 0; x < PW; x += 2) {
        const u = x + o2, i = Math.floor(u / 18), lx = u - i * 18, wdt = 10 + (hash(i * 5) % 4) * 2;
        if (lx >= wdt) continue;
        const top = 35 + (hash(i * 13 + 2) % 15);
        for (let y = top; y < 57; y++) {
          let col = C.k;
          if (lx > 1 && lx < wdt - 2 && lx % 4 === 2 && (y - top) % 4 === 2 && y < 54) {
            const h = hash(i * 977 + (y - top) * 31 + lx);
            col = h % 5 === 0 ? C.k : h % 7 === 0 ? C.lr : h % 3 === 0 ? C.w : C.y;
            if (h % 11 === 0 && Math.floor(t / 1500 + (h % 7)) % 2) col = C.k;
          }
          px.px(x, y, col);
        }
        if (hash(i * 17) % 4 === 0 && lx === 4) {
          px.wide = false;
          for (let k = 1; k < 6; k++) px.px(x + 1, top - k, C.gr);
          if (blink(t + i * 300, 600)) px.px(x + 1, top - 6, C.lr);
          px.wide = true;
        }
      }
      // sidewalk, street lamps, road
      px.rect(0, 57, PW, 3, C.gr); px.rect(0, 59, PW, 1, C.dg);
      px.rect(0, 60, PW, 12, C.dg);
      for (let x = 0; x < PW; x += 2) { if (((x + Math.floor(t / 9) * 2) % 24) < 12) px.rect(x, 65, 2, 1, C.lgr); if (hash(x * 13) % 9 === 0) px.px(x, 61 + hash(x) % 10, C.k); }
      px.rect(0, 72, PW, 1, C.gr);
      px.wide = false;
      const o3 = t / 22;
      for (let k = -1; k < 9; k++) {
        const x = Math.round(k * 44 - (o3 % 44));
        for (let j = 57; j < 60; j++) for (let i = -3; i < 7; i++) if ((x + i + j) % 2 === 0) px.px(x + i, j, C.y);
        px.rect(x, 39, 1, 18, C.lgr); px.rect(x, 39, 4, 1, C.lgr); px.rect(x + 3, 40, 3, 1, C.y); px.px(x + 4, 41, C.w);
      }
      if (o.car !== false) {
        for (let j = 63; j < 71; j++) for (let i = 71; i < 71 + (j - 62) * 5; i++) if ((i + j) % 2 === 0) px.px(i, j, C.lgr);
        const f = Math.floor(t / 90) % 2;
        px.sprite(CAR, 40, 61, { k: C.k, L: C.lb, c: C.c, R: C.lgr, r: C.c, T: C.lr, Y: C.y, A: f ? C.lgr : C.gr, B: f ? C.gr : C.lgr });
      }
      px.wide = false;
    }
    function border(px, col = BORDER) { px.wide = false; px.rect(0, 0, PW, BT, col); px.rect(0, PH - BT, PW, BT, col); px.rect(0, 0, BS, PH, col); px.rect(PW - BS, 0, BS, PH, col); }
    // tape loading: stripes in the border, more colors the louder it plays
    function stripes(px, t, amp) {
      const fr = Math.floor(t / 50), n = Math.max(2, Math.round(amp * 1.6));
      for (let y = 0; y < PH;) {
        const h = 1 + (hash(y * 13 + fr) % 3), col = PAL16[(hash(y * 7 + fr * 31) % n) + (n <= 2 ? 6 : 0)];
        if (y < BT || y >= PH - BT) px.rect(0, y, PW, h, col); else { px.rect(0, y, BS, h, col); px.rect(PW - BS, y, BS, h, col); }
        y += h;
      }
      px.rect(BS, Math.max(BT, 0), PW - 2 * BS, 0, 0);
    }
    const WASH = [C.b, C.lb, C.c, C.lgr, C.w, C.y, C.lr, C.p];
    const wash = (y0, t) => (x, y) => WASH[((((y - y0) >> 1) + Math.floor(t / 90)) % WASH.length + WASH.length) % WASH.length];
    const T = (px, s, x, y, col = C.w, o = {}) => px.text(F.c64, up(s, lang()), x, y, Object.assign({ c: col }, o));
    const bar = (px, y, h = 9) => px.rect(BS, y, PW - 2 * BS, h, C.k);
    const mini = (px, kind, x, y) => icon(px, WMINI[kind], x, y, kind === 'clear' ? C.y : C.lgr, kind === 'rain' ? C.lb : C.gr);
    function win(px, x, y, w, h, title) {
      px.rect(x, y, w, h, C.k); px.frame(x, y, w, h, C.lb); px.frame(x + 1, y + 1, w - 2, h - 2, C.lb);
      if (title) { px.rect(x + 2, y + 2, w - 4, 9, C.lb); T(px, title, x + 5, y + 3, C.b); }
    }
    const roles = () => ({ BG: C.k, GHOST: C.dg, FAINT: C.gr, HALO: BORDER, LIT: C.w, DIM: C.lb, HOT: C.y, HOTH: C.lr, KELP: C.lg });
    return {
      key: 'c64', name: 'C64-style', font: 'c64', time: '23:05', variants: { a: { label: 'Gece şehri' } }, roles, border: { top: BT * 2, sides: BS * 2 },
      desc: 'Oyun ekranı gibi: çift genişlikte multicolor pikseller, satır satır raster çizgili gökyüzü, iki katmanlı kayan şehir silüeti, sokak lambaları, otoyolda giden hi-res araba sprite\'ı. Kenarlık rengi her sayfada; radyoda kaset yükleme şeritleri müzikle çoğalır. Rakamlar satır satır renk döngüsü (intro tarzı).',
      paint: (px, t) => { world(px, t); border(px); },
      world(fb, { t }) { const px = new Px(); world(px, t); border(px); px.toFb(fb); },
      clock(fb, { t, S }) {
        const px = new Px(); world(px, t); border(px);
        const y0 = 8;
        px.bigTime(F.c64, hhmm(), 16, y0, { sx: 6, sy: 5, gap: 4, c: wash(y0, t), ol: C.k, sh: { dx: 2, dy: 2, c: C.k } });
        bar(px, 73);
        T(px, S.wd + ' ' + S.date, 12, 74);
        icon(px, BELL, 112, 75, C.y); T(px, DEMO.alarm, 121, 74, C.y);
        battery(px, 254, 75, C.lg, C.lg); T(px, DEMO.batt + '%', 270, 74, C.lg);
        px.toFb(fb);
      },
      radio(fb, { t, S }) {
        const px = new Px(); world(px, t);
        const L = levels(t), avg = L.reduce((a, b) => a + b, 0) / L.length;
        stripes(px, t, avg);
        bar(px, BT, 19);
        T(px, 'LOAD"' + DEMO.station + '"', 12, 5, C.lb);
        const l2 = S.loading + ' ' + DEMO.title;
        const e = T(px, l2, 12, 14, C.w);
        if (blink(t, 400)) px.rect(e + 2, 14, 7, 7, C.lb);
        // the tape deck
        const dx = 12, dy = 26;
        px.rect(dx, dy, 50, 22, C.lgr); px.frame(dx, dy, 50, 22, C.dg); px.rect(dx + 1, dy + 1, 48, 1, C.w);
        px.rect(dx + 9, dy + 3, 32, 11, C.k); px.frame(dx + 9, dy + 3, 32, 11, C.gr);
        const a = t / 140;
        for (const rx of [dx + 18, dx + 32]) {
          px.disc(rx, dy + 8.5, 4, (x, y, f) => (f > 0.6 ? C.w : undefined));
          for (let k = 0; k < 3; k++) { const an = a + k * 2.094; px.px(rx + Math.round(Math.cos(an) * 2), dy + 8 + Math.round(Math.sin(an) * 2), C.w); }
        }
        for (let k = 0; k < 6; k++) { const kx = dx + 3 + k * 8; px.rect(kx, dy + 16, 7, 4, k === 1 ? C.gr : C.dg); px.rect(kx, dy + 16, 7, 1, k === 1 ? C.dg : C.gr); }
        dancers(px, 178, 25, t, C.w, C.lgr, C.gr);
        px.rect(208, 22, 104, 47, C.k);
        L.forEach((lv, i) => {
          const x = 211 + i * 7, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) { const y = 64 - r * 4, on = r < lv; px.rect(x, y, 5, 3, on ? (r < 6 ? C.lg : r < 8 ? C.y : C.lr) : r === pk - 1 ? C.w : C.k); if (!on && r !== pk - 1) px.rect(x + 2, y + 1, 1, 1, C.dg); }
        });
        bar(px, 73);
        GENRES.forEach((g, i) => {
          const x = BS + i * 43 + 1, cur = i === DEMO.genre;
          if (cur) px.rect(x, 73, 42, 9, C.lg);
          T(px, g, x + Math.floor((42 - px.tw(g)) / 2), 74, cur ? C.k : C.lgr);
        });
        px.toFb(fb);
      },
      weather(fb, { t, S }) {
        const px = new Px(); world(px, t, { clouds: true }); border(px);
        const y0 = 8;
        px.bigTime(F.c64, WX.temp + '°', 16, y0, { sx: 6, sy: 5, gap: 4, c: wash(y0, t), ol: C.k, sh: { dx: 2, dy: 2, c: C.k } });
        T(px, S.sky[WX.sky], 16, 47, C.w, { ol: C.k });
        win(px, 196, 8, 116, 30);
        T(px, S.hum + ' ' + WX.hum + '%', 200, 13, C.lb);
        T(px, S.windL + ' ' + WX.wind + ' ' + S.kmh, 200, 25, C.lb);
        bar(px, 73);
        WX.next.forEach(([d, tmax, sky], k) => {
          const x = 14 + k * 100;
          mini(px, sky, x, 74); T(px, S.days[d], x + 12, 74, C.lgr); T(px, tmax + '°', x + 46, 74, C.y);
        });
        px.toFb(fb);
      },
      scene(fb, { t }) {
        const px = new Px(); px.wide = true; px.dm = 'checker';
        sceneLow(px, {
          sky: [C.k, C.k, C.b, C.p], star: [C.lgr, C.w], moon: [C.lgr, C.w], land: [C.k], sea: [C.b, C.b, C.k], wave: [C.lb, C.c],
          glint: [C.y, C.w], rock: [C.k, C.dg], tw: [C.w], tr: [C.r], iron: [C.dg], lamp: [C.y], sand: [C.o, C.n], foam: [C.w],
          palm: [C.k], sail: [C.lgr, C.w], hull: [C.r], beam: [C.gr, C.y, C.w],
        }, t);
        px.wide = false; border(px);
        T(px, hhmm(), PW - BS - 4 - 40, PH - BT - 11, C.w, { ol: C.k });
        px.toFb(fb);
      },
      settings(fb, { t, S, theme }) {
        const px = new Px(); world(px, t); border(px);
        win(px, 12, 7, 148, 72, S.options);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 20 + i * 9, sel = i === 0;
          T(px, (sel ? '▶' : ' ') + (i + 1) + ' ' + it, 16, y, sel ? wash(y, t) : C.lb);
        });
        win(px, 166, 7, 142, 72);
        T(px, S.items[0], 172, 11, C.lb); T(px, '1/5', 300 - 24, 11, C.gr);
        px.rect(172, 24, 26, 34, C.lb); px.rect(278, 24, 26, 34, C.lb);
        px.poly([[191, 31], [191, 51], [179, 41]], C.k); px.poly([[285, 31], [285, 51], [297, 41]], C.k);
        const nm = up(theme, lang());
        T(px, nm, 202 + Math.floor((74 - px.tw(nm)) / 2), 37, wash(37, t));
        for (let k = 0; k < 5; k++) px.rect(206 + k * 12, 62, 6, 4, k === 3 ? C.w : C.dg);
        T(px, S.hint, 172, 69, C.gr);
        px.toFb(fb);
      },
    };
  })();

  // ================================================================ EGA: a valley with a keep, Sierra-era dither
  const E = { k: 0x000000, b: 0x0000AA, g: 0x00AA00, c: 0x00AAAA, r: 0xAA0000, m: 0xAA00AA, br: 0xAA5500, lg: 0xAAAAAA,
    dg: 0x555555, lb: 0x5555FF, lgn: 0x55FF55, lc: 0x55FFFF, lr: 0xFF5555, lm: 0xFF55FF, y: 0xFFFF55, w: 0xFFFFFF };
  const EGA = (() => {
    const HERO = [
      ['.rr..', 'rrrr.', '.pp..', '.pp..', 'bbbb.', 'bbbbb', 'pbbbp', '.bbb.', '.b.b.', '.b..b', 'kk..k'],
      ['.rr..', 'rrrr.', '.pp..', '.pp..', 'bbbb.', 'bbbbb', 'pbbbp', '.bbb.', '.b.b.', 'b..b.', 'k..kk'],
    ];
    const pathX = (y) => 252 + (128 - 252) * Math.pow(clamp((y - 40) / 46), 1.2) + 8 * Math.sin((y - 40) / 7);
    function world(px, t, o = {}) {
      px.dm = 'checker';
      for (let y = 0; y < 46; y++) for (let x = 0; x < PW; x++) px.px(x, y, px.pick([E.b, E.lb, E.lc], Math.pow(y / 44, 0.85), x, y));
      // sun, then clouds that may cover part of it
      px.disc(286, 11, 7, (x, y, f) => (f < 0.55 ? E.w : E.y));
      const clouds = o.wx ? [[40, 12, 1], [150, 8, 0.8], [262, 14, 1.2], [210, 24, 0.7]] : [[40, 12, 1], [150, 8, 0.8], [232, 18, 0.7]];
      clouds.forEach(([x0, y0, s], i) => {
        const cx = ((x0 + t / 300) % 400) - 40;
        const blobs = [[0, 0, 6], [7, -3, 7], [15, 0, 6], [21, 2, 4], [-7, 2, 4]];
        for (let y = Math.floor(y0 - 11 * s); y < y0 + 5 * s; y++) for (let x = Math.floor(cx - 14 * s); x < cx + 30 * s; x++) {
          if (y > y0 + 3 * s || !blobs.some(([dx, dy, r]) => Math.hypot(x - cx - dx * s, y - y0 - dy * s) < r * s)) continue;
          px.px(x, y, y >= y0 + 3 * s - 1 ? E.lg : y > y0 ? px.pick([E.w, E.lg], 0.5, x, y) : E.w);
        }
      });
      // far mountains
      const r1 = (x) => 42 - (0.7 * ridged(x / 40, 21) + 0.3 * fbm(x / 15, 22, 2)) * 22;
      for (let x = 0; x < PW; x++) {
        const r = r1(x), s = r1(x + 1) - r1(x - 1);
        for (let y = Math.floor(r); y < 48; y++) {
          const d = y - r, snow = r < 30 && d < (30 - r) * 0.6 + 1;
          px.px(x, y, snow ? (s < 0 ? px.pick([E.lg, E.w], 0.5, x, y) : E.w) : s < 0 ? px.pick([E.b, E.lb], 0.5, x, y) : E.lb);
        }
      }
      // rolling hills
      const r2 = (x) => 44 - 7 * fbm(x / 30, 23, 3) + 4 * Math.sin(x / 41);
      for (let x = 0; x < PW; x++) {
        const r = r2(x), s = r2(x + 1) - r2(x - 1);
        for (let y = Math.floor(r); y < PH; y++) px.px(x, y, y - r < 2 && s > -0.2 ? px.pick([E.g, E.lgn], 0.5, x, y) : E.g);
      }
      // little round trees along the hills
      for (let k = 0; k < 14; k++) {
        const tx = 96 + k * 9 + (hash(k) % 5), ty = r2(tx) - 2, r = 2 + (hash(k * 3) % 2);
        px.rect(tx, ty, 1, 4, E.br);
        px.disc(tx + 0.5, ty - 1, r, (x, y) => ((x - tx) + (y - ty) < -1 ? px.pick([E.g, E.lgn], 0.5, x, y) : (x - tx) + (y - ty) > 1 ? px.pick([E.g, E.k], 0.5, x, y) : E.g));
      }
      // castle hill and the keep
      for (let x = 190; x < 316; x++) { const top = 37 + ((x - 252) / 44) ** 2 * 14; for (let y = Math.floor(top); y < 56; y++) px.px(x, y, y - top < 2 ? px.pick([E.g, E.lgn], 0.5, x, y) : E.g); }
      const stone = (x, y, x0, x1) => (x === x0 ? E.w : x >= x1 - 2 ? px.pick([E.lg, E.dg], 0.5, x, y) : (y % 4 === 0 && ((x + (y >> 2)) % 6 === 0)) ? E.dg : E.lg);
      const block = (x0, y0, x1, y1) => { for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) px.px(x, y, stone(x, y, x0, x1)); };
      block(228, 26, 278, 39);
      for (let x = 228; x < 278; x += 4) px.rect(x, 24, 2, 2, E.lg);
      block(222, 16, 232, 39); block(274, 18, 284, 39); block(244, 11, 260, 30);
      const roof = (x0, x1, y1, h) => { const mxr = (x0 + x1) / 2; px.poly([[x0 - 1, y1], [mxr, y1 - h], [x1 + 1, y1]], (x, y) => (x < mxr ? px.pick([E.r, E.lr], 0.5, x, y) : E.r)); };
      roof(222, 232, 16, 9); roof(274, 284, 18, 9); roof(244, 260, 11, 10);
      const wave = Math.floor(t / 250) % 2;
      for (const [fx, fy] of [[227, 6], [279, 8], [252, 0]]) { px.rect(fx, fy, 1, 4, E.dg); px.rect(fx + 1, fy + wave, 4, 2, E.y); px.px(fx + 4 + wave, fy + 1, E.y); }
      for (const [wx, wy] of [[226, 22], [278, 24], [250, 17], [254, 17], [236, 31], [268, 31]]) px.rect(wx, wy, 1, 2, wx === 250 && blink(t, 900) ? E.y : E.k);
      px.rect(249, 33, 6, 6, E.k); px.rect(250, 32, 4, 1, E.k);
      // winding path down from the gate
      for (let y = 40; y < PH; y++) {
        const cx = pathX(y), w = 1.5 + (y - 40) * 0.36;
        for (let x = Math.floor(cx - w); x <= cx + w; x++) px.px(x, y, Math.abs(x - cx) > w - 1 ? px.pick([E.br, E.lg], 0.5, x, y) : rnd(x * 7 + y * 131) > 0.92 ? E.lg : E.br);
      }
      // river
      for (let y = 50; y < PH; y++) {
        const cx = 96 - (y - 50) * 2.4 + 5 * Math.sin(y / 5), rw = 2 + (y - 50) * 0.45;
        for (let x = Math.floor(cx - rw - 1); x <= cx + rw + 1; x++) {
          const d = Math.abs(x - cx);
          let col = d > rw ? E.g : d > rw - 1 ? px.pick([E.b, E.lb], 0.5, x, y) : E.b;
          if (d < rw - 1 && hash(x * 7 + y * 131 + Math.floor(t / 300) * 17) % 13 === 0) col = E.w;
          else if (d < rw * 0.4 && (x + y) % 2 === 0) col = E.lb;
          px.px(x, y, col);
        }
      }
      // meadow texture, tufts and flowers in the foreground
      for (let y = 56; y < PH; y++) for (let x = 0; x < PW; x++) {
        const cur = px.get(x, y);
        if (cur === E.br || cur === E.b || cur === E.lb || cur === E.w || cur === E.lg) continue;
        if (cur !== E.g && cur !== E.lgn && y < 58) continue;
        const h = hash(x * 131 + y * 977);
        px.px(x, y, h % 71 === 0 ? [E.y, E.lr, E.w, E.lm][h % 4] : px.pick([E.g, E.lgn], 0.12 + (y - 56) / 70 + (fbm(x / 16 + y * 0.31, 3, 2) - 0.45) * 1.1, x, y));
      }
      // big tree framing the right side
      px.rect(300, 26, 6, 60, E.br); px.rect(304, 26, 2, 60, px.pick([E.br, E.k], 0.5, 0, 0)); px.rect(298, 82, 10, 4, E.br);
      for (const [cx, cy, r] of [[302, 12, 13], [288, 22, 9], [316, 22, 10], [297, 30, 8], [312, 32, 7]]) px.disc(cx, cy, r, (x, y) => {
        const s = (x - cx) + (y - cy);
        return s < -r * 0.45 ? px.pick([E.g, E.lgn], 0.5, x, y) : s > r * 0.5 ? px.pick([E.g, E.k], 0.5, x, y) : E.g;
      });
      if (o.walker !== false) {
        const p = (t / 26000 + 0.45) % 1, hy = Math.round(84 - p * 38), hx = Math.round(pathX(hy + 10)) - 2;
        px.sprite(HERO[Math.floor(t / 260) % 2], hx, hy, { r: E.r, p: E.lr, b: E.b, k: E.k });
      }
      for (let i = 0; i < 2; i++) { const bx = ((t / 60 + i * 30) % 380) - 30, by = 14 + i * 5, w = Math.floor(t / 200 + i) % 2; px.px(bx, by, E.k); px.px(bx - 1, by - w, E.k); px.px(bx + 1, by - w, E.k); px.px(bx - 2, by - 1, E.k); px.px(bx + 2, by - 1, E.k); }
    }
    const T = (px, s, x, y, col = E.k, o = {}) => px.text(F.ega, s, x, y, Object.assign({ c: col }, o));
    function box(px, x, y, w, h) {
      px.rect(x, y, w, h, E.w);
      px.frame(x, y, w, h, E.b); px.frame(x + 1, y + 1, w - 2, h - 2, E.b); px.frame(x + 3, y + 3, w - 6, h - 6, E.lb);
    }
    function button(px, x, y, w, h, pressed = false) {
      px.rect(x, y, w, h, pressed ? E.dg : E.lg);
      px.rect(x, y, w, 1, pressed ? E.k : E.w); px.rect(x, y, 1, h, pressed ? E.k : E.w);
      px.rect(x, y + h - 1, w, 1, pressed ? E.lg : E.dg); px.rect(x + w - 1, y, 1, h, pressed ? E.lg : E.dg);
      px.frame(x - 1, y - 1, w + 2, h + 2, E.k);
    }
    const titleDigits = (y0) => ({ s: 5, gap: 4, c: (x, y) => (y - y0 < 5 ? E.w : E.y), ol: E.k, sh: { dx: 3, dy: 3, c: E.br } });
    function knob(px, cx, cy, r, a) {
      px.disc(cx, cy, r + 1, E.k);
      px.disc(cx, cy, r, (x, y, f) => { const s = (x - cx) + (y - cy); return f > 0.8 && (Math.round(Math.atan2(y - cy, x - cx) * 6) % 2) ? E.dg : s < -r * 0.5 ? E.w : s > r * 0.5 ? E.dg : E.lg; });
      px.line(cx, cy, cx + Math.cos(a) * (r - 2), cy + Math.sin(a) * (r - 2), E.k);
    }
    const roles = () => ({ BG: E.b, GHOST: E.lb, FAINT: E.dg, HALO: E.k, LIT: E.w, DIM: E.lc, HOT: E.y, HOTH: E.br, KELP: E.lgn });
    return {
      key: 'ega', name: 'EGA', font: 'ega', time: '10:24', variants: { a: { label: 'Kale vadisi' } }, roles,
      desc: '80\'lerin macera oyunu ekranı gibi: 16 renk, %50 dama dither ile gökyüzü ve gölgeler, karlı dağlar, tepede kale, kıvrılan patika, nehir, yolda yürüyen küçük kahraman. Yazılar beyaz mesaj kutusunda; radyo arabanın konsolunda (ön camdan vadi görünür, torpidoda dans eden çift biblosu).',
      paint: (px, t) => world(px, t),
      world(fb, { t }) { const px = new Px(); world(px, t); px.toFb(fb); },
      clock(fb, { t, S }) {
        const px = new Px(); world(px, t);
        px.bigTime(F.ega, hhmm(), 10, 6, titleDigits(6));
        box(px, 6, 57, 308, 28);
        T(px, S.narr[0], 14, 63); T(px, S.narr[1], 14, 73);
        battery(px, 272, 74, E.k, E.g); T(px, DEMO.batt + '%', 286, 73, E.dg);
        px.toFb(fb);
      },
      radio(fb, { t, S }) {
        const view = new Px(); world(view, t, { walker: false });
        const px = new Px(); px.dm = 'checker';
        for (let y = 0; y < 32; y++) px.d.set(view.d.subarray((y + 12) * PW, (y + 13) * PW), y * PW);
        px.poly([[0, 0], [24, 0], [7, 33], [0, 33]], E.k); px.line(24, 0, 7, 33, E.dg);
        px.poly([[296, 0], [320, 0], [320, 33], [313, 33]], E.k); px.line(296, 0, 313, 33, E.dg);
        px.rect(118, 0, 50, 8, E.k); px.rect(120, 0, 46, 6, E.lb); px.rect(120, 4, 46, 2, E.g); px.rect(140, 0, 6, 1, E.dg);
        for (let x = 0; x < PW; x++) { const top = Math.round(31 + ((x - 160) / 160) ** 2 * 3); for (let y = top; y < PH; y++) px.px(x, y, y === top ? E.lg : y === top + 1 ? E.dg : px.pick([E.k, E.dg], 0.4 - (y - 34) * 0.006, x, y)); }
        // the figurine on the dash
        px.rect(176, 29, 28, 3, E.br); px.rect(176, 29, 28, 1, E.y);
        dancers(px, 177, -3, t, E.y, E.w, E.br);
        for (let k = 0; k < 14; k++) px.rect(4, 38 + k * 3, 14, 1, k % 2 ? E.dg : E.k);
        px.frame(3, 37, 16, 44, E.dg);
        // the radio
        px.rect(22, 36, 186, 48, E.lg); px.frame(21, 35, 188, 50, E.k); px.rect(22, 36, 186, 1, E.w); px.rect(22, 36, 1, 48, E.w); px.rect(22, 83, 186, 1, E.dg); px.rect(207, 36, 1, 48, E.dg);
        knob(px, 36, 50, 9, -2.2 + Math.sin(t / 3000) * 0.1); knob(px, 194, 50, 9, -0.6);
        px.rect(50, 39, 132, 23, E.k); px.frame(49, 38, 134, 25, E.dg);
        for (let x = 54; x <= 178; x += 4) { const major = (x - 54) % 20 === 0; px.rect(x, major ? 51 : 53, 1, major ? 4 : 2, major ? E.lg : E.dg); }
        const nx = Math.round(54 + DEMO.stationIndex / (DEMO.stationCount - 1) * 124);
        px.rect(nx, 41, 1, 19, E.lr); px.rect(nx - 1, 41, 3, 1, E.lr);
        if (blink(t, 1000)) px.rect(174, 41, 2, 2, E.lr);
        GENRES.forEach((g, i) => { const x = 26 + i * 26, cur = i === DEMO.genre; button(px, x, 67, 23, 13, cur); if (cur) px.rect(x + 10, 64, 3, 2, E.lr); });
        // graphic equaliser
        px.rect(212, 36, 104, 48, E.k); px.frame(211, 35, 106, 50, E.dg); px.rect(212, 36, 104, 1, E.lg);
        const L = levels(t);
        L.forEach((lv, i) => {
          const x = 215 + i * 7, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) {
            const y = 78 - r * 4, on = r < lv || r === pk - 1, hot = r >= 8, warm = r >= 6;
            px.rect(x, y, 5, 3, on ? (hot ? E.lr : warm ? E.y : E.lgn) : hot ? E.r : warm ? E.br : E.g);
          }
        });
        px.toFb(fb);
        // hi-res legends, like the printing on the faceplate
        fb.text(F.ega, DEMO.station, 106, 82, { s: 1, c: E.lc });
        fb.text(F.ega, 'FM', 342, 82, { s: 1, c: E.lr });
        [88, 92, 96, 100, 104, 108].forEach((f, k) => fb.text(F.ega, String(f), 108 + k * 40 - (f >= 100 ? 12 : 8), 112, { s: 1, c: E.lg }));
        fb.text(F.ega, 'VOL', 60, 122, { s: 1, c: E.dg }); fb.text(F.ega, 'TUNE', 372, 122, { s: 1, c: E.dg });
        GENRES.forEach((g, i) => { const cx = 2 * (26 + i * 26) + 23, cur = i === DEMO.genre; fb.text(F.ega, g, cx - g.length * 4, 140, { s: 1, c: cur ? E.w : E.k }); });
      },
      weather(fb, { t, S }) {
        const px = new Px(); world(px, t, { wx: true });
        px.bigTime(F.ega, WX.temp + '°', 10, 6, titleDigits(6));
        box(px, 202, 4, 114, 50);
        WX.next.forEach(([d, tmax, sky], k) => {
          const y = 8 + k * 11;
          wicon(px, sky, 208, y, { w: E.lg, g: E.dg, y: E.br, b: E.b });
          T(px, S.days[d], 226, y + 1); T(px, tmax + '°', 290, y + 1, E.r);
        });
        T(px, WX.hum + '%  ' + WX.wind + S.kmh, 208, 43, E.dg);
        box(px, 6, 57, 308, 28);
        T(px, S.wxNarr[0], 14, 63); T(px, S.wxNarr[1], 14, 73);
        px.toFb(fb);
      },
      scene(fb, { t }) {
        const px = new Px(); px.dm = 'checker';
        sceneLow(px, {
          sky: [E.k, E.k, E.b, E.b, E.m], star: [E.lg, E.w], moon: [E.y, E.w], land: [E.k], sea: [E.b, E.b, E.k], wave: [E.c, E.lc],
          glint: [E.y, E.w], rock: [E.k, E.dg], tw: [E.w], tr: [E.r], iron: [E.dg], lamp: [E.y], sand: [E.br, E.k], foam: [E.w],
          palm: [E.k], sail: [E.lg, E.w], hull: [E.r], beam: [E.lg, E.y, E.w],
        }, t);
        box(px, 266, 68, 50, 16); T(px, hhmm(), 272, 72);
        px.toFb(fb);
      },
      settings(fb, { t, S, theme }) {
        const px = new Px(); world(px, t);
        box(px, 4, 4, 160, 78);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 10 + i * 11, sel = i === 0;
          if (sel) { px.rect(8, y - 2, 152, 11, E.b); T(px, '▶ ' + it, 11, y, E.y); } else T(px, it, 27, y);
        });
        box(px, 170, 4, 146, 78);
        T(px, S.items[0], 177, 10); T(px, '1/5', 306 - 24, 10, E.dg);
        button(px, 177, 24, 28, 34); button(px, 283, 24, 27, 34);
        px.poly([[196, 31], [196, 51], [184, 41]], E.k); px.poly([[290, 31], [290, 51], [302, 41]], E.k);
        px.rect(210, 30, 68, 22, E.k);
        T(px, theme, 210 + Math.floor((68 - px.tw(theme)) / 2), 37, E.y);
        for (let k = 0; k < 5; k++) px.rect(214 + k * 12, 62, 6, 4, k === 2 ? E.b : E.lg);
        T(px, S.hint, 177, 71, E.dg);
        px.toFb(fb);
      },
    };
  })();

  // ================================================================ CGA: four colors, a desert sunset
  const CGA = (() => {
    const variants = {
      a: { label: 'Palet 1: camgöbeği, magenta, beyaz', k: 0x000000, c: 0x55FFFF, m: 0xFF55FF, w: 0xFFFFFF },
      b: { label: 'Palet 0: yeşil, kırmızı, sarı', k: 0x000000, c: 0x55FF55, m: 0xFF5555, w: 0xFFFF55 },
    };
    const HZ = 52, SX = 214;
    function world(px, t, V, o = {}) {
      const { k, c, m, w } = V;
      px.dm = 'bayer';
      for (let y = 0; y < HZ; y++) for (let x = 0; x < PW; x++) px.px(x, y, px.pick([k, c, m, w], Math.pow(y / (HZ - 1), 1.3) * 0.97, x, y));
      for (let i = 0; i < 40; i++) { const x = hash(i * 7 + 3) % PW, y = hash(i * 13 + 1) % 12; if (px.get(x, y) === k && Math.sin(t / 500 + i) > -0.3) px.px(x, y, w); }
      // the sun, sliced, sinking behind the far mesas
      px.disc(SX, HZ, 19, (x, y) => (y >= 37 && (y - 37) % 4 === 3 ? undefined : w));
      if (o.wx) [[150, 22, 1], [250, 30, 0.8], [60, 16, 0.9]].forEach(([x0, y0, s], i) => {
        const cx = ((x0 + t / 260) % 400) - 40, blobs = [[0, 0, 5], [7, -3, 6], [14, 0, 5], [20, 2, 3], [-6, 2, 3]];
        for (let y = y0 - 10; y < y0 + 4; y++) for (let x = Math.floor(cx - 12 * s); x < cx + 26 * s; x++) {
          if (y > y0 + 2 || !blobs.some(([dx, dy, r]) => Math.hypot(x - cx - dx * s, y - y0 - dy * s) < r * s)) continue;
          px.px(x, y, y > y0 ? px.pick([c, w], 0.5, x, y) : y > y0 - 4 ? px.pick([c, w], 0.8, x, y) : w);
        }
      });
      // far mesas in haze
      for (const pts of [[[104, HZ], [112, 42], [140, 42], [146, HZ]], [[150, HZ], [156, 45], [178, 45], [184, HZ]], [[236, HZ], [242, 41], [262, 41], [268, HZ]]]) px.poly(pts, (x, y) => px.pick([k, c], 0.45, x, y));
      // near mesas: black, rim-lit on the sun side, with strata
      const mesa = (pts, sunRight) => {
        const inside = new Set();
        px.poly(pts, (x, y) => { inside.add(y * 1024 + x); return k; });
        for (const key of inside) {
          const x = key % 1024, y = Math.floor(key / 1024), side = sunRight ? 1 : -1;
          if (!inside.has(y * 1024 + x + side) || !inside.has(y * 1024 + x + 2 * side)) px.px(x, y, m);
          else if (!inside.has((y - 1) * 1024 + x)) px.px(x, y, m);
          else if (y % 7 === 3) px.px(x, y, px.pick([k, m], 0.3, x, y));
        }
      };
      mesa([[0, HZ + 1], [0, 29], [6, 26], [44, 26], [50, 31], [54, 40], [62, HZ + 1]], true);
      mesa([[82, HZ + 1], [88, 40], [92, 37], [116, 37], [120, 42], [126, HZ + 1]], true);
      mesa([[262, HZ + 1], [268, 30], [272, 21], [296, 21], [300, 28], [308, HZ + 1]], false);
      // desert floor glowing toward the sun, the road to it
      for (let y = HZ; y < PH; y++) for (let x = 0; x < PW; x++) {
        const rf = (y - HZ) / (PH - HZ), glow = Math.exp(-(((x - SX) / 100) ** 2));
        let col = px.pick([k, m, w], (1 - rf * 1.4) * (0.45 + 0.55 * glow), x, y);
        const hw = (y - HZ) * 2.3, d = Math.abs(x - SX);
        if (d < hw) col = d > hw - 1.5 ? c : d < Math.max(0.6, (y - HZ) * 0.09) && ((40 / (y - HZ + 1) + t / 260) % 1) < 0.5 ? w : k;
        px.px(x, y, col);
      }
      // saguaros
      const cactus = (x, base, h) => {
        const pts = new Set(), put = (a, b) => pts.add(b * 1024 + a);
        for (let y = base - h; y < base; y++) for (let i = 0; i < 3; i++) put(x + i, y);
        const ay = base - Math.round(h * 0.55), by = base - Math.round(h * 0.4);
        for (let i = 1; i < 4; i++) { put(x - i, ay); put(x - i, ay + 1); }
        for (let y = ay - Math.round(h * 0.3); y <= ay; y++) { put(x - 3, y); put(x - 4, y); }
        for (let i = 3; i < 6; i++) { put(x + i, by); put(x + i, by + 1); }
        for (let y = by - Math.round(h * 0.25); y <= by; y++) { put(x + 5, y); put(x + 6, y); }
        for (const key of pts) { const a = key % 1024, b = Math.floor(key / 1024); px.px(a, b, !pts.has(b * 1024 + a + 1) ? m : k); }
      };
      cactus(36, 70, 26); cactus(298, 74, 22); cactus(150, 60, 9); cactus(188, 58, 6);
      for (let i = 0; i < 2; i++) {
        const a = t / 1500 + i * Math.PI, bx = 92 + 16 * Math.cos(a), by = 16 + 5 * Math.sin(a), f = Math.floor(t / 220 + i) % 2;
        px.px(bx, by, k); px.px(bx - 1, by - f, k); px.px(bx + 1, by - f, k); px.px(bx - 2, by - 1, k); px.px(bx + 2, by - 1, k);
      }
    }
    const T = (px, V, s, x, y, col, o = {}) => px.text(F.cga, up(s, lang()), x, y, Object.assign({ c: col }, o));
    function box(px, V, x, y, w, h) { px.rect(x, y, w, h, V.k); px.frame(x, y, w, h, V.c); px.frame(x + 2, y + 2, w - 4, h - 4, V.c); }
    const roles = (V) => ({ BG: V.k, GHOST: { flat: mix(V.k, V.c, 0.25), dither: ['BG', 'DIM', 0.25] }, FAINT: { flat: mix(V.k, V.c, 0.125), dither: ['BG', 'DIM', 0.125] },
      HALO: V.m, LIT: V.w, DIM: V.c, HOT: V.m, HOTH: V.c, KELP: V.c });
    const big = (V) => ({ s: 5, gap: 4, c: V.w, ol: V.k, sh: { dx: 3, dy: 3, c: V.m } });
    return {
      key: 'cga', name: 'CGA', font: 'cga', time: '17:05', variants, roles,
      desc: 'Erken PC oyunu gibi: sadece 4 renk, her ara ton 4x4 sıralı dither. Dilimli güneş mesaların arkasına batıyor, ufka uzanan yol, kaktüs silüetleri, daire çizen akbabalar. Arayüz siyah kutular, çift camgöbeği çerçeve. B = aynı resim palet 0 ile.',
      paint: (px, t, V) => world(px, t, V),
      world(fb, { t, P: V }) { const px = new Px(); world(px, t, V); px.toFb(fb); },
      clock(fb, { t, P: V, S }) {
        const px = new Px(); world(px, t, V);
        px.bigTime(F.cga, hhmm(), 8, 6, big(V));
        box(px, V, 6, 56, 128, 28);
        T(px, V, S.wd + ' ' + S.date, 12, 61, V.w);
        icon(px, BELL, 12, 73, V.m); T(px, V, DEMO.alarm, 20, 72, V.m);
        px.toFb(fb);
      },
      radio(fb, { t, P: V, S }) {
        const px = new Px(); world(px, t, V);
        // cassette
        px.rect(6, 4, 162, 58, V.k); px.frame(6, 4, 162, 58, V.m); px.frame(7, 5, 160, 56, V.m);
        for (const [sx, sy] of [[10, 8], [163, 8], [10, 57], [163, 57]]) px.px(sx, sy, V.w);
        px.rect(14, 9, 146, 30, V.w);
        T(px, V, DEMO.station, 14 + Math.floor((146 - px.tw(DEMO.station)) / 2), 11, V.k);
        T(px, V, 'A', 18, 21, V.m, { s: 2 });
        T(px, V, 'C90', 134, 24, V.k);
        px.rect(44, 21, 86, 16, V.k); px.frame(44, 21, 86, 16, V.c);
        const a = t / 160;
        for (const rx of [62, 112]) {
          px.disc(rx, 29, 6, (x, y, f) => (f > 0.68 ? V.w : f < 0.3 ? V.c : undefined));
          for (let k = 0; k < 3; k++) { const an = a + k * 2.094; px.px(rx + Math.round(Math.cos(an) * 3), 29 + Math.round(Math.sin(an) * 3), V.w); }
        }
        px.rect(68, 32, 38, 2, V.m);
        px.poly([[38, 47], [136, 47], [130, 62], [44, 62]], V.k); px.line(38, 47, 44, 61, V.m); px.line(136, 47, 130, 61, V.m); px.rect(38, 47, 99, 1, V.m);
        for (const hx of [56, 74, 98, 116]) px.rect(hx, 53, 4, 4, V.c);
        dancers(px, 178, 27, t, V.k, V.c);
        box(px, V, 210, 4, 106, 58);
        const L = levels(t);
        L.forEach((lv, i) => {
          const x = 213 + i * 7, pk = peak(lv, i);
          for (let r = 0; r < 10; r++) {
            const y = 55 - r * 5, on = r < lv;
            if (on) px.rect(x, y, 6, 4, r < 6 ? V.c : r < 8 ? V.w : V.m);
            else if (r === pk - 1) px.rect(x, y, 6, 4, V.m);
            else px.fill(x, y, 6, 4, (i2, j2) => ((i2 + j2) % 4 === 0 ? V.c : V.k));
          }
        });
        GENRES.forEach((g, i) => {
          const x = 4 + i * 45, cur = i === DEMO.genre;
          px.rect(x, 66, 43, 18, cur ? V.m : V.k); px.frame(x, 66, 43, 18, V.c);
          T(px, V, g, x + Math.floor((43 - px.tw(g)) / 2), 71, V.w);
        });
        px.toFb(fb);
      },
      weather(fb, { t, P: V, S }) {
        const px = new Px(); world(px, t, V, { wx: true });
        px.bigTime(F.cga, WX.temp + '°', 8, 6, big(V));
        T(px, V, S.sky[WX.sky], 8, 45, V.w, { ol: V.k });
        box(px, V, 6, 56, 124, 28);
        T(px, V, S.hum + ' ' + WX.hum + '%', 11, 61, V.c); T(px, V, S.windL + ' ' + WX.wind + ' ' + S.kmh, 11, 72, V.c);
        box(px, V, 134, 56, 182, 28);
        WX.next.forEach(([d, tmax, sky], k) => {
          const x = 139 + k * 59;
          wicon(px, sky, x, 61, { w: V.w, g: V.c, y: V.w, b: V.c });
          T(px, V, S.days[d], x + 16, 60, V.w); T(px, V, tmax + '°', x + 16, 71, V.m);
        });
        px.toFb(fb);
      },
      scene(fb, { t, P: V }) {
        const px = new Px();
        sceneLow(px, {
          sky: [V.k, V.k, V.k, V.m], star: [V.c, V.w], moon: [V.w], land: [V.k], sea: [V.c, V.k, V.k, V.k], wave: [V.c, V.w],
          glint: [V.w], rock: [V.k, V.m], tw: [V.w], tr: [V.m], iron: [V.c], lamp: [V.w], sand: [V.m, V.k], foam: [V.w],
          palm: [V.k], sail: [V.w], hull: [V.m], beam: [V.m, V.w],
        }, t);
        T(px, V, hhmm(), 270, 74, V.w, { sh: { dx: 1, dy: 1, c: V.m } });
        px.toFb(fb);
      },
      settings(fb, { t, P: V, S, theme }) {
        const px = new Px(); world(px, t, V);
        box(px, V, 4, 4, 152, 78);
        S.items.slice(0, 6).forEach((it, i) => {
          const y = 10 + i * 12, sel = i === 0;
          if (sel) px.rect(7, y - 2, 146, 11, V.m);
          T(px, V, it, 11, y, sel ? V.w : V.c);
        });
        box(px, V, 160, 4, 156, 78);
        T(px, V, S.items[0], 166, 10, V.c); T(px, V, '1/5', 310 - 24, 10, V.c);
        px.rect(166, 24, 30, 36, V.c); px.rect(282, 24, 28, 36, V.c);
        px.poly([[187, 32], [187, 52], [174, 42]], V.k); px.poly([[289, 32], [289, 52], [302, 42]], V.k);
        T(px, V, theme, 200 + Math.floor((80 - px.tw(theme, 2)) / 2), 34, V.w, { s: 2, sh: { dx: 2, dy: 2, c: V.m } });
        for (let k = 0; k < 5; k++) px.rect(214 + k * 12, 64, 6, 4, k === 1 ? V.w : V.c);
        T(px, V, S.hint, 166, 71, V.c);
        px.toFb(fb);
      },
    };
  })();
  // CGA clock: battery in palette colors (the shared helper takes plain colors)
  CGA.clock = function (fb, { t, P: V, S }) {
    const px = new Px(); world_(px, t, V);
    px.bigTime(F.cga, hhmm(), 8, 6, { s: 5, gap: 4, c: V.w, ol: V.k, sh: { dx: 3, dy: 3, c: V.m } });
    px.rect(6, 56, 128, 28, V.k); px.frame(6, 56, 128, 28, V.c); px.frame(8, 58, 124, 24, V.c);
    px.text(F.cga, up(S.wd + ' ' + S.date, lang()), 12, 61, { c: V.w });
    icon(px, BELL, 12, 73, V.m); px.text(F.cga, DEMO.alarm, 20, 72, { c: V.m });
    battery(px, 72, 73, V.c, V.c); px.text(F.cga, DEMO.batt + '%', 86, 72, { c: V.c });
    px.toFb(fb);
  };
  const world_ = (px, t, V) => CGA.paint(px, t, V);

  G.PHT.THEMES.push(CGA, EGA, C64, AMIGA);
})(typeof window !== 'undefined' ? window : globalThis);
