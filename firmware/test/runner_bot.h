// The player used by the host test (runner_fair_test.cpp): sees an obstacle 0.25 s after it is on screen, then picks the
// action it asks for. Kept apart so a developer build can fly the real game with it for screenshots.
#pragma once
#include <map>
#include <vector>
#include "../src/boards/touch349/gadget/RunnerSim.h"

namespace runner {

enum Act { SHORT_HOP, FULL_HOP, DUCK, LEDGE_HOP };

struct Haz {
  uint32_t id;
  uint8_t type;
  double tc, d;  // seconds until the runner's centre meets its centre; time the hit boxes overlap
  Act act;
};

struct Bot {
  int mode;           // 0 idle, 1 right action, 2 jump only
  double react, jitter, latency;
  float vis_ahead;    // how far ahead it can see, cells
  uint32_t jit_rng = 12345;
  std::map<uint32_t, bool> acted;
  std::map<uint32_t, double> jit;
  bool holding = false;
  double rel_at = 0, ready_at = 0;

  double jr(uint32_t id)
  {
    auto it = jit.find(id);
    if (it != jit.end()) return it->second;
    jit_rng ^= jit_rng << 13, jit_rng ^= jit_rng >> 17, jit_rng ^= jit_rng << 5;
    double v = ((jit_rng & 0xFFFF) / 65536.0 * 2 - 1) * jitter;
    jit[id] = v;
    return v;
  }

  static Act actFor(const Ob &o)
  {
    switch (o.type) {
      case BEAM: case SWARM: case WASP: return DUCK;
      case ARROW: case ARCHER: return o.var == 0 ? DUCK : SHORT_HOP;
      case TALL: case HOPPER: return FULL_HOP;
      case LEDGE: return LEDGE_HOP;
      default: return SHORT_HOP;
    }
  }

  void collect(const Sim &s, std::vector<Haz> &out) const
  {
    double S = s.speed, rc = s.dist + RUN_CX, rx0 = s.dist + RUN_X0;
    for (const Ob &o : s.obs) {
      if (!o.alive) continue;
      Haz h;
      h.id = o.id, h.type = o.type, h.act = actFor(o);
      const Dim &dm = DIMS[o.type];
      double hitw = o.w - 2 * dm.inset, c = o.x + o.w * 0.5, ahead = c - rc;
      switch (o.type) {
        case PUDDLE: case CRATE: case TALL: case BIRD: case BEAM: case LEDGE:
          if (o.x - rx0 > vis_ahead - react * S) continue;
          h.tc = ahead / S, h.d = (hitw + 6) / S;
          break;
        case PIT:
          if (o.x - rx0 > vis_ahead - react * S) continue;
          h.tc = ahead / S, h.d = (o.w + 3.5) / S;
          break;
        case BUG: case SWARM: case HOPPER: case WASP:
          if (!o.active || o.t < react) continue;
          h.tc = ahead / (S + dm.v), h.d = (hitw + 6) / (S + dm.v);
          break;
        case ARCHER: {
          if (!o.active || o.t < react || o.fired) continue;
          double rem = DRAW_S - o.t, ac = o.x - 2 + 5;
          h.tc = rem + (ac - (rc + S * rem)) / (S + ARROW_V), h.d = 14.0 / (S + ARROW_V);
          break;
        }
        case ARROW:
          h.tc = (o.x + 5 - rc) / (S + ARROW_V), h.d = 14.0 / (S + ARROW_V);
          break;
        default: continue;
      }
      out.push_back(h);
    }
  }

  double startAt(const Haz &h) const
  {
    switch (h.act) {
      case SHORT_HOP: return h.tc - 0.28;
      case FULL_HOP: return h.tc - 0.37;
      case LEDGE_HOP: return h.tc - h.d / 2 - 0.13;
      default: return h.tc - h.d / 2 - 0.10;
    }
  }

  void act(Sim &s)
  {
    if (mode == 0) return;
    double now = s.clk;
    if (holding && now >= rel_at) {
      s.release();
      holding = false, ready_at = now + latency;
    }
    std::vector<Haz> hz;
    collect(s, hz);
    const Haz *best = nullptr;
    double best_ts = 1e9;
    for (const Haz &h : hz) {
      if (acted[h.id] || h.tc < -h.d) continue;
      double ts = startAt(h);
      if (ts < best_ts) best_ts = ts, best = &h;
    }
    if (!best) return;
    Act a = best->act;
    if (mode == 2 && a == DUCK) a = SHORT_HOP;
    double ts = best_ts;  // seconds from now until the action should start (negative = late)
    if (mode == 2 && best->act == DUCK) ts = best->tc - 0.28;
    if (ts > -jr(best->id)) return;  // jitter: > 0 acts late, < 0 early
    if (holding) {
      s.release();
      holding = false, ready_at = now + latency;
    }
    if (now < ready_at) return;
    if (a != DUCK && !s.on_ground) return;
    acted[best->id] = true;
    if (a == DUCK) {
      s.touchBottom();
      holding = true, rel_at = now + rmax(0.01f, (float)(best->tc + best->d / 2 + 0.03));
    } else {
      s.touchTop();
      holding = true, rel_at = now + (a == FULL_HOP ? 0.30 : 0.07);
    }
  }
};

}  // namespace runner
