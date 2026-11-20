// Review page: every theme's screens on canvases, variant switch, palette roles, font sheet and a picks summary.
(function () {
  const { PH, PHT } = window;
  const LABEL = { clock: 'Saat', alarm: 'Alarm çalıyor', radio: 'Radyo', scene: 'Sahne: deniz feneri', settings: 'Ayarlar' };
  const state = { lang: 'en', anim: true, variant: {}, note: {} };
  try { Object.assign(state, JSON.parse(localStorage.getItem('phosphor-themes') || '{}')); } catch (e) { /* private mode */ }
  const save = () => { try { localStorage.setItem('phosphor-themes', JSON.stringify({ variant: state.variant, note: state.note })); } catch (e) { /* ignore */ } };
  const items = [];
  const $ = (tag, attrs = {}, kids = []) => {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) if (k === 'text') el.textContent = v; else el.setAttribute(k, v);
    for (const k of [].concat(kids)) if (k) el.append(k);
    return el;
  };
  const hex = (c) => PH.toHex(PH.from565(PH.to565(c)));
  const c565 = (c) => '0x' + PH.to565(c).toString(16).toUpperCase().padStart(4, '0');

  function screenCanvas(theme, screen, kind = 'screen') {
    const cv = $('canvas', { class: 'screen', width: 640, height: 172, role: 'img', 'aria-label': `${theme} ${screen}` });
    const item = { cv, ctx: cv.getContext('2d'), img: null, theme, screen, kind, last: -1e9, visible: true, fb: new PH.Fb() };
    item.img = item.ctx.createImageData(640, 172);
    items.push(item);
    return cv;
  }
  function draw(item, t) {
    const v = state.variant[item.theme] || 'a';
    if (item.kind === 'font') PHT.specimen(item.fb, item.theme, v);
    else PHT.render(item.fb, item.theme, v, item.screen, t, state.lang);
    item.fb.rgba(item.img.data);
    item.ctx.putImageData(item.img, 0, 0);
    item.last = t;
  }
  function roles(el, th) {
    el.textContent = '';
    const V = th.variants[state.variant[th.key] || 'a'];
    for (const [name, r] of Object.entries(th.roles(V))) {
      const sw = $('div', { class: 'sw' });
      let sub;
      if (typeof r === 'object') {
        const R = th.roles(V), a = PHT.flat(R[r.dither[0]]), b = PHT.flat(R[r.dither[1]]);
        const cv = $('canvas', { width: 8, height: 8, class: 'sw' });
        const cx = cv.getContext('2d');
        for (let y = 0; y < 8; y++) for (let x = 0; x < 8; x++) { cx.fillStyle = hex(PH.bayer(x, y) < r.dither[2] ? b : a); cx.fillRect(x, y, 1, 1); }
        el.append($('div', { class: 'role' }, [cv, $('b', { text: name }), $('small', { text: `dither ${Math.round(r.dither[2] * 100)}% · ${hex(r.flat)}` })]));
        continue;
      }
      sw.style.background = hex(r);
      sub = `${hex(r)} · ${c565(r)}`;
      el.append($('div', { class: 'role' }, [sw, $('b', { text: name }), $('small', { text: sub })]));
    }
  }

  const nav = document.getElementById('nav'), root = document.getElementById('themes'), pickrows = document.getElementById('pickrows');
  const roleEls = {};
  for (const th of PHT.THEMES) {
    const name = PHT.THEME_NAMES[PHT.THEMES.indexOf(th)];
    nav.append($('a', { href: '#t-' + th.key, text: name }));
    const sec = $('section', { class: 'theme', id: 't-' + th.key });
    const head = $('div', { class: 'thead' }, [$('h2', { text: name })]);
    const keys = Object.keys(th.variants);
    if (keys.length > 1) {
      const seg = $('div', { class: 'seg', role: 'group', 'aria-label': name + ' varyant' });
      for (const k of keys) {
        const b = $('button', { 'data-v': k, 'aria-pressed': String((state.variant[th.key] || 'a') === k), text: `${k.toUpperCase()} · ${th.variants[k].label}` });
        b.onclick = () => {
          state.variant[th.key] = k; save();
          seg.querySelectorAll('button').forEach((x) => x.setAttribute('aria-pressed', String(x === b)));
          roles(roleEls[th.key], th); items.filter((i) => i.theme === th.key).forEach((i) => { i.last = -1e9; }); summary();
        };
        seg.append(b);
      }
      head.append(seg);
    }
    sec.append(head, $('p', { class: 'desc', text: th.desc }));
    for (const sc of PHT.SCREENS) if (th[sc]) sec.append($('figure', {}, [$('figcaption', {}, [$('span', { text: LABEL[sc] })]), screenCanvas(th.key, sc)]));
    sec.append($('figure', {}, [$('figcaption', {}, [$('span', { text: 'Font + Türkçe harfler' })]), screenCanvas(th.key, 'font', 'font')]));
    const re = $('div', { class: 'roles' }); roleEls[th.key] = re; roles(re, th);
    sec.append($('figcaption', {}, [$('span', { text: 'Firmware rolleri (palettes.json)' })]), re);
    root.append(sec);
    // picks row
    const pick = $('div', { class: 'pick' }, [$('strong', { text: name })]);
    const sel = $('div', { class: 'seg' });
    for (const k of keys) {
      const b = $('button', { 'aria-pressed': String((state.variant[th.key] || 'a') === k), text: k.toUpperCase() });
      b.onclick = () => sec.querySelector(`.thead button[data-v="${k}"]`)?.click() || (state.variant[th.key] = k);
      sel.append(b);
    }
    const inp = $('input', { type: 'text', placeholder: 'not (ör. kenarlık daha ince, sarı yerine beyaz)', 'aria-label': name + ' not' });
    inp.value = state.note[th.key] || '';
    inp.oninput = () => { state.note[th.key] = inp.value; save(); summary(); };
    pick.append(sel, inp);
    pickrows.append(pick);
  }
  function summary() {
    document.querySelectorAll('.pick').forEach((row, i) => {
      const th = PHT.THEMES[i], v = state.variant[th.key] || 'a';
      row.querySelectorAll('.seg button').forEach((b) => b.setAttribute('aria-pressed', String(b.textContent.toLowerCase() === v)));
    });
    document.getElementById('out').value = PHT.THEMES.map((th, i) => {
      const v = state.variant[th.key] || 'a', n = state.note[th.key];
      return `${PHT.THEME_NAMES[i]}: ${v.toUpperCase()} (${th.variants[v].label})${n ? ' · not: ' + n : ''}`;
    }).join('\n');
  }
  summary();
  document.getElementById('copy').onclick = async () => {
    const out = document.getElementById('out');
    try { await navigator.clipboard.writeText(out.value); document.getElementById('copy').textContent = 'Kopyalandı'; } catch (e) { out.select(); document.execCommand('copy'); }
  };
  for (const [id, key] of [['lang', 'lang'], ['anim', 'anim']]) {
    const seg = document.getElementById(id);
    seg.querySelectorAll('button').forEach((b) => b.onclick = () => {
      state[key] = key === 'anim' ? b.dataset.v === '1' : b.dataset.v;
      seg.querySelectorAll('button').forEach((x) => x.setAttribute('aria-pressed', String(x === b)));
      items.forEach((i) => { i.last = -1e9; });
    });
  }
  if (matchMedia('(prefers-reduced-motion: reduce)').matches) { state.anim = false; document.querySelector('#anim [data-v="0"]').click(); }
  const io = new IntersectionObserver((es) => es.forEach((e) => { const it = items.find((i) => i.cv === e.target); if (it) it.visible = e.isIntersecting; }), { rootMargin: '200px' });
  items.forEach((i) => { io.observe(i.cv); draw(i, 0); });
  const t0 = performance.now();
  let frozen = 0;
  (function loop() {
    const now = performance.now() - t0, t = state.anim ? now : frozen;
    if (state.anim) frozen = now;
    for (const it of items) {
      if (!it.visible) continue;
      const every = it.kind === 'font' ? 1e9 : it.screen === 'scene' ? 100 : 60;
      if ((state.anim && now - it.last >= every) || it.last < -1e8) draw(it, t);
    }
    requestAnimationFrame(loop);
  })();
})();
