import random, statistics as st
G = {'A': dict(t0=650, dh=30, dw=0.9, tmin=320, p0=0.38, ph=0.02, nmax=(3,4)),
     'B': dict(t0=520, dh=25, dw=0.7, tmin=260, p0=0.48, ph=0.02, nmax=(4,5))}
POS = 5  # spawn at 5, danger at 1, resolve when stepping to 0
def tick_ms(s, g):
    c = G[g]; v = s % 1000; h, w = v // 100, v % 100
    if s >= 1000: h = max(h, 5)
    return max(c['tmin'], c['t0'] - c['dh'] * h - c['dw'] * w)
def play(g, rng, human=True, sw_mu=280, sw_sd=70, lapse=0.0015, load=0.004, F=380, max_ticks=40000):
    c = G[g]; ships = []  # [lane,pos]
    score = miss = 0; lane = 1; last_res_t = -1e9; last_spawn_lane = None
    t = 0.0; sim = 0; adj_diff = 0; worst_gap = 1e9; onscreen = []
    for k in range(max_ticks):
        T = tick_ms(score, g); t += T
        # move + resolve
        res = []
        for sh in ships: sh[1] -= 1
        for sh in [s for s in ships if s[1] == 0]:
            res.append(sh[0]); ships.remove(sh)
        if len(res) > 1: sim += 1
        for L in res:
            avail = t - last_res_t
            if L != lane:
                worst_gap = min(worst_gap, avail)
                need = rng.gauss(sw_mu, sw_sd) if human else 0
                ok = need <= avail
            else:
                ok = True
            if human and rng.random() < lapse + load*max(0,len(ships)): ok = False
            if ok: lane = L; score += 1
            else: miss += 1
            last_res_t = t
            if score in (300, 700) : miss = 0
        if miss >= 3: return score, sim, worst_gap, st.mean(onscreen) if onscreen else 0, t/60000
        # spawn: one per tick; lane needs pos5,pos4 free; after a spawn, next tick only same lane
        nmax = c['nmax'][1] if score % 1000 >= 500 else c['nmax'][0]
        p = c['p0'] + c['ph'] * ((score % 1000) // 100)
        if len(ships) < nmax and rng.random() < p:
            free = [L for L in range(3) if not any(s[0] == L and s[1] >= POS - 1 for s in ships)]
            if last_spawn_lane is not None and T < F + 20: free = [L for L in free if L == last_spawn_lane]
            if free:
                L = rng.choice(free); ships.append([L, POS + 1]); last_spawn_lane = L
            else: last_spawn_lane = None
        else: last_spawn_lane = None
        onscreen.append(len(ships))
    return score, sim, worst_gap, st.mean(onscreen), t/60000
rng = random.Random(7)
for g in 'AB':
    # fairness: perfect bot, 300 runs to 999+
    sims = 0; gaps = []
    for _ in range(300):
        s, sim, gap, on, mins = play(g, rng, human=False, max_ticks=3500)
        sims += sim; gaps.append(gap)
    print(f"Game {g}: simultaneous resolutions={sims}, min diff-lane gap={min(gaps):.0f} ms, tick at 0/500/999: {tick_ms(0,g):.0f}/{tick_ms(500,g):.0f}/{tick_ms(999,g):.0f}")
    res = [play(g, rng) for _ in range(2000)]
    sc = sorted(r[0] for r in res); mins = [r[4] for r in res]; on = st.mean(r[3] for r in res)
    q = lambda f: sc[int(f*len(sc))]
    print(f"  human model: score p25/median/p75/p95 = {q(.25)}/{q(.5)}/{q(.75)}/{q(.95)}, median game {st.median(mins):.1f} min, avg ships on screen {on:.2f}")
