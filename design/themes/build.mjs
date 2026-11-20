// Builds the theme mockups from src/: png/ (every screen at 640x172 and 2x), palettes.json, fonts.json and the
// self-contained mockups.html. Run: node design/themes/build.mjs
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import zlib from 'node:zlib';
import { fileURLToPath } from 'node:url';

const DIR = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(DIR, '../..');
const SRC = ['fonts.js', 'engine.js', 'themes.js'].map((f) => fs.readFileSync(path.join(DIR, 'src', f), 'utf8'));
const UI = fs.readFileSync(path.join(DIR, 'src', 'ui.js'), 'utf8');

// the VFD theme draws with the firmware's own 5x7 font
const vfdH = fs.readFileSync(path.join(ROOT, 'firmware/src/boards/touch349/gadget/VfdFont.h'), 'utf8');
const VFD_GLYPHS = {};
for (const m of vfdH.matchAll(/\{\s*0x([0-9A-Fa-f]+),\s*\{([^}]*)\}\s*\}/g)) VFD_GLYPHS[parseInt(m[1], 16)] = m[2].split(',').map((v) => parseInt(v, 16));

const ctx = { VFD_GLYPHS, console };
ctx.globalThis = ctx;
vm.createContext(ctx);
for (const s of SRC) vm.runInContext(s, ctx);
const { PH, PHT, FONTS, FONT_BASE } = ctx;

// ---- PNG
const CRC = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc32 = (buf) => { let c = 0xffffffff; for (const b of buf) c = CRC[(c ^ b) & 255] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
function png(w, h, rgba) {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
    const i = (y * w + x) * 4, o = y * (w * 3 + 1) + 1 + x * 3;
    raw[o] = rgba[i]; raw[o + 1] = rgba[i + 1]; raw[o + 2] = rgba[i + 2];
  }
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}
const scale2 = (rgba, w, h) => {
  const out = new Uint8Array(w * h * 16);
  for (let y = 0; y < h * 2; y++) for (let x = 0; x < w * 2; x++) { const s = ((y >> 1) * w + (x >> 1)) * 4, d = (y * w * 2 + x) * 4; out.set(rgba.subarray(s, s + 4), d); }
  return out;
};

const PNG_DIR = path.join(DIR, 'png');
fs.rmSync(PNG_DIR, { recursive: true, force: true });
fs.mkdirSync(path.join(PNG_DIR, '2x'), { recursive: true });
const written = [];
function save(name, fb) {
  const rgba = fb.rgba(new Uint8Array(640 * 172 * 4));
  fs.writeFileSync(path.join(PNG_DIR, name + '.png'), png(640, 172, rgba));
  fs.writeFileSync(path.join(PNG_DIR, '2x', name + '.png'), png(1280, 344, scale2(rgba, 640, 172)));
  written.push(name);
}
// missing glyphs show up as '?': report them
const missing = new Set();
for (const f of Object.values(FONTS)) {
  const orig = f.glyphs;
  f.glyphs = new Proxy(orig, { get: (o, k) => { if (typeof k === 'string' && !(k in o) && k.length <= 2) missing.add(`${f.name}:${k}`); return o[k]; } });
}

for (const th of PHT.THEMES) {
  for (const v of Object.keys(th.variants)) {
    for (const sc of PHT.SCREENS) {
      if (!th[sc]) continue;
      save(`${th.key}${v === 'a' ? '' : '-' + v}-${sc}`, PHT.render(new PH.Fb(), th.key, v, sc, 0, 'en'));
    }
    save(`${th.key}${v === 'a' ? '' : '-' + v}-settings-tr`, PHT.render(new PH.Fb(), th.key, v, 'settings', 0, 'tr'));
    save(`${th.key}${v === 'a' ? '' : '-' + v}-font`, PHT.specimen(new PH.Fb(), th.key, v));
  }
}
for (const f of Object.values(FONTS)) f.glyphs = Object.assign({}, f.glyphs);

// ---- palettes.json
const hx = (c) => PH.toHex(PH.from565(PH.to565(c)));
const role = (r) => {
  if (typeof r === 'object') return { hex: hx(r.flat), rgb565: '0x' + PH.to565(r.flat).toString(16).toUpperCase().padStart(4, '0'), dither: { a: r.dither[0], b: r.dither[1], level: r.dither[2] }, note: 'drawn as an ordered dither of a and b; hex is the flat fallback' };
  return { hex: hx(r), rgb565: '0x' + PH.to565(r).toString(16).toUpperCase().padStart(4, '0') };
};
const ROLE_NOTES = {
  BG: 'screen background', GHOST: 'unlit segments and dots, inactive labels', FAINT: 'faintest marks (unlit sky matrix, tracks)',
  HALO: 'glow around lit (VFD); edge, frame and shadow color in the flat themes', LIT: 'main ink', DIM: 'secondary text',
  HOT: 'accent', HOTH: 'glow around the accent; second accent in the flat themes', KELP: 'battery and kelp green',
};
const extraColors = (th, V) => {
  const out = {};
  for (const [k, c] of Object.entries(V)) if (typeof c === 'number' && k !== 'wb') out[k] = role(c);
  return out;
};
const palettes = { generated: 'design/themes/build.mjs', roleNotes: ROLE_NOTES, themes: {} };
for (const th of PHT.THEMES) {
  const t = { name: th.name, font: th.font ? `fonts.json: ${th.font}` : 'firmware VfdFont.h (5x7 dot matrix)', default: 'a', variants: {} };
  if (th.border) t.border = { ...th.border, color: 'HALO role (C64 border)' };
  for (const [k, V] of Object.entries(th.variants)) {
    const R = th.roles(V);
    t.variants[k] = { label: V.label, roles: Object.fromEntries(Object.entries(R).map(([n, c]) => [n, role(c)])), chrome: extraColors(th, V) };
  }
  palettes.themes[th.key] = t;
}
palettes.themes.amiga.copper = {
  digits: ['#F60', '#F70', '#F80', '#F90', '#FA0', '#FB2', '#FC4', '#FD6', '#FE8', '#FFA', '#FFC', '#FFE', '#FFF', '#EFF', '#DEF', '#BDF', '#ACF', '#9BF', '#8AF', '#79E', '#68D'],
  vu: ['#05A', '#06B', '#08C', '#0AD', '#2CE', '#6DE', '#AEE', '#FFF', '#FD8', '#F80'],
  sky: ['#001', '#002', '#012', '#013', '#023', '#024', '#134', '#135', '#245', '#356', '#467', '#578', '#879', '#A89'],
  note: '12-bit colors, one per scanline band (copper list); expand #RGB to #RRGGBB',
};
palettes.themes.c64.machine16 = { black: '#000000', white: '#FFFFFF', red: '#68372B', cyan: '#70A4B2', purple: '#6F3D86', green: '#588D43',
  blue: '#352879', yellow: '#B8C76F', orange: '#6F4F25', brown: '#433900', lightRed: '#9A6759', darkGrey: '#444444', grey: '#6C6C6C',
  lightGreen: '#9AD284', lightBlue: '#6C5EB5', lightGrey: '#959595' };
palettes.themes.ega.machine16 = { black: '#000000', blue: '#0000AA', green: '#00AA00', cyan: '#00AAAA', red: '#AA0000', magenta: '#AA00AA',
  brown: '#AA5500', lightGrey: '#AAAAAA', darkGrey: '#555555', lightBlue: '#5555FF', lightGreen: '#55FF55', lightCyan: '#55FFFF',
  lightRed: '#FF5555', lightMagenta: '#FF55FF', yellow: '#FFFF55', white: '#FFFFFF' };
fs.writeFileSync(path.join(DIR, 'palettes.json'), JSON.stringify(palettes, null, 2) + '\n');

// ---- fonts.json
const fonts = { generated: 'design/themes/build.mjs', format: '8 rows per glyph, top first; bit 7 = leftmost column. Capitals and digits sit on rows 0-6 (baseline row 6), row 7 holds descenders and cedillas.', fonts: {} };
for (const [key, f] of Object.entries(FONTS)) {
  const glyphs = {};
  for (const [ch, rows] of Object.entries(f.glyphs)) glyphs[ch] = { cp: '0x' + ch.codePointAt(0).toString(16).toUpperCase().padStart(4, '0'), rows: Array.from(rows).map((r) => '0x' + r.toString(16).toUpperCase().padStart(2, '0')) };
  fonts.fonts[key] = { name: f.name, theme: key, width: 8, height: 8, advance: 8, upperCaseOnly: f.upper, bold: f.bold, square: f.square, glyphCount: Object.keys(glyphs).length, glyphs };
}
fs.writeFileSync(path.join(DIR, 'fonts.json'), JSON.stringify(fonts, null, 2) + '\n');

// ---- mockups.html
const tpl = fs.readFileSync(path.join(DIR, 'src', 'page.html'), 'utf8');
const script = `window.VFD_GLYPHS=${JSON.stringify(VFD_GLYPHS)};\n` + SRC.join('\n') + '\n' + UI;
fs.writeFileSync(path.join(DIR, 'mockups.html'), tpl.replace('/*SCRIPT*/', () => script.replace(/<\/script/g, '<\\/script')));

console.log(`${written.length} screens -> png/ (+2x), palettes.json, fonts.json, mockups.html`);
if (missing.size) console.log('missing glyphs:', [...missing].join(' '));
