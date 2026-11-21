#pragma once

// Rules of the endless runner (GameRunner.cpp draws and handles touch; this file is the game): physics, the obstacle
// generator and its spacing rule. No Arduino, no drawing, no clock of its own beyond the time it is stepped with, so the
// host test (firmware/test/runner_fair_test.cpp) runs exactly the code the device runs.
//
// World units: sprite cells. Positions are world x in cells, heights are cells above the ground line.

#include <cmath>
#include <cstdint>

namespace runner {

constexpr float G = 220.0f, V0 = 82.0f, V_CUT = 46.0f, V_FAST = 120.0f;  // gravity, take-off, cut-short climb, fast fall (duck in the air)
constexpr float MIN_HOLD_S = 0.06f, MIN_DUCK_S = 0.25f, BUFFER_S = 0.12f;
constexpr float SPEED0 = 62.0f, SPEED_GAIN = 0.6f, SPEED_MAX = 104.0f;  // cells/s, + per second of running
constexpr float M_PER_CELL = 0.2f;                                      // score (metres) per cell
constexpr float ACT_D = 104.0f;  // a moving enemy wakes when it is this far ahead (inside the narrowest screen, the C64's)
constexpr float RUN_X0 = 1.5f, RUN_X1 = 7.5f, RUN_BOT = 1.0f, R_TOP = 11.0f, R_DUCK_TOP = 5.5f;  // hit box of the runner, from its origin
constexpr float FOOT_X0 = 3.0f, FOOT_X1 = 6.5f;                                              // its feet, for pits
constexpr float RUN_CX = 4.5f;                                                          // centre of the hit box
constexpr float DRAW_S = 0.6f, ARROW_V = 80.0f, FLEE_ADD = 40.0f;                    // archer: bow draw time, arrow speed in the world
constexpr float HOP_T = 0.85f, HOP_H = 6.0f;                                         // hopping bug
constexpr int MAX_OBS = 16;

enum Type : uint8_t { PUDDLE, CRATE, TALL, BIRD, BUG, ARCHER, BEAM, SWARM, PIT, HOPPER, WASP, LEDGE, ARROW, TYPES };

enum Event : uint32_t { EV_JUMP = 1, EV_DUCK = 2, EV_DIE = 4, EV_MILE = 8, EV_ARROW = 16, EV_ZONE = 32 };

struct Dim {
  float w, hb, ht, inset, v;  // sprite width, hit box bottom and top, hit box narrower by inset each side, speed in the world (leftwards) once awake
};
const Dim DIMS[TYPES] = {
  { 14, 0, 1.2f, 0.5f, 0 },     // PUDDLE   jump
  { 8, 0, 5.5f, 0.5f, 0 },      // CRATE    jump
  { 8, 0, 12.5f, 0.5f, 0 },     // TALL     jump, held
  { 11, 2.0f, 6.5f, 2.5f, 0 },  // BIRD     jump (sprite bottom 1.5)
  { 8, 0, 3.5f, 1, 18 },        // BUG      jump
  { 13, 0, 0, 0, 0 },           // ARCHER   (not solid) shoots ARROWs: head height = duck, foot height = jump
  { 30, 7.5f, 99, 1, 0 },       // BEAM     duck
  { 14, 7.0f, 12.5f, 1.5f, 20 },  // SWARM  duck
  { 24, 0, 0, 0, 0 },           // PIT      jump (width varies, ground missing)
  { 8, 0, 4.0f, 1, 14 },        // HOPPER   jump held (or duck under it at the top of a hop)
  { 9, 0, 4.5f, 1, 28 },        // WASP     duck (dives along a visible path)
  { 26, 0, 4.5f, 0, 0 },        // LEDGE    jump onto / over; the ground is 4.5 higher on it
  { 10, 0, 1.6f, 1, 0 },        // ARROW
};

struct Ob {
  float x = 0, w = 0, y = 0, t = 0;  // world left edge, width, bottom height now, seconds awake
  uint8_t type = 0, var = 0;         // ARCHER/ARROW: 0 = head height, 1 = foot height
  uint32_t id = 0;                   // spawn number (an arrow carries its archer's)
  bool alive = false, active = false, fired = false;
};

struct Pick {
  uint8_t type, var;
  bool tight;  // the gap before it is the bare minimum (a combo)
  Pick(uint8_t t = CRATE, uint8_t v = 0, bool ti = false) : type(t), var(v), tight(ti) {}
};

inline float rmin(float a, float b) { return a < b ? a : b; }
inline float rmax(float a, float b) { return a > b ? a : b; }
inline float rclamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

inline bool isDuck(const Pick &p) { return p.type == BEAM || p.type == SWARM || p.type == WASP || (p.type == ARCHER && p.var == 0); }

// Difficulty by distance (metres of score): unlock list and how tight the gaps get. Every gap is the minimum a player can
// make (see baseGap) plus slack that shrinks to the end of the ramp.
constexpr float RAMP_M = 1500.0f;
inline float slackS(float m) { return 0.50f + (0.10f - 0.50f) * rclamp(m / RAMP_M, 0, 1); }
inline float randS(float m) { return 0.90f + (0.30f - 0.90f) * rclamp(m / RAMP_M, 0, 1); }

struct Spec {
  uint8_t type, var;
  uint16_t unlock;  // metres
  uint8_t weight;
};
const Spec SPECS[] = {
  { PUDDLE, 0, 0, 20 },  { CRATE, 0, 0, 30 },  { BIRD, 0, 150, 14 },   { BUG, 0, 150, 16 },    { TALL, 0, 200, 10 },
  { ARCHER, 0, 300, 9 }, { ARCHER, 1, 300, 9 }, { BEAM, 0, 450, 14 },   { SWARM, 0, 450, 12 },
  { PIT, 0, 600, 14 },   { HOPPER, 0, 600, 10 }, { LEDGE, 0, 650, 8 },  { WASP, 0, 800, 12 },
};
constexpr int NSPECS = sizeof(SPECS) / sizeof(SPECS[0]);
constexpr float COMBO_FROM_M = 800.0f;
const Pick COMBOS[][2] = {
  { { CRATE, 0, false }, { BEAM, 0, true } },   { { BEAM, 0, false }, { CRATE, 0, true } },   { { PUDDLE, 0, false }, { SWARM, 0, true } },
  { { BUG, 0, false }, { BEAM, 0, true } },     { { PIT, 0, false }, { BEAM, 0, true } },     { { BEAM, 0, false }, { PIT, 0, true } },
  { { BIRD, 0, false }, { SWARM, 0, true } },   { { ARCHER, 0, false }, { CRATE, 0, true } }, { { WASP, 0, false }, { PUDDLE, 0, true } },
  { { TALL, 0, false }, { BEAM, 0, true } },    { { CRATE, 0, false }, { CRATE, 0, true } },
};
constexpr int NCOMBOS = sizeof(COMBOS) / sizeof(COMBOS[0]);

// The least time (s) between the end of A's hit span and the start of B's that a player can make: A's landing (a short
// hop lands 0.29 s after the middle of a low obstacle), the finger moving between the two halves of the screen, and B's
// take-off or duck. Jump to jump is the slowest.
inline float baseGap(const Pick &a, const Pick &b)
{
  bool da = isDuck(a), db = isDuck(b);
  float g = da ? (db ? 0.32f : 0.40f) : (db ? 0.36f : 0.50f);
  switch (a.type) {
    case TALL: g += 0.10f; break;    // a high jump lands later
    case HOPPER: g += 0.12f; break;  // the same
    case LEDGE: g += 0.22f; break;   // dropping off the back of it
    case PIT: g += 0.05f; break;
    default: break;
  }
  return g;
}

struct Sim {
  Ob obs[MAX_OBS];
  float dist = 0, speed = SPEED0, run_s = 0, y = 0, vy = 0, next_x = 0, clk = 0, view = 128;
  float jump_t = 0, cut_at = 0, buf_until = 0, duck_min_until = 0;
  bool on_ground = true, ducking = false, cut_pending = false, buffered = false, dead = false, fell = false;
  uint8_t finger = 0;  // 0 none, 1 upper half, 2 lower half
  uint32_t rng = 1, ev = 0, last_mile = 0, serial = 0;
  int last_zone = 0;
  Pick nxt, q[4], last1, last2;
  int qn = 0;

  float rnd()
  {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return (rng & 0xFFFF) / 65536.0f;
  }
  float score() const { return dist * M_PER_CELL; }
  static int zoneOf(float m) { return ((int)(m / 400.0f)) & 3; }

  void reset(uint32_t seed, float view_cells)
  {
    for (Ob &o : obs) o = Ob();
    dist = 0, speed = SPEED0, run_s = 0, y = 0, vy = 0, clk = 0, view = view_cells;
    jump_t = cut_at = buf_until = duck_min_until = 0;
    on_ground = true, ducking = false, cut_pending = false, buffered = false, dead = false, fell = false;
    finger = 0, ev = 0, last_mile = 0, last_zone = 0, qn = 0, serial = 0;
    rng = seed ? seed : 1;
    for (int i = 0; i < 8; i++) rnd();
    nxt = Pick();  // a crate first
    last1 = last2 = Pick();
    next_x = 90;
  }

  // ----- input (finger edges) ---------------------------------------------------------------------------------------
  void startJump()
  {
    vy = V0, on_ground = false, jump_t = clk, cut_pending = false, buffered = false, ducking = false;
    ev |= EV_JUMP;
  }
  void touchTop()  // a finger lands on the upper half: jump now (or on landing if it is about to land)
  {
    finger = 1;
    ducking = false;
    if (dead) return;
    if (on_ground) startJump();
    else buffered = true, buf_until = clk + BUFFER_S;
  }
  void touchBottom()  // lower half: duck for as long as the finger stays, at least MIN_DUCK_S; in the air it drops fast
  {
    finger = 2;
    if (dead) return;
    ducking = true, duck_min_until = clk + MIN_DUCK_S, buffered = false;
    if (!on_ground && vy > -V_FAST) vy = -V_FAST;
    ev |= EV_DUCK;
  }
  void release()
  {
    if (finger == 1 && !on_ground && vy > V_CUT) cut_pending = true, cut_at = rmax(clk, jump_t + MIN_HOLD_S);  // press length = height
    finger = 0;
  }

  // ----- generator --------------------------------------------------------------------------------------------------
  Pick pickOne()
  {
    float m = score();
    for (int tries = 0; tries < 6; tries++) {
      float tot = 0;
      float wt[NSPECS];
      for (int i = 0; i < NSPECS; i++) {
        wt[i] = 0;
        if (SPECS[i].unlock > m) continue;
        wt[i] = SPECS[i].weight * (m - SPECS[i].unlock < 120 ? 3.0f : 1.0f);  // a new enemy shows up soon after it unlocks
        tot += wt[i];
      }
      float r = rnd() * tot;
      int k = 0;
      for (; k < NSPECS - 1; k++) {
        if (r < wt[k]) break;
        r -= wt[k];
      }
      while (wt[k] == 0 && k > 0) k--;
      Pick p;
      p.type = SPECS[k].type, p.var = SPECS[k].var;
      if (p.type == last1.type && p.type == last2.type) continue;
      if ((p.type == ARCHER || p.type == PIT) && p.type == last1.type) continue;
      return p;
    }
    return Pick();
  }

  Pick takeNext()
  {
    Pick p;
    if (qn > 0) {
      p = q[0];
      for (int i = 1; i < qn; i++) q[i - 1] = q[i];
      qn--;
    } else if (score() >= COMBO_FROM_M && rnd() < 0.25f) {
      int c = (int)(rnd() * NCOMBOS) % NCOMBOS;
      p = COMBOS[c][0];
      q[0] = COMBOS[c][1], qn = 1;
    } else {
      p = pickOne();
    }
    last2 = last1, last1 = p;
    return p;
  }

  // Puts `p` with its hit span starting at world x = left; returns where the span ends.
  float place(const Pick &p, float left)
  {
    Ob *o = nullptr;
    for (Ob &c : obs) {
      if (!c.alive) {
        o = &c;
        break;
      }
    }
    if (!o) return left;
    const Dim &d = DIMS[p.type];
    float S = speed;
    *o = Ob();
    o->alive = true, o->type = p.type, o->var = p.var, o->w = d.w, o->id = ++serial;
    float span = d.w - 2 * d.inset, end = left + span;
    switch (p.type) {
      case PIT: {
        float wmax = rmin(S * 0.28f, 32.0f);
        o->w = span = 14.0f + rnd() * rmax(wmax - 12.0f, 2.0f);
        o->x = left, end = left + span;
        break;
      }
      case BUG: case SWARM: case HOPPER: case WASP: {  // asleep until ACT_D ahead, then it comes: placed so it meets the runner at the span
        float M = left + span * 0.5f, T = (ACT_D + d.w * 0.5f - 3.0f) / (S + d.v);
        o->x = M + ACT_D - 3.0f - S * T;
        if (p.type == HOPPER) o->var = (uint8_t)(rnd() * 10.0f);
        if (p.type == WASP) o->y = waspY(o->x - (dist + RUN_X0));
        break;
      }
      case ARCHER: {  // placed so the arrow meets the runner at [left, left + 8]
        float M = left + 4.0f, tau = (ACT_D - DRAW_S * S) / (S + ARROW_V);
        o->x = M + ACT_D - 3.0f - S * (DRAW_S + tau);
        end = left + 8.0f;
        if (o->x + 15.0f > next_x) next_x = o->x + 15.0f;  // keeps the next thing from standing on the archer
        break;
      }
      default: o->x = left - d.inset;
    }
    return end;
  }

  void spawnOne()
  {
    Pick a = nxt;
    float end = place(a, next_x);
    nxt = takeNext();
    float m = score();
    float t = baseGap(a, nxt) + (nxt.tight ? 0.0f : slackS(m) + randS(m) * rnd());
    float nx = end + speed * 1.05f * t;
    if (a.type == ARCHER) {  // never closer than the archer's own standing room
      for (const Ob &o : obs) {
        if (o.alive && o.type == ARCHER && !o.fired && o.x + 15.0f > nx) nx = o.x + 15.0f;
      }
    }
    next_x = nx;
  }

  // ----- world ------------------------------------------------------------------------------------------------------
  static float waspY(float dxr)  // height of a wasp dx cells ahead of the runner: it dives to head height along this curve
  {
    float u = rclamp((dxr - 8.0f) / 72.0f, 0, 1);
    return 7.8f + 16.2f * u * u;
  }

  static bool box(const Ob &o, float &x0, float &x1, float &b, float &t)
  {
    if (!o.alive || o.type == ARCHER || o.type == PIT) return false;
    const Dim &d = DIMS[o.type];
    b = d.hb, t = d.ht;
    if (o.type == ARROW) b = o.y + 0.7f, t = o.y + 2.3f;
    if (o.type == HOPPER || o.type == WASP) b = o.y, t = o.y + d.ht;
    x0 = o.x + d.inset, x1 = o.x + o.w - d.inset;
    return true;
  }

  float support() const  // ground height under the runner: the top of a ledge it overlaps, else 0
  {
    float h = 0;
    for (const Ob &o : obs) {
      if (o.alive && o.type == LEDGE && dist + RUN_X1 > o.x && dist + RUN_X0 < o.x + o.w) h = rmax(h, DIMS[LEDGE].ht);
    }
    return h;
  }

  bool overPit() const  // the whole of the feet over a pit
  {
    for (const Ob &o : obs) {
      if (o.alive && o.type == PIT && dist + FOOT_X0 > o.x && dist + FOOT_X1 < o.x + o.w) return true;
    }
    return false;
  }

  bool hits() const
  {
    float rb = y + RUN_BOT, rt = y + (ducking ? R_DUCK_TOP : R_TOP), rx0 = dist + RUN_X0, rx1 = dist + RUN_X1;
    for (const Ob &o : obs) {
      float x0, x1, b, t;
      if (box(o, x0, x1, b, t) && rx1 > x0 && rx0 < x1 && rb < t && rt > b) return true;
    }
    return false;
  }

  void die(bool fall)
  {
    dead = true, fell = fall;
    ev |= EV_DIE;
  }

  void updateObs(float dt)
  {
    for (Ob &o : obs) {
      if (!o.alive) continue;
      const Dim &d = DIMS[o.type];
      float ahead = o.x - (dist + RUN_X0);
      switch (o.type) {
        case BUG: case SWARM: case HOPPER: case WASP:
          if (!o.active && ahead <= ACT_D) o.active = true, o.t = 0;
          if (o.active) {
            o.t += dt, o.x -= d.v * dt;
            if (o.type == HOPPER) {
              float u = fmodf(o.t + o.var * 0.1f, HOP_T) / HOP_T;
              o.y = 4.0f * HOP_H * u * (1.0f - u);
            }
          }
          if (o.type == WASP) o.y = waspY(o.x - (dist + RUN_X0));
          break;
        case ARCHER:
          if (!o.active && ahead <= ACT_D) o.active = true, o.t = 0;
          if (o.active) {
            o.t += dt;
            if (!o.fired && o.t >= DRAW_S) {
              o.fired = true;
              for (Ob &a : obs) {
                if (a.alive) continue;
                a = Ob();
                a.alive = a.active = true, a.type = ARROW, a.var = o.var, a.w = DIMS[ARROW].w, a.id = o.id;
                a.x = o.x - 2.0f, a.y = o.var == 0 ? 7.0f : 1.0f;
                ev |= EV_ARROW;
                break;
              }
            }
            if (o.fired) o.x += (speed + FLEE_ADD) * dt;  // it bolts to the right after the shot
          }
          break;
        case ARROW: o.x -= ARROW_V * dt; break;
        default: break;
      }
      bool gone = o.type == ARCHER ? o.x > dist + 400 : o.x + o.w < dist - 6.0f;
      if (gone) o.alive = false;
    }
  }

  void step(float dt)
  {
    if (dead) return;
    clk += dt, run_s += dt;
    speed = rmin(SPEED_MAX, SPEED0 + SPEED_GAIN * run_s);
    dist += speed * dt;
    if (ducking && finger != 2 && clk >= duck_min_until) ducking = false;
    if (cut_pending && finger != 1 && clk >= cut_at) {
      if (vy > V_CUT) vy = V_CUT;
      cut_pending = false;
    }
    updateObs(dt);
    float gh = support();
    if (on_ground && y > gh + 0.01f) on_ground = false, vy = 0;  // walked off the back of a ledge
    if (!on_ground) {
      float py = y;
      vy -= G * dt;
      y += vy * dt;
      if (vy <= 0 && py >= gh - 0.05f && y <= gh) {
        y = gh, vy = 0, on_ground = true, cut_pending = false;
        if (buffered && clk < buf_until) {  // a touch just before landing jumps on landing
          startJump();
          if (finger != 1) cut_pending = true, cut_at = clk + MIN_HOLD_S;
        }
        buffered = false;
      }
    }
    if (on_ground && gh == 0 && overPit()) {
      die(true);
      return;
    }
    int guard = 0;
    while (next_x < dist + view + 12 && guard++ < 6) spawnOne();
    uint32_t sc = (uint32_t)score();
    if (sc / 100 > last_mile) last_mile = sc / 100, ev |= EV_MILE;
    int z = zoneOf(score());
    if (z != last_zone) last_zone = z, ev |= EV_ZONE;
    if (hits()) die(false);
  }
};

}  // namespace runner
